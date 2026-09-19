# Build

> この文書は日本語正本。文書言語方針は `doc/document-language-policy.md`。

## Windows

リポジトリ直下の `build.bat` を実行する。

```bat
build.bat
```

処理内容:

1. CMake configure
2. Kadoka rule checker
3. Release build
4. CTest実行

Rule checkerはCMakeの `kadoka_othello_runtime` 依存として組み込まれているため、`cmake --build` を直接実行した場合もRuntimeコンパイル前に実行される。

必要条件:

- CMake 3.20以上
- C++17対応コンパイラ
- Python 3（Kadoka rule checker用）
- CMakeから利用可能なVisual Studio Build Tools等

Checker成功時:

```text
Kadoka check: OK
```

違反時:

```text
src/example.cpp:12 KAD101 runtime must not include creator header: kadoka_othello/ai_creator.hpp
Kadoka check: 1 error(s)
```

詳細は `tools/kadoka_rule_checker/README.md` を参照。

## Headless Runner

ビルド後の `kadoka_othello_headless` を使用する。

基本引数:

```text
kadoka_othello_headless \
  <games> <board_size> <legacy-state-output|-> <seed> \
  <black-manifest|-> <white-manifest|-> \
  <board-state.jsonl|-> <game-aux.jsonl|->
```

legacy stateだけ出す例:

```bat
build\Release\kadoka_othello_headless.exe 10000 8 dataset.jsonl 12345
```

Game Record v1だけ出す例:

```bat
build\Release\kadoka_othello_headless.exe 100 8 - 12345 - - board-state.jsonl game-aux.jsonl
```

- `games`: 対局数
- `board_size`: 6 / 8 / 10等の偶数size
- `legacy-state-output`: compatibility用canonical state JSONL。不要なら `-`
- `seed`: 0ならnon-deterministic、固定値なら再現可能
- black/white manifest: 省略または `-` でRandom AI
- BoardState/GameAux: Game Record v1。2本同時指定する

Record仕様は `doc/game-record-v1.md`。
