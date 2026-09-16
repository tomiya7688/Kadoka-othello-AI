# Current State

This file is a compact index of what is implemented now. It is not a replacement for source files or detailed specifications.

## Implemented

### Othello Core / Runtime

- Variable-size board implementation supporting 6x6 / 8x8 / 10x10.
- Rules, legal move generation, game state and headless execution.
- Common AI input/output contract and adapters.
- AI package manifest loading.
- Runtime and Creator Support are separate CMake targets.
- `KADOKA_BUILD_AI_CREATOR=OFF` provides a Runtime-only CMake configuration; Headless/core tests build without Creator Support.
- Headless execution links Runtime only.
- Canonical state changes remain owned by `Game` / rules; AI output is non-authoritative until applied successfully.

### Model / Package

- `manifest.json` describes the AI package.
- `model.json` is a root model descriptor and can reference multiple assets.
- Model assets can represent evaluator, behavior, network/search/opening data or other model-owned resources.
- `ModelRootDescriptor` / `ModelAssetDescriptor` resolve model assets.

### Script Evaluator

- `kadoka.script_evaluator.v1` is a Runtime feature.
- `python_process` runtime is implemented.
- `native_process` runtime is implemented.
- `native_in_process` runtime is implemented as a stable C ABI DLL/SO path.
- Native in-process output is returned through host-owned callbacks, so C++ containers do not cross the module ABI boundary.
- Batch input is used for all runtimes.
- `wasm` is reserved but not yet executable.
- CMake builds a native in-process sample module and generates a runnable evaluator config in the build tree.
- CTest includes a native in-process evaluator smoke test.
- The evaluator probe supports repeated calls for simple runtime-latency comparison.

### Character AIs

#### Obake Kadoka

- Does not receive legal moves.
- Uses Kadoka-specific local board features rather than normal Othello strategy.
- Uses weighted randomness.
- Keeps short attempt memory including inferred rejected moves.
- Evaluator and behavior parameters are model assets and are intended to be tunable.

#### Obake Maru

- Does not receive legal moves.
- Uses a much weaker local-interest evaluator and high randomness.
- Remembers only the immediately previous rejected attempt.

### AI Creator / Tooling

- Analyze, batch analyze, compare and benchmark paths exist.
- Package import and data/model conversion support exist.
- Root `AI_CONTEXT.md` is the compact AI-assisted-development entrypoint.
- `doc/context-routing.md` maps task categories to source/tests/docs/validation.
- `tools/context_route.py` prints one route without loading unrelated documentation.
- `tools/next_issue.py` creates a compact Goal / Required / Acceptance task capsule and routes it to the relevant subsystem.
- `tools/kadoka_rule_checker/` checks mechanically verifiable project rules.
- Rule checker is invoked from the normal CMake build before Runtime compilation.

### Quality / CI

- `.clang-format` / `.clang-tidy` baseline is aligned with Kadoka Shougi AI, using this project's C++17 level.
- Linux GitHub Actions builds, runs CTest, executes a fixed-seed headless smoke, and separately verifies a Creator-disabled Runtime-only configuration.
- Windows GitHub Actions runs `build.bat`, executes the same bounded headless smoke and uploads developer build artifacts.
- The 2026-09-16 Linux and Windows workflows both passed after CI exposed and we fixed an AI Creator format-listing compile mismatch.
- Sibling-project cross-adoption policy is documented in `doc/sibling-project-alignment.md`.

## Open / Pending Work

- Connect a production AI model that actually consumes `native_in_process` Script Evaluator assets during Headless/Creator inference; the Runtime evaluator path and smoke module exist, but no current character AI requires this asset yet.
- Complete comparative performance measurements for `python_process` / `native_process` / `native_in_process` under identical inputs.
- WASM Script Evaluator runtime.
- Define a real portable distribution boundary before calling Windows developer artifacts a release/distribution package.
- Additional rule-checker coverage where rules can be verified without noisy false positives.
- Broader AI families planned in project Issues/specs are not implied to be implemented by this summary.

## Performance-sensitive Boundaries

The following areas should not gain unnecessary allocations, serialization, process launches or rich diagnostics on the normal path:

- `think()` inference path
- move generation
- board evaluation
- search / Monte Carlo loops
- headless self-play
- dataset-generation execution loop

Creator-only analysis and conversion must stay outside those hot paths.

## Validation Baseline

For broad/shared changes:

```text
python tools/kadoka_rule_checker/script/kadoka_rule_checker.py .
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

For focused changes, use `doc/context-routing.md` to run targeted evidence first, then broaden only when the changed contract is shared/public or impact is uncertain.
