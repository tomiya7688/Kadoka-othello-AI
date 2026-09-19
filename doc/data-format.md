# State / Dataset Format

## Canonical Core state

authoritative state exchangeは `kadoka.core_state.v1`。

詳細は `doc/core-api.md`。

Core stateに含むのはboard / side-to-move / timeのみ。

含めない:

- legal moves
- history
- evaluation
- search diagnostics
- training labels

## Game Record v1

実対局履歴は2本のJSON Linesへ分離する。

- `kadoka.board_state` version 1
- `kadoka.game_aux` version 1

BoardStateはcanonical position sequence + `game_id + ply`。

GameAuxはaccepted move、illegal attempt、invalid-move notification、pass、terminal + `event_index`。

`game_id + ply` でjoinする。

詳細は `doc/game-record-v1.md`。

## Transitional Headless state output

旧Headless single-output JSONL引数はcompatibilityのため当面残す。

現在はaccepted move後のcanonical `kadoka.core_state.v1` のみ出力する。

実対局履歴を必要とする新consumerはGame Record v1を使用する。Game Recordには次がある。

- initial position
- pass state
- illegal attempt / notification
- terminal result
- stable game ID / ply

## Dataset enrichment

Dataset metadata / analysisはCore state / Game Recordと分離する。

例:

- dataset/provenance ID
- boardから派生したlegal moves
- evaluation / candidate ranking
- search statistics
- result/relabel label
- league rating context

League/Dataset toolingはGame Record IDを参照できるが、これらをCore state schemaへ追加しない。

## Compression

JSONがexchange上の正。

fixed-length binary、compressed record、tensor、feature vectorはAI Creator / Dataset Toolingがtraining/performance用途に生成してよい。

それらを第2のGame Core APIにしない。
