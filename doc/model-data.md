# Kadoka Model Record v1

## 目的

`ModelRecord` はAI development tool / dataset converter用のinternal neutral record。

Game Coreのstate contractではない。

## Fields

現在のrecordは次を保持できる。

- `format_version`
- `model_id`
- `game_id`
- `ply`
- `board_size`
- `player`
- `board`
- `legal_moves`
- `selected_move`
- `candidates`
- `diagnostics`

`legal_moves` はCreator/Dataset側でboardから派生したanalysis fieldであり、Core API入力ではない。

## Board encoding

現在JSONL codecはintegerを使用する。

- `0`: empty
- `1`: black
- `2`: white

internal representationをJSONに固定しない。

## Candidate

optional candidate:

- move
- value
- policy
- optional Q

Qを持たないAIでは `null`。

将来拡張候補:

- search visits
- uncertainty / confidence
- Monte Carlo statistics
- mobility
- parity
- model-specific field

## Diagnostics

`diagnostics` はarbitrary string key/value map。

例:

```json
{
  "nodes": "250000",
  "depth": "11",
  "elapsed_us": "4200",
  "opening": "ushi",
  "engine": "kadoka.hyper_faster"
}
```

## Standard codec

`kadoka.jsonl.v1`

1 `ModelRecord` / 1 line。

## 他format

serializationは `IModelDataCodec` でinternal recordから分離する。

候補:

- CSV/TSV
- compact binary
- MessagePack / CBOR
- NumPy training data
- PyTorch tensor
- external Othello dataset
- legacy Kadoka format

converterは原則external format -> `ModelRecord` -> requested codecの順で処理する。
