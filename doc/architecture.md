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

## Core State API

Core と外部層の正規状態交換形式は JSON とする。

Core が状態として公開するのは次の最小情報だけとする。

- 盤面情報
- 手番
- 時間情報

合法手一覧、評価値、解放度、履歴、学習ラベル、探索情報などは Core API の正規状態には含めない。
必要な上位層が Core のルール機能や AI Creator / Dataset Tooling を使って派生させる。

Core は HTTP / IPC 等の transport を所有しない。通信方式は上位層の責務とするが、そこで扱う正規ペイロードの意味論は JSON state を基準とする。

Dataset や学習用に固定長バイナリ・圧縮形式・tensor 等へ変換する場合も、それは AI Creator / Dataset Tooling 側の仕事であり、Core API の正規形式を置き換えない。

## AI Protocol

Core から見た AI との正規的なやり取りは JSON state と着手提案で構成する。

入力 JSON の意味論:

- 盤面情報
- 手番
- 時間情報

出力:

- 打つ手

合法手一覧は Core API から AI へ与える必須入力ではない。通常AIが合法手を必要とする場合は、受け取った盤面から自分で生成するか、Runtime 内部のルール補助を利用する。Obake Kadoka / Maru のように合法手を知らないAIも同じ状態入力境界を使う。

内蔵AIも外部AIも Runner から見れば `AIPackage` として同じ扱いにする。
DLL、外部exe、Python、IPC、ネットワークAIは Adapter / Loader 側で吸収する。

Runtime 内部では性能のため JSON を毎回再パースせず、正規 JSON state と同値な typed/native view を利用してよい。ただしそれは内部最適化であり、Core API の正規形式を別形式へ変更するものではない。

通常のゲーム実行は `think()` を使う。
`inspect()` は Creator / 開発用途であり、対局ホットパスで常用しない。

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

AI が返した着手の合法性は常に Game Core が判定する。

違法手の場合:

- 盤面は変化しない
- 手番は進まない
- Core は違法手イベントを発生させる
- 上位層はそのイベントを GUI 表示、キャラクター反応、ログ、再問い合わせ等へ利用できる

Headless Runner は違法手イベントを受けて同じ手番で再問い合わせできる。
無限ループ防止のため試行回数上限を持つ。
通常棋譜・Datasetには成功した合法手のみを残す。

## Headless Runner

Headless Runnerは `AIPackage` を黒・白それぞれ受け取り、GUIを介さず対局する。
大量対局はRuntime経路だけで実行する。

Dataset生成時も、対局そのものはRuntime、重い整形・変換・再評価はCreator/Tooling側に分離する。

AIの強さや実装言語はRunnerの責務ではない。
