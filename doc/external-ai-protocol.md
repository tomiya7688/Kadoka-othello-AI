# External AI Protocol v1

AI Creator can import Python scripts and external executables as Kadoka AI packages.

## Import

```bat
kadoka_othello_ai_creator.exe import python my_ai.py packages\my_ai my.ai "My AI"
```

or:

```bat
kadoka_othello_ai_creator.exe import external_process my_ai.exe packages\my_ai my.ai "My AI"
```

The source file is copied into the package directory and a `manifest.json` is generated.

## Canonical request semantics

External AI transports carry the same canonical state as the Core API:

- board information
- side to move
- time information

JSON is the canonical interchange representation.

A transport may frame that JSON for stdin/stdout, files, IPC or network use, but it must not change the meaning of the payload.

Legal moves are not part of the canonical request. External AIs that need them derive them from the board state.

## Response

The required response is a proposed move.

Optional development output may include candidate values or diagnostics, but these belong to Runtime/Creator inspection and are not authoritative game state.

The game runtime consumes the move proposal and submits it to the Core.

## Invalid moves

The Core validates every proposed move.

If a move is illegal, the canonical state remains unchanged and the Core emits an invalid-move event. The transport/session layer may then request another move for the same unchanged state according to its retry policy.

## Performance note

External-process transport is intended for compatibility, prototyping and model development.

High-speed engines should prefer persistent or in-process/native execution. A native fast path may use a typed view after the canonical JSON boundary has been decoded, but the public/Core interchange contract remains JSON.

Binary or compressed training formats belong to AI Creator / Dataset Tooling, not to this Core protocol.
