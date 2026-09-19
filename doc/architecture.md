# Kadoka Othello AI Architecture

> この文書は日本語正本。文書言語方針は `doc/document-language-policy.md`。

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
- canonical Core state生成
- lightweight Game event発行

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

authoritative exchange contractは `kadoka.core_state.v1` JSON。

含むのは次だけ。

- board
- side-to-move
- time

legal moves、history、result metadata、evaluation、search diagnosticsはCore state fieldではない。

native Runtimeは同じ意味論のzero-copy `CoreStateView` を使い、hot pathでのJSON serializationを避ける。

external/script transportだけがcanonical JSONへserializeする。

詳細は `doc/core-api.md`。

## AI Protocol

全AIはboard + side-to-move + timeという同じlogical stateを受け取る。

合法手が必要なAIはstateから内部生成する。旧pass-through/drop-legal-moves adapter layerは撤去済み。

AIはmove proposalを返し、`Game` がauthoritativeに検証する。

illegal proposal:

- board不変
- side-to-move不変
- ply不変
- Core eventとして `InvalidMove` 発行

通常対局は `think()`、Creator/analysisは必要時に `inspect()` を使う。

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
Core APIは合法手一覧をAIへ渡さず、AI出力は常にproposalとして扱う。

Headless Runnerでは違法手の場合は同じ手番で再問い合わせする。
無限ループ防止のため試行回数上限を持つ。
Game Recordでは違法proposalをGameAuxへ記録し、BoardStateは増やさない。

GUIでは、かどか・まる等の違法手試行に対して専用演出を追加可能。

## Headless Runner

Headless Runnerは `AIPackage` を黒・白それぞれ受け取り、GUIを介さず対局する。
大量対局はRuntime経路だけで実行する。

Dataset生成時も、対局そのものはRuntime、重い整形・変換・再評価はCreator/Tooling側に分離する。

AIの強さや実装言語はRunnerの責務ではない。
