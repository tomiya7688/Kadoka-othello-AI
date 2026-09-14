# Context Routing

Use this map to choose the smallest useful working set for AI-assisted changes.
Do not read every listed file automatically; start with the matching route and expand only when a contract boundary requires it.

## Standard Order

```text
task / changed area
  -> target source
  -> matching tests or smallest smoke
  -> direct contracts
  -> detailed docs if needed
  -> broader validation only for shared/public/uncertain impact
```

## Routes

### core

Use for board, rules, game state, passes, end conditions, 6x6/8x8/10x10 behavior.

- Source: `src/board.cpp`, `src/rules.cpp`, `src/game.cpp`, `src/state.cpp`, matching headers under `src/kadoka_othello/`
- Tests: `src/tests/core_tests.cpp`
- Docs: `doc/architecture.md`, `doc/data-format.md` only when state contract changes
- Validation: rule checker -> core tests -> Release build when public/core interfaces change

### runtime

Use for AI invocation, adapters, package execution and Runtime-only dependency behavior.

- Source: `src/ai.cpp`, `src/package_loader.cpp`, `src/headless.cpp`, matching headers
- Tests: `src/tests/core_tests.cpp` plus bounded headless smoke
- Docs: `doc/runtime-creator-boundary.md`, `doc/ai-protocol.md`, `doc/architecture.md`
- Validation: rule checker is required; fixed-seed headless smoke for behavior changes

### creator

Use for AI Creator analyze/compare/benchmark/import workflows.

- Source: `src/ai_creator.cpp`, `src/package_import.cpp`, `src/tools/ai_creator_main.cpp`, matching headers
- Tests: targeted CLI/smoke where available; broader build because Creator links Runtime
- Docs: `doc/ai-creator.md`, `doc/runtime-creator-boundary.md`
- Validation: rule checker -> Creator build -> command-specific smoke

### model-package

Use for model root descriptors, package manifests, asset resolution and package compatibility.

- Source: `src/model_descriptor.cpp`, `src/package.cpp`, `src/package_loader.cpp`, model/package headers
- Tests: package/model loading smoke; `src/tests/core_tests.cpp` where contract coverage exists
- Docs: `doc/model-format.md`, `doc/ai-package.md`
- Validation: representative real package must load; do not rely on parser source review only

### script-evaluator

Use for `kadoka.script_evaluator.v1`, Python/native/WASM/in-process evaluator work.

- Source: `src/script_evaluator.cpp`, `src/kadoka_othello/script_evaluator.hpp`, `src/tools/script_evaluator_probe.cpp`
- Samples: `samples/script_evaluator/`
- Docs: `doc/script-evaluator-runtime.md`, `doc/model-format.md`, `doc/runtime-creator-boundary.md`
- Validation: evaluator probe with bounded batch; compare runtimes using identical feature input for compatibility changes
- Performance: process count, temp-file I/O and per-batch latency are relevant evidence

### obake-kadoka

Use for Kadoka character AI behavior or tuning.

- Source: `src/obake_kadoka.cpp`, `src/obake_kadoka_evaluator.cpp`, `src/obake_kadoka_model.cpp`, matching headers
- Model assets: `src/packages/obake_kadoka/`
- Tests: fixed seed, bounded headless games; inspect candidate diagnostics only when needed
- Docs: `doc/obake-kadoka.md`
- Invariant: do not add conventional Othello strategy knowledge unless explicitly requested

### obake-maru

Use for Maru character AI behavior or tuning.

- Source: `src/obake_maru.cpp`, `src/obake_maru_model.cpp`, matching headers
- Model assets: `src/packages/obake_maru/`
- Tests: fixed seed, bounded headless games
- Docs: `doc/obake-maru.md`
- Invariant: keep Maru weaker and extremely short-memory by design

### data-conversion

Use for JSONL/model records, dataset conversion and format mappings.

- Source: `src/model_data.cpp`, `src/conversion.cpp`, matching headers
- Docs: `doc/model-data.md`, `doc/data-format.md`, `doc/data-conversion.md`
- Validation: use a disposable output location and representative records; inspect compact output/diff rather than large datasets
- Boundary: transformation/analysis belongs to Creator/tooling, not the Runtime inference hot path

### protocol

Use for native AI contract, external process/Python protocol or adapter contract changes.

- Source: `src/ai.cpp`, `src/package_loader.cpp`, protocol/package headers
- Docs: `doc/ai-protocol.md`, `doc/external-ai-protocol.md`, `doc/ai-package.md`
- Validation: producer + consumer smoke; existing character adapters must retain intended legal-move visibility
- Fallback: because protocol is shared/public, broaden validation to Creator + Headless

### build-policy

Use for CMake, build scripts, coding rules and checker behavior.

- Source: `CMakeLists.txt`, `build.bat`, `tools/kadoka_rule_checker/`
- Docs: `doc/build.md`, `doc/coding-rules.md`, `doc/runtime-creator-boundary.md`
- Validation: run checker directly, then normal Release build; checker success with zero scanned targets is not valid evidence

## Broadening Rules

Broaden beyond the selected route when any of these is true:

- shared/public AI or model contract changed
- Runtime/Creator boundary changed
- core board/rules API changed
- CMake target/dependency graph changed
- package/distribution contents changed
- affected dependency is unclear

Otherwise prefer targeted evidence and stop when acceptance is satisfied.

## Ignore Normally

- `build/`
- `.codex/` except current task capsule
- generated datasets / large JSONL
- old logs / temp evaluator files
- unrelated character models
- unrelated Issues and historical diffs

## Compact Completion Report

Record only:

```text
Changed:
- files / responsibility

Impact:
- behavior / compatibility / performance

Validated:
- commands or targeted evidence + result

Unverified:
- only remaining relevant gaps
```
