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

%% @private
-module(wirerlhair_nif).

-export([
    encoder_create/2,
    encode/2,
    decoder_create/2,
    decode/3,
    recover/1
    ]).

-export_type([
    ctx/0
    ]).

-on_load(init/0).

-opaque ctx() :: reference().

try_paths([Last], BaseName) ->
    filename:join([Last, BaseName]);
try_paths([Path | Next], BaseName) ->
    case filelib:is_dir(Path) of
        true ->
            WCard = filename:join([Path, "{lib,}" ++ BaseName ++ ".*"]),
            case filelib:wildcard(WCard) of
                [] -> try_paths(Next, BaseName);
                _ -> filename:join([Path, BaseName])
            end;
        false -> try_paths(Next, BaseName)
    end.

init() ->
    Paths0 = [
        filename:join(["..", lib, wirerlhair, priv]),
        filename:join(["..", priv]),
        filename:join([priv])
    ],
    Paths1 = case code:priv_dir(wirerlhair) of
        {error, bad_name} -> Paths0;
        Dir -> [Dir | Paths0]
    end,
    SoName = try_paths(Paths1, "wirerlhair_nif"),
    erlang:load_nif(SoName, 0).

-type msg() :: iolist().
-type bytes() :: integer().
-type block_size() :: bytes().
-type msg_size() :: bytes().
-type block_id() :: integer().
-type block() :: binary().

-spec encoder_create(msg(), block_size()) -> {ok, ctx()} | {error, term()}.
encoder_create(_Msg, _BlockSize) -> error(no_nif).

-spec encode(ctx(), block_id()) -> {ok, block()} | {error, term()}.
encode(_Ctx, _BlockId) -> error(no_nif).

-spec decoder_create(msg_size(), block_size()) -> {ok, ctx()} | {error, term()}.
decoder_create(_MsgSize, _BlockSize) -> error(no_nif).

-spec decode(ctx(), block_id(), block()) -> more_data | ok | {error, term()}.
decode(_Ctx, _BlockId, _Block) -> error(no_nif).

-spec recover(ctx()) -> {ok, msg()} | {error, term()}.
recover(_Ctx) -> error(no_nif).
