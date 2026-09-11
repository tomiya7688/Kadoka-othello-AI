# Kadoka Othello AI Architecture

## 方針

ゲーム本体は最小限のルールエンジンにする。
GUI、AI、学習、Dataset管理、画面認識などの責務はCoreから分離する。

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
  *.cpp            Core / Protocol実装
doc/               設計・仕様資料
build.bat           Windows一発ビルド
CMakeLists.txt       ビルド定義
```

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
将来的なDLL、外部exe、Python、IPC、ネットワークAIはAdapter/Loader側で吸収し、Core Protocolは変更しない。

高速AIでは盤面と合法手をコピーせず参照するnative pathを優先し、JSON等のシリアライズは外部Transport側の責務とする。

詳細は `doc/ai-protocol.md` を参照。

## Invalid Move

AIが返した着手の合法性は常にGame Coreが判定する。
合法手一覧を渡していてもAI出力を無条件には信用しない。

Headless Runnerでは違法手の場合は同じ手番で再問い合わせする。
無限ループ防止のため試行回数上限を持つ。
通常棋譜・Datasetには成功した合法手のみを残す。

GUIでは、かどか・まる等の違法手試行に対して専用演出を追加可能。

## Headless Runner

Headless Runnerは `AIPackage` を黒・白それぞれ受け取り、GUIを介さず対局する。
初期内蔵AIとしてRandom AIを用意し、大量対局とDataset出力の経路を確認できる。

AIの強さや実装言語はRunnerの責務ではない。
