# AI Runtime / AI Creator Boundary

## Purpose

Kadoka Othello AI separates the AI execution path from the AI creation/development path.

This is primarily a performance rule.

The game and headless self-play runner may execute millions of inferences. Development-only analysis, conversion, import, rich diagnostics and dataset tooling must not become dependencies of that hot path.

## Two products

### AI Runtime

The Runtime exists to load a packaged model and return a move as cheaply as possible.

Runtime responsibilities:

- load `manifest.json`
- load the root `model.json`
- resolve required model assets
- adapt game input to the model input
- run native/model/script inference
- preserve model-local state such as character memory
- return the selected move
- optionally expose lightweight inspection when explicitly requested
- support headless self-play

Runtime must not depend on:

- AI Creator batch analysis
- dataset conversion
- import/package creation tools
- format conversion tables
- training/tuning orchestration
- rich report generation
- Creator-only benchmark orchestration

### AI Creator

AI Creator is a development application built on top of the Runtime.

Creator responsibilities:

- create/package models
- import external AIs
- inspect candidate values and diagnostics
- compare multiple AIs
- batch-analyze positions
- benchmark models
- convert model/dataset formats
- validate package assets
- tune parameters
- export datasets and analysis records

Creator is allowed to depend on Runtime.
Runtime must never depend on Creator.

## Build dependency rule

Current CMake targets enforce the direction:

```text
kadoka_othello_runtime
        ^
        |
kadoka_othello_creator_support
        ^
        |
kadoka_othello_ai_creator
```

Game/headless execution links only:

```text
kadoka_othello_runtime
```

The Creator links:

```text
kadoka_othello_creator_support
+ kadoka_othello_runtime
```

This is intentional. Do not merge these targets again for convenience.

## Shared contract

Runtime and Creator share the model/package contracts, not Creator implementation code.

Shared concepts include:

- `AIPackageManifest`
- `ModelRootDescriptor`
- `ModelAssetDescriptor`
- `IAIEngine`
- `IAIAdapter`
- `AIInput`
- `AIOutput`
- optional `AIInspection`
- `ScriptEvaluator`

The game contract remains:

```text
input:
  board
  legal moves

output:
  selected move
```

Adapters may remove information before the model receives it. For example Obake Kadoka and Obake Maru use `drop_legal_moves`.

## Hot path rule

Normal game execution should use `think()`.

```text
Game
  -> Runtime package
  -> adapter
  -> model engine
  -> think()
  -> move
```

Development tools may use `inspect()`.

```text
AI Creator
  -> Runtime package
  -> adapter
  -> model engine
  -> inspect()
  -> move + candidates + diagnostics
```

Do not make `think()` internally call Creator analysis code.
Do not generate dataset records, strings, reports or conversion objects on the normal Runtime path unless the model itself requires them for inference.

## Script evaluators

`kadoka.script_evaluator.v1` is a Runtime feature because a model may require a script to perform inference.

This does not make the script evaluator a Creator feature.

Runtime may execute a model-owned script because the script is part of the model definition.
Creator may inspect/test that same evaluator through Runtime APIs.

The distinction is:

```text
model-required computation -> Runtime
model-development tooling   -> Creator
```

For `python_process`, candidates should be batch-evaluated in one process invocation whenever possible. Per-candidate process launches are prohibited on performance grounds.

## Dataset generation

Headless self-play belongs to Runtime because it is repeated game execution.

Dataset serialization, transformation, relabeling, conversion and analysis belong to Creator/tooling.

Recommended flow:

```text
Runtime self-play
  -> compact game/result stream
  -> Creator / dataset tools
  -> analysis / conversion / training data
```

The Runtime may emit minimal raw records when explicitly enabled, but it should not own heavy dataset processing.

## Dependency review checklist

Before adding a new component, ask:

1. Is this required to choose a move during a game?
   - yes: Runtime candidate
   - no: probably Creator/tooling

2. Does it create reports, datasets, conversions or comparisons?
   - Creator/tooling

3. Is it a model asset interpreter needed by inference?
   - Runtime

4. Is it only useful while developing/tuning a model?
   - Creator/tooling

5. Would adding it increase allocations, serialization, process launches or dependency size on every inference?
   - keep it out of Runtime unless strictly required

## Design invariant

**AI Creator creates, analyzes and tunes AIs. AI Runtime executes AIs.**

Keeping this boundary strict is a performance requirement, not only a code-organization preference.
