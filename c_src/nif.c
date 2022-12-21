/*
%%
%% wirerlhair
%% erlang bindings for wirehair
%%
%% Copyright 2022 The University of Queensland
%%
%% Redistribution and use in source and binary forms, with or without
%% modification, are permitted provided that the following conditions
%% are met:
%%
%% 1. Redistributions of source code must retain the above copyright
%%    notice, this list of conditions and the following disclaimer.
%%
%% 2. Redistributions in binary form must reproduce the above copyright
%%    notice, this list of conditions and the following disclaimer in the
%%    documentation and/or other materials provided with the distribution.
%%
%% 3. Neither the name of the copyright holder nor the names of its
%%    contributors may be used to endorse or promote products derived from
%%    this software without specific prior written permission.
%%
%% THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
%% IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
%% OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
%% IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
%% INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
%% NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
%% DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
%% THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
%% (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
%% THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
%%
*/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "erl_nif.h"

#include <wirehair/wirehair.h>

static ErlNifResourceType *ctx_rsrc;

struct ctx {
	ErlNifMutex		*c_lock;
	struct WirehairCodec_t	*c_codec;
	ErlNifEnv		*c_env;
	ErlNifBinary		 c_msg;
	size_t			 c_msgsz;
	size_t			 c_blocksz;
};

static void
ctx_dtor(ErlNifEnv *env, void *arg)
{
	struct ctx *ctx = arg;
	enif_mutex_lock(ctx->c_lock);
	if (ctx->c_codec != NULL)
		wirehair_free(ctx->c_codec);
	ctx->c_codec = NULL;
	if (ctx->c_env != NULL)
		enif_free_env(ctx->c_env);
	ctx->c_env = NULL;
	enif_mutex_unlock(ctx->c_lock);
	enif_mutex_destroy(ctx->c_lock);
}

static ERL_NIF_TERM
encoder_create2(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
	struct ctx *ctx;
	uint ui;
	ERL_NIF_TERM term;

	if (argc != 2)
		return (enif_make_badarg(env));
	if (!enif_get_uint(env, argv[1], &ui))
		return (enif_make_badarg(env));

	ctx = enif_alloc_resource(ctx_rsrc, sizeof (struct ctx));
	bzero(ctx, sizeof (*ctx));
	ctx->c_lock = enif_mutex_create("wirehair_ctx");
	ctx->c_env = enif_alloc_env();
	ctx->c_blocksz = ui;

	term = enif_make_copy(ctx->c_env, argv[0]);

	if (!enif_inspect_iolist_as_binary(ctx->c_env, term, &ctx->c_msg)) {
		enif_release_resource(ctx);
		return (enif_make_badarg(env));
	}
	ctx->c_msgsz = ctx->c_msg.size;

	ctx->c_codec = wirehair_encoder_create(NULL, ctx->c_msg.data,
	    ctx->c_msgsz, ctx->c_blocksz);
	if (ctx->c_codec == NULL) {
		enif_release_resource(ctx);
		return (enif_make_tuple2(env,
		    enif_make_atom(env, "error"),
		    enif_make_atom(env, "wirehair_encoder_create")));
	}

	term = enif_make_resource(env, ctx);
	enif_release_resource(ctx);

	return (enif_make_tuple2(env, enif_make_atom(env, "ok"), term));
}

static ERL_NIF_TERM
wh_rc_to_err(ErlNifEnv *env, WirehairResult rc)
{
	switch (rc) {
	case Wirehair_Success:
		return (enif_make_atom(env, "ok"));
	case Wirehair_NeedMore:
		return (enif_make_atom(env, "more_data"));
	case Wirehair_InvalidInput:
		return (enif_make_tuple2(env, enif_make_atom(env, "error"),
		    enif_make_atom(env, "invalid_input")));
	case Wirehair_BadDenseSeed:
		return (enif_make_tuple2(env, enif_make_atom(env, "error"),
		    enif_make_atom(env, "bad_dense_seed")));
	case Wirehair_BadPeelSeed:
		return (enif_make_tuple2(env, enif_make_atom(env, "error"),
		    enif_make_atom(env, "bad_peel_seed")));
	case Wirehair_BadInput_SmallN:
		return (enif_make_tuple2(env, enif_make_atom(env, "error"),
		    enif_make_atom(env, "msg_too_small")));
	case Wirehair_BadInput_LargeN:
		return (enif_make_tuple2(env, enif_make_atom(env, "error"),
		    enif_make_atom(env, "too_many_blocks")));
	case Wirehair_ExtraInsufficient:
		return (enif_make_tuple2(env, enif_make_atom(env, "error"),
		    enif_make_atom(env, "extra_insufficient")));
	case Wirehair_Error:
		return (enif_make_tuple2(env, enif_make_atom(env, "error"),
		    enif_make_atom(env, "wirehair_generic")));
	case Wirehair_OOM:
		return (enif_make_tuple2(env, enif_make_atom(env, "error"),
		    enif_make_atom(env, "out_of_memory")));
	case Wirehair_UnsupportedPlatform:
		return (enif_make_tuple2(env, enif_make_atom(env, "error"),
		    enif_make_atom(env, "unsupported_platform")));
	default:
		return (enif_make_tuple2(env, enif_make_atom(env, "error"),
		    enif_make_atom(env, "unknown")));
	}
}

