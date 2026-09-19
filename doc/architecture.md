# Kadoka Othello AI Architecture

## 方針

ゲーム本体は最小限のルールエンジンにする。
GUI、AI、学習、Dataset管理、画面認識などの責務はCoreから分離する。

さらにAI系は **実行機(Runtime)** と **AI作成機(AI Creator)** を明確に分離する。
これは整理上の都合ではなく、推論ホットパスのボトルネックを避けるための性能要件である。

## Coreが持つ責務

- 6x6 / 8x8 / 10x10を含む偶数サイズ盤面
- 初期配置
- 合法手判定
- 石の反転
- 手番管理
- パス
- 終局判定
- 勝敗と石数
- 最小限の着手履歴
- 現在状態のSnapshot生成

## Coreが持たない責務

- GUI描画
- 合法手の表示方法
- 評価値表示
- 解放度表示
- 画面認識
- AIの探索・学習
- Dataset Recipe / Pool管理
- 棋譜の高度な変換
- キャラクター演出

## ディレクトリ

```text
src/
  kadoka_othello/  公開ヘッダ
  tools/           CLIツール
  tests/           テスト
  *.cpp            Core / Protocol / Runtime実装
doc/               設計・仕様資料
build.bat           Windows一発ビルド
CMakeLists.txt       ビルド定義
```

## AI Runtime と AI Creator

AI Runtimeは対局中にモデルを実行するための最小経路である。
AI Creatorはモデルを作る・解析する・比較する・変換するための開発アプリケーションである。

依存方向は必ず次の一方向とする。

```text
AI Creator
    ↓
Creator Support
    ↓
AI Runtime
    ↓
Core
```

RuntimeからCreator Supportを参照してはいけない。

CMake上も以下のように分離する。

```text
kadoka_othello_runtime
kadoka_othello_creator_support
```

Headless/Game側は `kadoka_othello_runtime` のみをリンクする。
AI Creatorだけが `kadoka_othello_creator_support` をリンクする。

Runtimeに含めてよいもの:

- package/model descriptor loader
- model asset loader
- native inference
- modelが推論時に必須とするscript evaluator
- character memory
- headless self-play

Creator側に限定するもの:

- batch解析
- 複数AI比較
- package import
- Dataset変換
- format converter
- tuning支援
- rich diagnostics/report
- Creator用benchmark orchestration

詳細は `doc/runtime-creator-boundary.md` を参照。

## State API

The authoritative exchange contract is `kadoka.core_state.v1` JSON.

It contains only:

- board
- side to move
- time information

Legal moves, history, result metadata, evaluation and search diagnostics are not Core-state fields.

Native Runtime uses the zero-copy `CoreStateView` corresponding to the same semantics, avoiding JSON serialization on the hot path. External/script transports serialize that view to canonical JSON.

See `doc/core-api.md`.

## AI Protocol

All AIs receive the same logical state: board + side to move + time.

AIs that need legal moves derive them from the state. The old pass-through/drop-legal-moves adapter layer is removed.

All AIs return a proposed move. `Game` validates it authoritatively.

On an illegal proposal:

- board remains unchanged
- side to move remains unchanged
- ply remains unchanged
- `InvalidMove` is emitted through the Core event contract

Normal game execution uses `think()`. Creator/analysis paths may use `inspect()`.

## Model Package

`manifest.json` はAIパッケージの入口、`model.json` はモデル構成のルート記述子とする。

```text
manifest.json
    ↓
model.json
    ↓
assets[]
    ├─ evaluator.json
    ├─ behavior.json
    ├─ network.onnx
    ├─ opening.bin
    ├─ search.json
    ├─ script evaluator
    └─ その他モデル固有asset
```

モデル推論に必要なasset interpreterはRuntimeに属する。
モデルの生成・変換・調整はCreatorに属する。

`kadoka.script_evaluator.v1` はモデル自身が評価スクリプトを持つためのRuntime機能である。
AI Creator専用機能ではない。

## Invalid Move

AIが返した着手の合法性は常にGame Coreが判定する。
合法手一覧を渡していてもAI出力を無条件には信用しない。

Headless Runnerでは違法手の場合は同じ手番で再問い合わせする。
無限ループ防止のため試行回数上限を持つ。
通常棋譜・Datasetには成功した合法手のみを残す。

GUIでは、かどか・まる等の違法手試行に対して専用演出を追加可能。

## Headless Runner

Headless Runnerは `AIPackage` を黒・白それぞれ受け取り、GUIを介さず対局する。
大量対局はRuntime経路だけで実行する。

Dataset生成時も、対局そのものはRuntime、重い整形・変換・再評価はCreator/Tooling側に分離する。

AIの強さや実装言語はRunnerの責務ではない。
