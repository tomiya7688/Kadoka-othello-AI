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
- AI adapter
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

`GameSnapshot` はGUIや外部ツールへ現在局面を渡すための軽量データ構造。
盤面、手番、合法手、履歴、終局結果を保持する。

Coreは通信方式そのものを持たず、HTTPやIPC等は上位層で実装する。

## AI Protocol

すべてのAIは論理的に同じ入出力を使用する。

入力:

- 現在の盤面
- 合法手一覧

出力:

- 打つ手

AI実装そのものは `IAIEngine`、入力変換は `IAIAdapter` として分離する。
通常AIは `PassThroughAdapter`、合法手を知らないAIは `DropLegalMovesAdapter` を利用できる。

内蔵AIも外部AIもRunnerから見れば `AIPackage` として同じ扱いにする。
DLL、外部exe、Python、IPC、ネットワークAIはAdapter/Loader側で吸収し、Core Protocolは変更しない。

高速AIでは盤面と合法手をコピーせず参照するnative pathを優先し、JSON等のシリアライズは外部Transport側の責務とする。

通常のゲーム実行は `think()` を使う。
`inspect()` はCreator/開発用途であり、対局ホットパスで常用しない。

詳細は `doc/ai-protocol.md` を参照。

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
