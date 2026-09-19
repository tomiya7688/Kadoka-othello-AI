# Kadoka AI Family Model Metadata

`kadoka.ai_metadata.v1` はKadoka Othello AI / Shougi AI / Tetris AIで共有するfamily-level metadata contract。

各gameのruntime manifest、model descriptor、weights、opening book、search config等を置き換えない。

目的はModel Hub / AI Creator / Training / benchmark toolingが、game固有model formatを完全理解しなくても共通metadataを読めること。

## 責務分離

```text
runtime manifest / model descriptor
  -> このgameでAIをどうload/executeするか

metadata.json (kadoka.ai_metadata.v1)
  -> identity / origin / requirements / reproducibility / license / benchmark
```

明示featureが必要としない限りRuntime hot pathはpackage load後にmetadataを要求しない。

## Required fields

```json
{
  "format": "kadoka.ai_metadata.v1",
  "model_id": "kadoka.obake_kadoka",
  "model_name": "Obake Kadoka",
  "model_version": "0.1.0",
  "architecture": "character_evaluator_randomizer",
  "game": "othello",
  "license": {
    "technical": "MIT",
    "character": "Kadoka AI Character License v1.0"
  }
}
```

意味:

- `format`: `kadoka.ai_metadata.v1`
- `model_id`: stable machine-readable ID
- `model_name`: display name
- `model_version`: package/model version
- `architecture`: high-level family
- `game`: `othello` / `shogi` / `tetris`
- `license`: stringまたはobject。technical/modelとcharacter licenseが異なる場合object推奨。

## 推奨metadata

可能な範囲で次を持つ。

- runtime requirements
- search config
- training recipe
- dataset provenance
- deterministic seed/config
- code/model origin
- benchmark results
- created_at
- parent/checkpoint
- board-size/profile

未知optional fieldはreaderが無視できるようにする。

breaking change時だけformat versionを更新する。

## Othello package

Othello packageでは `manifest.json` の `metadata` pathから参照できる。

metadataはidentity/provenance用で、canonical Core API stateではない。

## 兄弟連携

field名・意味論を可能な範囲で兄弟間共有するが、binary weightsやgame-specific model formatまで統一必須にはしない。

Othello側へ採用したmetadata仕様の文書は日本語を正本とする。


## training_recipe

学習済みmodelは、実際に使用したDataset Recipeをoptional `training_recipe` fieldへsnapshotとして記録できる。

```json
{
  "training_recipe": {
    "format": "kadoka.dataset_recipe.v1",
    "recipe_id": "eval-ml-standard-v1",
    "model_id": "kadoka.eval_ml",
    "usage": "training",
    "datasets": [
      {"dataset_id": "league-8", "ratio": 0.6},
      {"dataset_id": "exact-endgame", "ratio": 0.4}
    ]
  }
}
```

要件:

- `format = kadoka.dataset_recipe.v1`
- `usage = training`
- `training_recipe.model_id` はroot `model_id` と一致
- dataset ratioはfiniteかつ0より大きい
- actual trainingで使用したRecipe snapshotを保存する

Othelloの生成・埋め込みAPIは `doc/dataset-pool.md` を参照。
