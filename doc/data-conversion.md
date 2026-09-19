# Model Data Conversion

Kadoka Othello AIはAI開発・Dataset変換用のneutral recordとして `ModelRecord` を使う。

external formatはstatic field mapping経由で変換する。

## 原則

```text
External format
    <->
StaticFormatMapping
    <->
ModelRecord
    <->
StaticFormatMapping
    <->
External format
```

converterへformat固有AI logicを埋め込まない。

新formatは原則、新しい `StaticFormatMapping` と未実装rule用encoder/decoderだけで追加できる形を目標とする。

## Built-in mapping ID

- `csv.v1`
- `tsv.v1`
- `json.v1`
- `jsonl.v1`
- `numpy.npz.v1`
- `pytorch.tensor.v1`
- `othello.kifu.text.v1`

CSV/TSVではcomplex fieldをJSON stringとしてcell内に格納できる。

Othello kifu textはmove sequenceからboardを再構築する。

## Field mapping

各 `FieldMapping` は次を記述する。

- Kadoka `ModelRecord` field
- external field name
- logical field type
- conversion rule
- optional rule parameter
- requiredか

mappingはstatic definitionであり、Runtime hot pathへ持ち込まない。

## Core API境界

`ModelRecord` はCore stateではない。

legal moves、candidate、diagnostics、training feature等を持てるが、それらはCreator/Dataset側の派生情報。

canonical Core APIへ逆流させない。

## Conversion responsibility

conversion/import/exportはCreator/tooling責務。

Game Runtimeが通常inferenceでformat conversionを実行しない。

大量学習向けbinary/tensor conversionもCreator/Dataset側で行う。
