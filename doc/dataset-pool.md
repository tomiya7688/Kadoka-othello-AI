# Dataset Pool / Dataset Recipe

Kadoka Othello AIでは、対局・局面・再評価結果などを全AI共通の**Dataset Pool**へ登録し、各AIが独自の**Dataset Recipe**で使用Datasetと混合比を選ぶ。

共通化するのは保管・provenance・選択基盤であり、全AIへ同じ学習分布を強制しない。

## 責務境界

Dataset Pool / RecipeはAI Creator / Dataset Tooling側の機能。

```text
Game / Headless
  -> Game Record v1
  -> Dataset Registry / Pool
  -> Dataset Recipe
  -> training / validation / benchmark / league evaluation
```

Runtimeの `think()` hot pathはDataset Registryを参照しない。

実装は `kadoka_othello_creator_support` に置く。

## Dataset Entry

schema:

```text
format = kadoka.dataset_entry.v1
```

1 entryは1つの論理Datasetを表す。

例:

```json
{
  "format": "kadoka.dataset_entry.v1",
  "dataset_id": "dataset-01ARZ3NDEKTSV4RRFFQ69G5FAV",
  "usage": "training",
  "source_type": "league",
  "board_size": 8,
  "generator_model": {
    "model_id": "kadoka.obake_kadoka",
    "model_version": "0.1.0"
  },
  "opponent_model": {
    "model_id": "kadoka.obake_maru",
    "model_version": "0.1.0"
  },
  "parent_dataset": null,
  "search_config": "",
  "relabel_history": [],
  "created_at": "2026-09-20T00:00:00Z",
  "license": "MIT",
  "game_ids": ["01ARZ3NDEKTSV4RRFFQ69G5FAV"],
  "tags": ["league"],
  "artifacts": ["board-state.jsonl", "game-aux.jsonl"]
}
```

### dataset_id

`generate_dataset_id()` は `dataset-` + 26文字ULIDを生成する。

Registry内で同じ `dataset_id` を2回登録するとerror。

### game_id

Game Record v1が生成するgame_idはULID。

Dataset Poolではimport済みlegacy dataも扱えるようgame_idをopaque stable IDとして保持するが、空文字・同一Dataset内重複は禁止する。

## Provenance

最低限追跡する。

- `source_type`
- `board_size`
- generator model ID/version
- opponent model ID/version
- `parent_dataset`
- `search_config`
- `relabel_history`
- `created_at`
- `license`
- `game_ids`
- artifact path

`source_type` はopen stringとし、次のようなsourceを表現できる。

- selfplay
- league
- champion_candidate
- external_kifu
- human_kifu
- random_exploration
- obake
- difficult_positions
- exact_endgame
- derived

## Board size

現在のPool validationはOthelloで利用する

- 6x6
- 8x8
- 10x10

を明示的に区別する。

Dataset Entryの `board_size` は必須。

## 用途分離

`DatasetUsage`:

- `training`
- `validation`
- `benchmark`
- `league_evaluation`

Recipeもtarget usageを持つ。

たとえば `usage=training` のRecipeへvalidation/benchmark Datasetを混ぜるとerror。

これにより固定評価Datasetの学習混入をAPIレベルで防ぐ。

## 派生Dataset

`derive_dataset()` はparent Datasetからgame subsetを選び、新しいDataset Entryを作る。

例tag:

- opening_only
- midgame_only
- endgame_only
- exact_endgame
- uncertain_positions
- high_disagreement
- corner_mistakes
- mobility_failures
- parity_positions
- rare_positions
- blunder_positions
- losing_recovery

派生Dataset:

- `source_type = derived`
- `parent_dataset` を必須化
- board sizeを親から継承
- generator/opponent/search/license等のprovenanceを継承
- new `created_at` を持つ
- game_idは親のsubsetだけ許可

Registry全体の `validate()` は以下も検査する。

- parent存在
- parent/child board size一致
- child game_idがparent game setのsubset
- parent chain cycleなし

派生Datasetは親artifactを参照するvirtual selectionとして使える。実際に別binary/tensorを生成する処理はDataset conversion層で行う。

## Dataset Recipe

schema:

```text
format = kadoka.dataset_recipe.v1
```

例:

```json
{
  "format": "kadoka.dataset_recipe.v1",
  "recipe_id": "eval-ml-standard-v1",
  "model_id": "eval_ml_standard",
  "usage": "training",
  "datasets": [
    {"dataset_id": "league_8x8", "ratio": 0.30},
    {"dataset_id": "exact_endgame", "ratio": 0.25},
    {"dataset_id": "uncertain_positions", "ratio": 0.20},
    {"dataset_id": "external_kifu", "ratio": 0.15},
    {"dataset_id": "selfplay", "ratio": 0.10}
  ]
}
```

ratioはfiniteかつ0より大きい値。

同じRecipeで同じdataset_idを重複指定できない。

## 重複学習防止

同じraw gameが

- raw league Dataset
- opening_only
- uncertain_positions
- relabel後Dataset

など複数経路に存在すること自体は許可する。

`DatasetRegistry::resolve()` はRecipe順に `game_id` をglobal dedupし、1つのresolved training plan内では同じgameを1回だけ残す。

重複除去で空になったsliceは除外し、残ったsliceのratioを再正規化する。

これによりprovenanceを失わず、同一対局の意図しない二重学習を防ぐ。

## Registry保存

Registryは1 Dataset Entry / 1 lineのJSON Lines。

API:

- `write_dataset_registry_jsonl()`
- `read_dataset_registry_jsonl()`
- `dataset_entry_to_json()`
- `parse_dataset_entry_json()`

Recipe:

- `dataset_recipe_to_json()`
- `parse_dataset_recipe_json()`

unknown optional fieldはreaderが無視できる。

## 学習済みmodelへのRecipe記録

`kadoka.ai_metadata.v1` のoptional field `training_recipe` に、実際に使用した `kadoka.dataset_recipe.v1` objectを埋め込む。

`metadata_with_training_recipe()` は次を検証する。

- Recipe usageが `training`
- metadata `model_id` とRecipe `model_id` が一致
- metadataに既存 `training_recipe` がない

family metadata validatorも同fieldを検証する。

これによりmodel artifactから「どのDatasetをどの比率で学習したか」を追跡できる。

## Dataset Tool

AI Creator用CLI:

```text
kadoka_dataset_tool validate <registry.jsonl>
kadoka_dataset_tool plan <registry.jsonl> <recipe.json>
kadoka_dataset_tool attach-recipe <metadata.json> <recipe.json> <output.json>
```

`plan` はusage検査・parent provenance検査・game_id dedup後のnormalized ratioとunique game countを表示する。

`attach-recipe` は学習後のmodel metadataへRecipe snapshotを埋め込む。

sample:

- `samples/dataset_pool/registry.jsonl`
- `samples/dataset_pool/recipe.json`

## 設計原則

**共通化するのはデータ生成・保管基盤。各AIが何を学ぶかはRecipeで分散させる。**
