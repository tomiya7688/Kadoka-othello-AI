# AI Creator

## 目的

`kadoka_othello_ai_creator` はGUIを介さずAI packageを解析・比較・benchmark・importする開発CLI。

Game Runtimeは通常 `think()` のmove proposalだけを使う。

AI Creatorは `inspect()` を利用し、candidate evaluation、policy/value、timing、diagnostics等の開発情報を取得できる。

## Commands

### 1 AIの解析

```bat
build\Release\kadoka_othello_ai_creator.exe analyze src\packages\random\manifest.json
```

position指定:

```bat
build\Release\kadoka_othello_ai_creator.exe analyze src\packages\random\manifest.json samples\position_8x8.txt
```

出力:

- selected move
- inference time
- candidate move/value/policy
- diagnostics

### 複数AI比較

```bat
build\Release\kadoka_othello_ai_creator.exe compare samples\position_8x8.txt manifest_a.json manifest_b.json
```

各AIは同じboard / side-to-moveを受ける。

legal movesはCore APIから渡さず、必要なengineがCore rulesから派生する。

用途:

- model comparison
- regression check
- disagreement抽出

### Benchmark

```bat
build\Release\kadoka_othello_ai_creator.exe benchmark src\packages\random\manifest.json 10000 samples\position_8x8.txt
```

出力:

- iterations
- total_us
- average_us
- min_us
- max_us

Hyper Faster等のlatency-sensitive AI開発に利用できる。

### Format mapping一覧

```bat
build\Release\kadoka_othello_ai_creator.exe formats
```

Kadoka ModelRecord変換用のstatic mappingを表示する。

## Position file

先頭行:

```text
<board_size> <black|white> <ply>
```

続けて `board_size` 行。

- `B`: black
- `W`: white
- `.`: empty

例:

```text
8 black 0
........
........
........
...WB...
...BW...
........
........
........
```

AI Creatorはboard/playerからCore rulesでlegal movesを必要時に再計算する。

## Inspection contract

`AIInspection`:

- selected move
- candidate list
  - move
  - value
  - policy
- diagnostics

AIごとに全candidate情報を提供する義務はない。

Random AIはlegal setを内部生成し、uniform policyをminimal exampleとして返す。

## Runtimeとの分離

Game:

```text
CoreStateView
  -> AI think()
  -> move proposal
  -> Game validation
```

Creator:

```text
CoreStateView
  -> AI inspect()
  -> move
  -> candidates
  -> diagnostics
  -> benchmark data
```

通常Game pathへCreator analysisを混ぜない。

## 今後

Issueに従って追加する候補:

- selectable output codec
- conversion execution拡張
- candidate visualization export
- disagreement extraction
- dynamic-library loading
- per-stage timing
- Dataset Pool / Relabelとの接続