static ERL_NIF_TERM
encode2(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
	struct ctx *ctx;
	uint block_id;
	WirehairResult rc;
	uint32_t len;
	ErlNifBinary bin;
	ERL_NIF_TERM term;

	if (argc != 2)
		return (enif_make_badarg(env));
	if (!enif_get_resource(env, argv[0], ctx_rsrc, (void **)&ctx))
		return (enif_make_badarg(env));
	if (!enif_get_uint(env, argv[1], &block_id))
		return (enif_make_badarg(env));

	enif_mutex_lock(ctx->c_lock);
	if (!enif_alloc_binary(ctx->c_blocksz, &bin)) {
		enif_mutex_unlock(ctx->c_lock);
		return (enif_make_badarg(env));
	}
	rc = wirehair_encode(ctx->c_codec, block_id, bin.data, bin.size, &len);
	enif_mutex_unlock(ctx->c_lock);

	if (rc != Wirehair_Success)
		return (wh_rc_to_err(env, rc));

	term = enif_make_binary(env, &bin);
	if (len != bin.size)
		term = enif_make_sub_binary(env, term, 0, len);
	return (enif_make_tuple2(env, enif_make_atom(env, "ok"), term));

}

static ERL_NIF_TERM
decoder_create2(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
	struct ctx *ctx;
	uint msgsz, blocksz;
	ERL_NIF_TERM term;

	if (argc != 2)
		return (enif_make_badarg(env));
	if (!enif_get_uint(env, argv[0], &msgsz))
		return (enif_make_badarg(env));
	if (!enif_get_uint(env, argv[1], &blocksz))
		return (enif_make_badarg(env));

	ctx = enif_alloc_resource(ctx_rsrc, sizeof (struct ctx));
	bzero(ctx, sizeof (*ctx));
	ctx->c_lock = enif_mutex_create("wirehair_ctx");
	ctx->c_env = enif_alloc_env();
	ctx->c_blocksz = blocksz;
	ctx->c_msgsz = msgsz;

	ctx->c_codec = wirehair_decoder_create(NULL, ctx->c_msgsz,
	    ctx->c_blocksz);
	if (ctx->c_codec == NULL) {
		enif_release_resource(ctx);
		return (enif_make_tuple2(env,
		    enif_make_atom(env, "error"),
		    enif_make_atom(env, "wirehair_decoder_create")));
	}

	term = enif_make_resource(env, ctx);
	enif_release_resource(ctx);

	return (enif_make_tuple2(env, enif_make_atom(env, "ok"), term));
}

static ERL_NIF_TERM
decode3(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
	struct ctx *ctx;
	ErlNifBinary bin;
	uint block_id;
	WirehairResult rc;

	if (argc != 3)
		return (enif_make_badarg(env));
	if (!enif_get_resource(env, argv[0], ctx_rsrc, (void **)&ctx))
		return (enif_make_badarg(env));
	if (!enif_get_uint(env, argv[1], &block_id))
		return (enif_make_badarg(env));
	if (!enif_inspect_iolist_as_binary(env, argv[2], &bin))
		return (enif_make_badarg(env));

	enif_mutex_lock(ctx->c_lock);
	rc = wirehair_decode(ctx->c_codec, block_id, bin.data, bin.size);
	enif_mutex_unlock(ctx->c_lock);

	return (wh_rc_to_err(env, rc));
}

static ERL_NIF_TERM
recover1(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
	struct ctx *ctx;
	ErlNifBinary bin;
	WirehairResult rc;
	ERL_NIF_TERM term;

	if (argc != 1)
		return (enif_make_badarg(env));
	if (!enif_get_resource(env, argv[0], ctx_rsrc, (void **)&ctx))
		return (enif_make_badarg(env));

	enif_mutex_lock(ctx->c_lock);
	if (!enif_alloc_binary(ctx->c_msgsz, &bin)) {
		enif_mutex_unlock(ctx->c_lock);
		return (enif_make_badarg(env));
	}
	rc = wirehair_recover(ctx->c_codec, bin.data, bin.size);
	enif_mutex_unlock(ctx->c_lock);

	if (rc != Wirehair_Success)
		return (wh_rc_to_err(env, rc));

	term = enif_make_binary(env, &bin);
	return (enif_make_tuple2(env, enif_make_atom(env, "ok"), term));
}

static int
nif_load(ErlNifEnv *env, void **priv_data, ERL_NIF_TERM load_info)
{
	ctx_rsrc = enif_open_resource_type(env, NULL, "wirehair_ctx",
	    ctx_dtor, ERL_NIF_RT_CREATE | ERL_NIF_RT_TAKEOVER, NULL);
	wirehair_init();
	return (0);
}

static void
nif_unload(ErlNifEnv *env, void *priv_data)
{
}

static ErlNifFunc nif_funcs[] = {
	{ "encoder_create",	2, encoder_create2 },
	{ "encode",		2, encode2 },
	{ "decoder_create",	2, decoder_create2 },
	{ "decode",		3, decode3 },
	{ "recover",		1, recover1 },
};

ERL_NIF_INIT(wirerlhair_nif, nif_funcs, nif_load, NULL, NULL, nif_unload);
