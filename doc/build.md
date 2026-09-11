# Build

## Windows

リポジトリ直下の `build.bat` を実行する。

```bat
build.bat
```

処理内容:

1. CMake configure
2. Release build
3. CTest実行

必要条件:

- CMake 3.20以上
- C++17対応コンパイラ
- CMakeから利用可能なVisual Studio Build Tools等

## Headless Runner

ビルド後の `kadoka_othello_headless` を使用する。

```text
kadoka_othello_headless [games] [board_size] [output.jsonl] [seed]
```

例:

```bat
build\Release\kadoka_othello_headless.exe 10000 8 dataset.jsonl 12345
```

引数:

- `games`: 対局数
- `board_size`: 6 / 8 / 10等の偶数サイズ
- `output.jsonl`: 省略時はDatasetを書き出さない
- `seed`: 省略または0ならランダムseed
