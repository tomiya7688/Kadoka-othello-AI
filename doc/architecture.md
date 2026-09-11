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
  *.cpp            Core実装

doc/               設計・仕様資料
build.bat           Windows一発ビルド
CMakeLists.txt       ビルド定義
```

## State API

`GameSnapshot` はGUIや外部ツールへ現在局面を渡すための軽量データ構造。
盤面、手番、合法手、履歴、終局結果を保持する。

Coreは通信方式そのものを持たず、HTTPやIPC等は上位層で実装する。

## Headless Runner

初期実装ではRandom vs Randomを高速に回す。
目的はAIの強さではなく、GUIを介さず大量対局とDataset出力を行う経路を確立すること。

AI参加者インターフェースは後から差し替える。
