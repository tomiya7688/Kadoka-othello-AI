# Sibling Project Alignment

Kadoka Othello AI, Kadoka Shougi AI and Kadoka Tetris AI are sibling projects. They should reuse proven development methods without forcing game-specific implementation details into a shared lowest-common-denominator design.

Sibling repositories:

- `tomiya7688/Kadoka-othello-AI`
- `tomiya7688/Kadoka-shougi-ai`
- `tomiya7688/kadoka_tetris_ai`

## Shared engineering invariants

### Authoritative core

AI output is a proposal, not authoritative game state.

The game/rules core validates the proposal and owns the canonical state transition. Character AIs may intentionally propose poor or illegal actions, but they must never bypass rule validation.

### Runtime stays small

Match-time/runtime code contains only work needed to execute a game or inference.

Training, model creation, conversion, rich analysis, reports and large dataset processing belong to Creator/tooling layers. Runtime must not depend upward on them.

### Correctness before strength

Rule correctness and deterministic reproduction take priority over search or model strength. A faster/stronger AI is not useful if it can corrupt canonical game state or cannot be reproduced for debugging.

### Deterministic seams

Random behavior should accept an explicit seed when practical. Benchmarks and AI comparisons should use fixed inputs/seeds and bounded runtimes.

### Semantic action boundary

Humans and AIs should ultimately reach the same authoritative game-action validation path. UI/protocol adapters translate input; they do not become a second rules engine.

### Hot-path flexibility

Responsibility separation is the default, but move generation, search, evaluation, rollout and other measured hot paths may use localized optimizations when abstraction overhead is meaningful. Performance exceptions must not break layer direction.

## Shared AI backend vocabulary

The sibling projects use the same conceptual execution-backend names even though their game-state/action types are different:

- `native`
- `dynamic_library`
- `external_process`
- `script`
- `network`

The backend only proposes an action/result. The authoritative game core still validates and applies it.

Othello currently exposes `python` as a package-interface compatibility name for a Python script/process path. It maps conceptually to the shared `script` category; existing manifests do not need to be rewritten immediately.

External/process/script transports must stay outside Core. High-throughput paths should prefer persistent sessions or in-process/native execution rather than per-move process startup.

## Methods adopted from siblings

From Kadoka Shougi AI:

- correctness-before-strength
- engine result is non-authoritative until core validation
- C++ `.clang-format` / `.clang-tidy` baseline
- narrow engine/runtime/core dependency direction
- common `AIBackend` runner vocabulary for native/process/script/network adapters

From Kadoka Tetris AI:

- deterministic seed/input discipline
- headless-first tests
- source CI and Windows build-artifact validation as separate evidence
- one-command build expectation
- do not claim GUI/artifact verification that was not actually performed

From Kadoka Othello AI:

- Runtime / Creator separation
- context-reduction entrypoint and task routing
- compact mechanically verifiable rule checker
- model/package assets separate from execution tooling

## Cross-project adoption rule

Before adding a new project-wide workflow, CI pattern, checker, model/package convention or major runtime boundary, briefly inspect the sibling implementations first.

Adopt a sibling technique when:

1. the same problem exists here;
2. it preserves this game's semantics and performance needs;
3. maintenance cost is reasonable;
4. it can be validated locally in this repository.

Do not copy blindly. Language-specific or game-specific details remain local.

## CI baseline

The common direction is:

```text
static/policy checks
  -> source build / unit tests
  -> deterministic or headless smoke when relevant
  -> platform/artifact build when distribution is affected
```

Othello currently uses Linux CMake build/test CI and Windows build-artifact CI. A Windows artifact is a developer build until a real portable distribution boundary is explicitly defined and smoke-tested.

## Review trigger

Revisit sibling repositories when changing any of these:

- CI/build/release workflow
- AI/common engine protocol
- Runtime/Creator or runtime/tooling boundary
- deterministic benchmark methodology
- package/model format
- dependency checker / coding-policy automation
- distribution/artifact validation

The goal is shared learning, not forced identical code.
