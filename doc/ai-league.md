# AI League

The AI League is an orchestration layer above AI Runtime and Headless.

It is intentionally built as `kadoka_othello_league_support`, not as part of `kadoka_othello_runtime`. Game binaries do not need rating, tournament scheduling, or league logging code.

## Participant identity

A league participant is not identified only by an AI name.

`LeagueParticipantConfig` contains:

- package manifest path
- board size
- deterministic participant seed
- optional caller-provided `config_hash` for settings not yet represented by package assets

The generated participant ID also fingerprints:

- package ID and version
- package interface / adapter / entry
- manifest content
- model descriptor content
- model asset content
- board size
- seed
- extra config hash

Example shape:

```text
kadoka.obake_kadoka@0.1.0:b8:s12345:h0123456789abcdef
```

This prevents a changed model/configuration from silently sharing one rating entry simply because the display name stayed the same.

## Match scheduling

`run_round_robin_league()` currently implements deterministic round-robin scheduling.

For every participant pair and each configured round it schedules both:

```text
A black vs B white
B black vs A white
```

Each game reloads both AI packages. Stateful runtime memory therefore starts fresh for each independent league game.

Actual per-game AI seeds are derived deterministically from:

- league seed
- participant seed
- game index
- color

The derived seeds are written into the game log.

## Rating

The initial rating implementation is online Glicko-1 style rating:

- initial rating: 1500
- initial rating deviation (RD): 350
- RD is retained as rating uncertainty
- win/loss/draw counts are retained separately

This is an online update after each game rather than a full Glicko rating-period implementation. A future persistent league registry may group games into explicit rating periods without changing the participant or game-record formats.

Ratings are separated naturally by participant identity. Board size is part of that identity, so 6x6, 8x8 and 10x10 do not accidentally share one rating.

## Game log

When a game-log stream is supplied, each game emits one JSON line:

```text
format = kadoka.league_game.v1
```

It records:

- deterministic `game_id`
- black / white participant IDs
- board size
- actual derived seeds
- result
- elapsed game time
- Headless AI-call and invalid-attempt metrics
- rating and RD before / after the game

## Position dataset

When a position-dataset stream is supplied, Headless snapshots are wrapped as:

```text
format = kadoka.league_position.v1
```

Each row includes:

- `game_id`
- black / white participant IDs
- board size
- the canonical Headless snapshot

This keeps weak AI, character AI, old checkpoints and external AIs useful as position generators without treating their chosen moves as trusted training labels. Stronger relabeling belongs to the separate Multi-engine Relabeling pipeline.

## Metrics and hot path

League runs may collect:

- AI calls
- total / maximum `think()` time
- invalid attempt count
- per-turn invalid-attempt histogram
- game elapsed time

`HeadlessConfig::collect_metrics` is opt-in. Normal high-throughput Dataset generation does not pay per-call timing/histogram overhead unless requested.

## CLI

```text
kadoka_ai_league <board-size> <games-per-color> <seed> \
  <game-log.jsonl|-> <position-dataset.jsonl|-> \
  <manifest.json> <manifest.json> [...]
```

Example:

```text
kadoka_ai_league 8 2 12345 league-games.jsonl league-positions.jsonl \
  src/packages/obake_kadoka/manifest.json \
  src/packages/obake_maru/manifest.json \
  src/packages/random/manifest.json
```

`-` disables that output stream.

## Current scope

Implemented now:

- reproducible participant identity
- round-robin
- mandatory color-swapped pairing
- fresh package lifecycle per game
- online Glicko rating + RD uncertainty
- 6x6 / 8x8 / 10x10 compatible participant boundary
- common JSONL game logs
- position Dataset output with provenance IDs
- Headless timing / invalid-attempt metrics

Still intentionally left for later Issue #1 work:

- persistent league registry across executions
- random / rating-band / Candidate / Hall-of-Fame schedulers
- explicit checkpoint registry
- search nodes / simulations metrics once engines expose them through a common runtime metric interface
- rating-period Glicko/Glicko-2 if needed
