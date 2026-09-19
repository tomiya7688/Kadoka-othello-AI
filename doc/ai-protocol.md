# AI Protocol

## Purpose

All AI implementations use the same logical state/action boundary.
The game core does not depend on AI implementation details.

## Canonical input

The canonical Core/API exchange format is JSON.

An AI decision request contains only the game-observable state required by the project contract:

- board state
- side to move
- time information

Legal moves are not part of the canonical AI input contract.

An AI that needs legal moves derives them from the received board state, or uses Runtime-internal rule helpers. This keeps normal AIs, experimental AIs and character AIs on the same state boundary.

## Output

Every AI proposes one move.

The game core is always authoritative. An AI proposal does not change the game state until the core validates and applies it.

## Invalid moves

When a proposed move is illegal:

- canonical board state is unchanged;
- side to move is unchanged;
- an invalid-move event is emitted.

The caller may react to that event by retrying the same AI turn, showing a GUI effect, updating character-local memory, logging diagnostics, or terminating after a configured retry limit.

Only successfully applied legal moves belong to normal game history / game-result datasets.

## Runtime representation

JSON is the canonical interchange format, not a requirement to serialize and parse text inside every native hot-path call.

Runtime may use a typed/native view equivalent to the canonical JSON state after the boundary has been decoded. Native implementations may also use bitboards, SIMD, lookup tables, fixed-size buffers, multithreading, or other implementation-specific structures internally.

Those are implementation details and must not redefine the Core/API contract.

## Adapter layer

Adapters may translate transports, legacy package formats, or model-specific representations.

They must not invent a second authoritative rules layer.

Legacy adapters that inject or remove legal-move lists are compatibility mechanisms only; legal moves are not part of the canonical Core/API input.

## AI package runtime

`AIPackage` binds an AI implementation to the Runtime-facing package/adapter layer.

The runner does not need to know whether the implementation is built in, loaded from a library, proxied to another process, or connected through another transport.

External process, script, dynamic-library and network transports all preserve the same logical state/action contract.

## Performance rule

The canonical API is JSON, while high-throughput native execution may avoid repeated JSON serialization after decoding the state boundary.

Dataset compression, binary encodings and training tensors belong to AI Creator / Dataset Tooling. They are not alternate Core API formats.
