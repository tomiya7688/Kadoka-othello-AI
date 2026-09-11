# AI Protocol

## Purpose

All AI implementations use the same logical input and output contract.
The game core does not depend on AI implementation details.

## Input

Every AI request is created from:

- current board
- legal move list

The runtime representation is `AIInput`.
The board and legal move list are passed by reference/pointer so the normal native path does not copy them.

## Output

Every AI returns one move through `AIOutput`.

The game core is always the authority for legality. An AI output is never trusted as legal merely because a legal move list was supplied.

## Adapter layer

Adapters transform protocol input before it reaches an AI implementation.

### PassThroughAdapter

Passes both board and legal moves to the AI.
This is the default adapter for normal AI implementations.

### DropLegalMovesAdapter

Passes the board but removes the legal move list.
This is intended for AI implementations such as Obake Kadoka and Obake Maru that do not know legal moves.

The AI implementation itself therefore does not need special code to discard legal moves.

## Invalid moves

The game core remains responsible for validating the returned move.
The headless runner retries the same turn when an AI returns an invalid move.
Only successful legal moves are added to normal game history and dataset snapshots.

`HeadlessConfig::max_invalid_attempts_per_turn` prevents an AI from creating an infinite retry loop.

GUI implementations may separately visualize invalid attempts, character reactions, warnings, or other presentation effects.

## AI package runtime

`AIPackage` currently binds:

- an `IAIEngine`
- an `IAIAdapter`

The runner only knows this pair and does not need to know whether the AI is built in, loaded from a library, proxied to another process, or connected through another adapter.

Future loaders can therefore support native libraries, executables, Python, network services, or legacy AI without changing the game core protocol.

## Performance rule

Fast native AIs should remain on the zero-copy native path where possible.
Expensive serialization such as JSON should be treated as an adapter/transport concern, not as the internal core interface.

This allows optimized AI implementations to use bitboards, SIMD, lookup tables, fixed-size buffers, multithreading, or other implementation-specific techniques without changing the protocol.
