# Canonical Core API

Kadoka Othello AIのCore boundaryでは1つのauthoritative state contractを使う。

## State format

canonical exchange formatはJSON。

```text
format = kadoka.core_state.v1
```

含むもの:

- board size + row-major cells
- side to move
- time

例:

```json
{
  "format": "kadoka.core_state.v1",
  "board_size": 8,
  "cells": [0, 0, 0, 0],
  "side_to_move": "black",
  "time": {
    "black_remaining_ms": null,
    "white_remaining_ms": null,
    "move_limit_ms": null
  }
}
```

cell:

- `0`: empty
- `1`: black
- `2`: white

timeはmillisecond。`null` は現在のtime-control policyが値を提供しないことを示す。

## Core stateに含めないもの

- legal move list
- candidate/ranking
- evaluation
- mobility/openness
- search diagnostics
- training labels
- game history
- terminal result metadata

必要なAI / Runtime helper / AI Creator / Dataset toolingが派生する。

## Native fast path

JSONがpublic semantic source of truthだが、native Runtimeで毎move serialize/parseはしない。

`CoreStateView` が `kadoka.core_state.v1` と1対1の意味論を持つzero-copy typed view。

```text
Board reference + side_to_move + time
```

legal movesが必要なAIはboard + side-to-moveから生成する。

`core_state_to_json()` / `parse_core_state_json()` がexchange representationを定義・検証する。

bitboard / fixed buffer / tensor / compressed representationは内部表現。

## AI output / legality

AIはmove proposalを返す。authoritativeではない。

`Game::play()` がlegalityとstate transitionを決定する。

illegal move:

- board不変
- side-to-move不変
- ply不変
- normal legal history不変
- `GameEventType::InvalidMove` 発行

Headlessはretry limit内で同じstateを再問い合わせできる。

## Game event

Core event:

- `MoveAccepted`
- `InvalidMove`
- `Pass`
- `Terminal`

listenerはoptional。

通常state transitionはlogger/GUI/Game Record writerを必要としない。

Game Record v1はこのevent contractを購読する。

## External/script

persistent sessionではtransport frame内にcanonical JSONを載せる。

```text
request 1
state {"format":"kadoka.core_state.v1",...}
end
```

legal-move recordは送らない。

## Training format

binary/compressed/tensor/feature-rich formatはalternate Core APIではない。

AI Creator / Dataset Toolingがcanonical state + separate provenance/analysisから生成する。
