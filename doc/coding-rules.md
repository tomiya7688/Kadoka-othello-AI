# Coding Rules

This project adopts the applicable parts of `tomiya7688/upd-commander-base-design` as coding rules.

The purpose is to keep the Othello runtime, AI runtime, AI Creator and model tooling easy to maintain without creating avoidable performance bottlenecks.

## 1. One file, one responsibility

A source/header pair should represent one main responsibility.

Examples:

- `obake_kadoka_evaluator.*` -> Kadoka evaluator only
- `obake_kadoka_model.*` -> Kadoka model loading only
- `script_evaluator.*` -> script evaluator runtime only
- `package_loader.*` -> package/runtime loading only

Do not mix Creator-only analysis/conversion logic into Runtime files.

Small helper types, enums and private helpers may remain with the responsibility they support.

## 2. One module, one responsibility

C++ modules may be classes, namespaces or free-function groups. OOP is not required.

Prefer responsibility boundaries over class boundaries.

## 3. One function, one action

A function should normally perform one coherent action.

Split unrelated steps such as parsing, validation, evaluation, persistence and presentation.

Do not over-split tiny operations when the extra calls or abstractions would make a measured hot path slower or harder to optimize.

## 4. Comments describe intent

Use comments to explain the purpose of a processing block, invariant, non-obvious optimization or boundary rule.

Avoid comments that only restate the next line of code.

Performance exceptions must explain why the less-structured form is necessary when that reason is not obvious from the code.

## 5. Closed processing modules

Processing modules should normally be called by their owning coordinator/runner and return results to it instead of calling peer processing modules directly.

In this project the equivalent principle is:

```text
Game / Headless / AI Runtime coordinator
  -> processing module
  -> return result
```

Avoid hidden horizontal call chains between independent AI/runtime processing modules.

Allowed internal calls include:

- private helpers belonging to the same responsibility
- pure functions
- data/value types
- explicitly shared utility code

## 6. Dependency direction

The current high-level dependency direction is:

```text
AI Creator
    -> Creator Support
    -> AI Runtime
    -> Othello Core
```

The reverse direction is forbidden.

In particular:

- Runtime must not include or link Creator Support.
- Headless/Game execution must not depend on Dataset conversion/import/analysis code.
- Creator may use Runtime interfaces.
- Shared protocol/data contracts must not contain Creator-only or GUI-only behavior.

These rules apply to include/import/reference relationships, not only runtime calls.

## 7. Runtime hot-path exception

Performance-sensitive code is allowed to deviate from normal decomposition when measurement or clear complexity shows that the normal structure would create a meaningful bottleneck.

Typical hot paths include:

- move generation
- board access
- evaluation functions
- search loops
- transposition-table access
- rollout / Monte Carlo loops
- large-scale self-play / Dataset generation
- character AI candidate evaluation
- model inference adapters

Acceptable exceptions include:

- fixed-size arrays instead of dynamically allocated containers
- combining tightly coupled calculation stages in one loop
- avoiding virtual calls in inner loops
- inlining small helpers
- data-oriented structures instead of responsibility-heavy class hierarchies
- caching/precomputation
- board-size-specific optimized paths
- specialized 8x8 bitboard implementations

An exception should satisfy all of the following:

1. It is limited to the performance-sensitive area.
2. The public responsibility boundary remains understandable.
3. The reason is documented if non-obvious.
4. A benchmark or measurable requirement can justify it when practical.
5. The exception does not introduce forbidden Creator/Runtime or layer dependencies.

Performance is a valid design requirement; coding rules must not force avoidable overhead into the Runtime.

## 8. External dependency containment

Keep external dependencies inside the module that needs them.

Examples:

- Python process handling -> script evaluator runtime
- future WASM runtime -> WASM evaluator module
- GUI framework -> GUI layer only

Do not leak external-library-specific types through common AI protocol boundaries unless the protocol explicitly defines them.

## 9. Build and quality

Normal changes should:

- compile successfully
- pass existing tests
- preserve normal CI
- avoid unresolved warnings/errors
- add tests for important new behavior where practical

Formatter/linter suppressions must be narrow and have a reason.

Performance-specific code may suppress a rule only when the reason is documented and the scope is minimal.

## 10. Review checklist

Check at least:

- Does each file/module have one main responsibility?
- Does each function perform one coherent action?
- Is Runtime independent from Creator Support?
- Are processing modules horizontally coupled without need?
- Are comments explaining intent rather than syntax?
- Is a new abstraction adding allocations, copies, process launches or virtual dispatch to a hot path?
- If a performance exception exists, is its scope and reason clear?
- Does the change build and pass tests?

## Source design

These rules are adapted from `upd-commander-base-design`, especially its recommended practices, dependency rules and implementation quality requirements, with Othello AI Runtime performance exceptions added for this project.
