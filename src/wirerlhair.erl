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

%% @doc Fountain code-based erasure encoder for error correction.
%%
%% Wirehair produces a stream of error correction blocks from a data source
%% using an erasure code. When enough of these blocks are received, the
%% original data can be recovered.
%%
%% As compared to other similar libraries, an unlimited number of error
%% correction blocks can be produced, and much larger block counts are
%% supported. Furthermore, it gets slower as `O(N)' in the amount of input
%% data rather than `O(N Log N)' like the Leopard block code or `O(N^2)' like
%% the Fecal fountain code, so it is well-suited for large data.
%%
%% This is not an ideal MDS code, so sometimes it will fail to recover `N'
%% original data packets from N symbol packets. It may take `N + 1' or `N + 2'
%% or more. On average it takes about `N + 0.02' packets to recover. Overall
%% the overhead from the code inefficiency is low, compared to LDPC and many
%% other fountain codes.
%%
%% [https://github.com/catid/wirehair]
-module(wirerlhair).

-export([
    new_encoder/2,
    encode/2,
    new_decoder/2,
    decode/3,
    recover/1
    ]).

-export_type([
    encoder/0,
    decoder/0
    ]).

-opaque encoder() :: wirerlhair_nif:ctx().
-opaque decoder() :: wirerlhair_nif:ctx().

-type msg() :: iolist().
%% A message being encoded or decoded.

-type bytes() :: integer().

-type block_size() :: bytes().
%% The size of each block in bytes.

-type msg_size() :: bytes().
%% The size of a message in bytes.

-type block_id() :: integer().
%% The ID number of a block, given as a zero-based positive integer.

-type reason() :: invalid_input | bad_dense_seed | bad_peel_seed |
    msg_too_small | too_many_blocks | extra_insufficient | wirehair_generic |
    out_of_memory | unsupported_platform | unknown | wirehair_encoder_create |
    wirehair_decoder_create.
%% Possible failure codes from Wirehair.

-type error() :: {error, reason()}.

%% @doc Create a new Wirehair encoder.
%%
%% Creates a new Wirehair encoder instance for a specific message. The block
%% size must be chosen such that at least 2 blocks are used for the message
%% and at most 64,000. The most efficient operation is achieved at around
%% 1000 blocks per message.
%%
%% @see msg()
%% @see block_size()
-spec new_encoder(msg(), block_size()) -> {ok, encoder()} | error().
new_encoder(Msg, BlockSize) ->
    wirerlhair_nif:encoder_create(Msg, BlockSize).

%% @doc Encode a block.
%%
%% The first `BlockId < N' blocks are the same as the input data.
%%
%% The `BlockId >= N' blocks are error-correction data, generated
%% on demand.
%%
%% You can generate as many parity blocks as needed by repeatedly calling this
%% function with increasing values of <code>BlockId</code>.
%%
%% @see new_encoder/2
-spec encode(encoder(), block_id()) -> {ok, binary()} | error().
encode(Encoder, BlockId) ->
    wirerlhair_nif:encode(Encoder, BlockId).

%% @doc Create a new Wirehair decoder.
%%
%% The final size of the message must be known in advance, as well as the
%% block size.
%%
%% @see msg_size()
%% @see block_size()
-spec new_decoder(msg_size(), block_size()) -> {ok, decoder()} | error().
new_decoder(MsgSize, BlockSize) ->
    wirerlhair_nif:decoder_create(MsgSize, BlockSize).

%% @doc Decode a block.
%%
%% The atom <code>more_data</code> will be returned until sufficient blocks
%% have been received to recover the message.
%%
%% Once this function returns <code>ok</code>, you can use the {@link recover/1}
%% function to retrieve the message itself.
%%
%% @see new_decoder/2
%% @see block_id()
-spec decode(decoder(), block_id(), iolist()) -> more_data | ok | {error, term()}.
decode(Decoder, BlockId, Block) ->
    wirerlhair_nif:decode(Decoder, BlockId, Block).

%% @doc Retrieve the decoded message.
%%
%% This function can only be called once {@link decode/3} has returned
%% <code>ok</code>.
%%
%% @see new_decoder/2
%% @see decode/3
%% @see msg()
-spec recover(decoder()) -> {ok, msg()} | {error, term()}.
recover(Decoder) ->
    wirerlhair_nif:recover(Decoder).
