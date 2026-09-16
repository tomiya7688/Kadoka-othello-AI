# AI Context

This file is the small entrypoint for AI-assisted work in this repository.
Do not preload the whole repository, all docs, all Issues, or generated artifacts.

## Project

- Name: Kadoka Othello AI
- Main language: C++17
- Purpose: Othello core, AI Runtime, AI Creator, model/package tooling, headless self-play

## Source of Truth

- Architecture: `doc/architecture.md`
- Runtime / Creator boundary: `doc/runtime-creator-boundary.md`
- Coding rules and performance exceptions: `doc/coding-rules.md`
- Current implementation state: `doc/current-state.md`
- Sibling-project engineering policy: `doc/sibling-project-alignment.md`
- Model/package contracts: `doc/model-format.md`, `doc/ai-package.md`
- AI protocol: `doc/ai-protocol.md`, `doc/external-ai-protocol.md`
- Current task: GitHub Issue or explicit user request
- Source and tests: `src/`, `src/tests/`

## Start Here

1. Read the current task or `.codex/next_issue.md` when generated.
2. Use `doc/context-routing.md` to select the smallest working set.
3. Read target source and matching tests before broad documentation.
4. Read detailed docs only when the task needs them.
5. Stop exploring once Goal / Required / Acceptance and the affected boundary are clear.

## Important Invariants

- Correctness comes before engine strength or optimization.
- Canonical board state belongs to the Othello core/game. AI output is a proposal; `Game::play()` / rules validation remains authoritative.
- Human/protocol/AI actions must not bypass authoritative rule application.
- AI Runtime executes models; AI Creator creates, analyzes, converts and tunes them.
- Runtime must not depend on Creator Support.
- Normal game execution uses the light `think()` path; rich inspection is opt-in.
- Random/character AI behavior should accept explicit seeds for reproducible tests and comparisons.
- Performance-sensitive Runtime code may use documented coding-rule exceptions, but those exceptions must not reverse dependency direction.
- 6x6 / 8x8 / 10x10 must remain supported unless a task explicitly narrows support.
- Obake Kadoka / Maru character behavior must not gain conventional Othello knowledge accidentally.

## Sibling Projects

Kadoka Shougi AI and Kadoka Tetris AI are sibling projects. Before introducing a new CI/build/release pattern, common AI protocol, Runtime/tooling boundary, benchmark method, package convention or policy checker, briefly inspect `doc/sibling-project-alignment.md` and the relevant sibling implementation.

Adopt useful methods, not game-specific code blindly.

## Ignore Normally

- `build/`
- `.codex/` output except the current task capsule
- generated datasets and large JSONL files
- temporary evaluator input/output files
- unrelated Issues, docs and history
- full logs after a successful validation

## Context Priority

- P0: current task, acceptance conditions, architectural invariants
- P1: target source and matching tests
- P2: direct dependencies and contracts
- P3: detailed reference docs
- P4: history, unrelated subsystems, large generated outputs

## Validation

Use the route in `doc/context-routing.md` and prefer the smallest sufficient evidence first.

Baseline completion checks when the change is broad or shared:

- `python tools/kadoka_rule_checker/script/kadoka_rule_checker.py .`
- `cmake -S . -B build`
- `cmake --build build --config Release`
- `ctest --test-dir build -C Release --output-on-failure`

For random or character-AI behavior, prefer a fixed seed and bounded headless runs.
For model/package changes, validate the actual package/assets rather than inferring from source only.
For CI/build/distribution changes, source tests and built-artifact smoke are separate evidence.

## Working Rules

- Search first, read second.
- Do not mix unrelated refactors into the current task.
- Summaries are indexes, not replacements for source-of-truth files.
- If evidence is sufficient, stop exploring.
- If something remains unchecked, report it as `Unverified` instead of reading unrelated areas to fill space.
