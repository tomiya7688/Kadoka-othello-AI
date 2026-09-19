# AI Protocol

## Purpose

Every AI receives the same semantic state and returns a proposed move. Game Core remains authoritative.

## Input

`AIInput` is the native typed view of `kadoka.core_state.v1`.

It contains:

- current board
- side to move
- time information

It does not contain a legal-move list.

Native engines receive a zero-copy board reference. An engine that needs legal moves derives them internally from board + side to move.

Examples:

- Random AI derives the legal set and samples it.
- Evaluator AI derives legal candidates, builds evaluator features, then scores them.
- Obake Kadoka / Maru intentionally inspect empty squares without knowing legality.

## Output

`AIOutput` contains one proposed board position.

The proposal is passed to `Game::play()`. A move is not trusted simply because it came from a native or packaged AI.

## Invalid moves

If a proposal is illegal:

- board does not change
- side to move does not change
- ply does not change
- `GameEventType::InvalidMove` is emitted

Headless retries the same state up to `HeadlessConfig::max_invalid_attempts_per_turn`.

Character/UI behavior and logging can subscribe to the event without being part of Core legality logic.

## Package runtime

`AIPackage` binds an `IAIEngine`.

The old `PassThroughAdapter` / `DropLegalMovesAdapter` layer was removed because legal moves are no longer a Core input field.

Package manifests may still contain an old `adapter` key from historical packages; it is ignored as an unknown compatibility field and new manifests do not emit it.

## External/script transport

External and script backends receive canonical JSON state. Transport framing, process lifetime and timeout policy are Runtime concerns and do not change Core semantics.

## Performance rule

JSON is the API source of truth, not a requirement to serialize on the native hot path.

Native engines use `CoreStateView`; external transports serialize it with `core_state_to_json()`.

Optimized engines may derive bitboards, fixed buffers, SIMD layouts or tensors internally. Those representations must remain semantically equivalent to the canonical state and must not become alternate public Core APIs.
