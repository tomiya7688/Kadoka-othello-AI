# Current State

This file is a compact index of what is implemented now. It is not a replacement for source files or detailed specifications.

## Implemented

### Othello Core / Runtime

- Variable-size board implementation supporting 6x6 / 8x8 / 10x10.
- Rules, legal move generation, game state and headless execution.
- Common AI input/output contract and adapters.
- AI package manifest loading.
- Runtime and Creator Support are separate CMake targets.
- Headless execution links Runtime only.

### Model / Package

- `manifest.json` describes the AI package.
- `model.json` is a root model descriptor and can reference multiple assets.
- Model assets can represent evaluator, behavior, network/search/opening data or other model-owned resources.
- `ModelRootDescriptor` / `ModelAssetDescriptor` resolve model assets.

### Script Evaluator

- `kadoka.script_evaluator.v1` is a Runtime feature.
- `python_process` runtime is implemented.
- `native_process` runtime is implemented.
- Batch input is used so one board's candidate set can be evaluated in one process invocation.
- `wasm` is reserved but not yet executable.
- Native in-process evaluation is not yet implemented.

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
- `tools/next_issue.py` creates a compact next-task capsule.
- `tools/kadoka_rule_checker/` checks mechanically verifiable project rules.
- Rule checker is invoked from the normal CMake build before Runtime compilation.

## Open / Pending Work

- Native in-process Script Evaluator for removing process/temp-file overhead.
- WASM Script Evaluator runtime.
- CI enforcement of Runtime / Creator dependency direction.
- Full Release build / test / benchmark verification of the current AI foundation.
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
