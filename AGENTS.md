# Kadoka Othello AI — Codex Context

## Purpose
C++17 Othello engine / AI development project with headless execution, model/package support, evaluator integration, and AI Creator tooling.

## Standard workflow
- `start_task.bat` -> selects the highest-priority open Issue and writes compact context to `.codex/next_issue.md`.
- `finish_task.bat` -> runs the existing CMake Release build and CTest through `build.bat`.
- `finish_pr.bat` -> runs the compact PR workflow through `pull_request.bat`.
- The older `next_issue.bat` and `pull_request.bat` remain valid direct entrypoints.

## Build and test
- Windows: run `build.bat`.
- The script configures CMake, builds Release, and runs CTest.
- Do not replace the existing CMake/CTest workflow with another test framework unless an issue explicitly requires it.

## Architecture rules
- Runtime code belongs in `kadoka_othello_runtime` and must stay independent from Creator-only tooling.
- Creator support may depend on runtime; runtime must not depend on Creator support.
- Keep C++ source and headers under `src/` unless a task specifically requires otherwise.
- Preserve variable board-size support (6x6 / 8x8 / 10x10) unless an issue explicitly changes that behavior.
- Prefer deterministic, testable changes over hidden behavior.

## Context discipline
- If `.codex/next_issue.md` exists, read it immediately after this file and treat that issue as the current task.
- Read only the `doc/*.md` files listed by `.codex/next_issue.md` plus files directly needed to implement the issue.
- Do not preload the entire `doc/` directory or scan all Issues.
- Do not read full PR diffs for routine PR preparation; prefer changed file names, `git diff --stat` / `--shortstat`, commit summaries, and test results. Inspect full diffs only when needed to diagnose a problem.

## Key documents
- `doc/architecture.md` — responsibility boundaries and source layout
- `doc/build.md` — build / runner usage
- `doc/runtime-creator-boundary.md` — runtime vs Creator dependency boundary
- `doc/ai-creator.md` — AI Creator
- `doc/ai-package.md` — AI package format / behavior
- `doc/ai-protocol.md` and `doc/external-ai-protocol.md` — evaluator / external AI protocols
- `doc/data-format.md` and `doc/data-conversion.md` — game data and conversion
- `doc/model-format.md` and `doc/model-data.md` — model representation
- `doc/obake-kadoka.md`, `doc/obake-maru.md` — built-in AI implementations
