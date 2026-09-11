# Kadoka Model Record v1

## Purpose

`ModelRecord` is the internal neutral record used by AI development tools and dataset converters.

The game itself only consumes the selected move. AI development tools may consume additional inference information.

## Core fields

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

## Board encoding

Current JSONL codec uses integers:

- `0`: empty
- `1`: black
- `2`: white

The internal representation is not tied to JSON.

## Candidates

Optional candidate records can contain:

- move
- value
- policy

Future formats may extend this with search visits, uncertainty, confidence, Monte Carlo statistics, mobility values, parity information, or model-specific fields.

## Diagnostics

`diagnostics` is a string key/value map for arbitrary inference metadata.

Examples:

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

The first standard codec is:

`kadoka.jsonl.v1`

One `ModelRecord` is written per line.

## Other formats

The internal record is separated from serialization through `IModelDataCodec`.

Possible future codecs:

- CSV
- compact binary
- MessagePack / CBOR
- NumPy-oriented training data
- PyTorch tensors
- external Othello dataset formats
- legacy Kadoka formats

Converters should normally translate external formats into `ModelRecord`, then write them through the requested codec.
