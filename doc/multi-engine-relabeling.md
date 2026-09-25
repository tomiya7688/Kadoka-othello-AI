# Multi-engine Relabeling / Active Re-evaluation

弱いAI・旧世代・Obake・外部棋譜などから得た**局面そのもの**を再利用し、複数engineと必要時の終盤完全読みで学習ラベルを強化する。

弱いAIの着手を正解扱いしない。ただし、そのAIが到達した局面は捨てない。

## 全体flow

```text
Game Record / Dataset Pool
  ↓
BoardState
  +
ConfidenceSample
  ↓
priority / audit sampling
  ↓
同一局面を複数AIへ投入
  ↓
各engine結果を個別保存
  ↓
disagreement計算
  ↓
終盤ならbudget付きexact solver
  ↓
before / after label + provenance
  ↓
kadoka.relabel_record.v1
  ↓
派生Dataset / relabel_history
```

この処理はAI Creator / Dataset Tooling側。

Game Runtimeの `think()` hot pathへRelabelerを依存させない。

## Input

`RelabelInput`:

- `RelabelPosition`
  - game_id
  - ply
  - canonical `CoreState`
  - optional original label
  - source Dataset ID
- matching `ConfidenceSample`

confidence側の `game_id + ply + board_size` がpositionと一致しない場合はrejectする。

Game Record v1の `BoardStateRecord` は `make_relabel_position()` で直接入力へ変換できる。

## Active selection

#2で実装した `select_reanalysis_samples()` をそのまま利用する。

優先対象:

- low confidence
- multi-AI disagreement
- candidate gapが小さい
- policy entropyが高い
- Monte Carlo varianceが高い
- exact endgame候補
- high-confidence random audit

Relabeler自身に別のpriority体系を持たせない。

## Engine spec

`RelabelEngineSpec`:

- manifest path
- deterministic seed
- rating
- rating deviation
- search config

実際のmodel ID/versionはmanifestから取得する。

各局面・各engineごとにpackageをfresh loadする。

character memoryやexternal session内状態を別局面の再評価へ漏らさないため。

engine seedは、

- configured seed
- game_id
- ply
- engine index

からdeterministicに派生する。

## 複数AI結果を潰さない

`RelabelEngineResult` をengineごとに保存する。

- model ID
- model version
- rating / RD
- search config
- actual seed
- selected move
- selected move legality
- standard Runtime metrics
  - nodes
  - simulations
  - depth
  - search_effort
- candidate evaluation
  - move
  - value
  - policy
  - optional Q
- diagnostics

Obake等がillegal moveを提案しても記録自体は残す。

ただしderived labelの候補には使わない。

## Candidate mode

### AllLegal

全合法手をRelabel recordへ保持する。

各engineが `AIInspection.candidates` でscoreを返した手はvalue/policy/Qを保存する。

engineがある合法手のscoreを提供しない場合、その手自体は残すがscore fieldはnull。

つまり「全合法手を保持する」と「すべてのengineが全合法手をscoreできる」は別。

### TopK

engineが返したlegal candidateをvalue順に並べ、最大 `top_k` 件を保持する。

selected moveがTopK外の場合は末尾と入れ替え、件数上限を超えずselected moveを保証する。

## Q value

`AICandidate` はoptional `q` を持てる。

external/script protocolのcandidateは次を許可する。

```text
candidate <row> <col> <value> <policy> [q]
```

旧4数値形式もそのまま有効。

## Disagreement

Relabel recordは単一多数決へ潰さず、各engine結果に加えて集計値だけを別保存する。

- selected move disagreement
- selected-move value standard deviation
- distinct selected move count

`move_disagreement` は合法selected moveだけを対象とする。

illegal proposalはengine provenanceには残るが、合法手disagreementの票にはしない。

## Othello局面解析

`analyze_relabel_position()` はCore stateから派生情報を作る。

- legal move count
- opponent legal move count
- mobility difference
- potential mobility
- frontier discs
- corner-anchored stable edge estimate
- corner availability difference
- empty count
- parity

これらはCore API fieldではない。

### stable discについて

現在の `stable_corner_*` は**cornerから連続するedge石の簡易推定**。

完全なstable-disc solverではないため、exact stable discsと呼ばない。

将来より強い推定器へ差し替え可能。

## Exact endgame relabeler

Creator側にbudget付きexact solverを実装する。

- board/rules Coreを使用
- pass対応
- alpha-beta pruning
- final disc differenceをside-to-move視点で返す
- 全合法手をexact evaluation
- exact best move
- visited node count

`RelabelExactResult`:

- attempted
- completed
- budget_exhausted
- nodes
- selected move
- final disc difference
- per-candidate exact value

**全候補を完了した場合だけ `completed=true`。**

budget途中で切れた場合、完了済みcandidateは残してもderived labelをexactとは扱わない。

## 盤面size別exact threshold

`RelabelBoardParameters` で6x6 / 8x8 / 10x10別に設定する。

default:

- 6x6: 12 empty
- 8x8: 14 empty
- 10x10: 10 empty

これはGame ruleではなく計算資源policy。

solver性能・machine性能に合わせて変更可能。

## Resource budget

`RelabelBudget`:

- max positions
- max engine calls
- max exact nodes
- max elapsed milliseconds
- candidate mode
- top_k
- exact solver enable/disable

engine callは開始前にelapsed/engine-call budgetを確認する。

external engine自体を途中interruptするhard timeoutは各AI transport側timeoutが担当する。

exact solverはnode/time budgetを探索中にも確認する。

## Before / After label

Relabel recordは再評価前後を分離する。

### before

元Dataset/棋譜由来:

- original selected move
- original value
- source

### after

derived label:

1. exact endgameが完全完了 → exact best move/value
2. それ以外 → 合法moveを返した再評価engineのうち、ratingが高く、同ratingならRDが小さいengineをreferenceとして採用
3. usable resultがなければ空

`after` は便利なderived labelであり、engine結果を削除しない。

学習手法によっては `after` を使わず各engine result/disagreementを直接利用してよい。

## Record format

```text
format = kadoka.relabel_record.v1
```

主なfield:

- game_id / ply / board_size / side_to_move
- source_dataset_id
- before
- confidence_before
- board_analysis
- legal_moves
- engines[]
- disagreement
- exact
- after
- provenance[]

1 position / 1 JSON lineで `write_relabel_jsonl()` から出力できる。

## Dataset Pool provenance

`make_relabelled_dataset_entry()` はRelabel結果から親Datasetの派生entryを作る。

- `source_type = derived`
- parent Dataset
- relabel対象game_id subset
- `relabelled` tag
- relabel output artifact
- `relabel_history`

history例:

```text
run=relabel-run-001;format=kadoka.relabel_record.v1;records=100;engine_calls=300;exact_nodes=250000
```

Dataset Pool側のparent/subset validationをそのまま利用する。

## Game Record CLI

`kadoka_relabel_tool` はBoardState / optional GameAux / Confidence JSONLを `game_id + ply` でjoinする。

```text
kadoka_relabel_tool \
  <board-state.jsonl> <game-aux.jsonl|-> \
  <confidence.jsonl> <output.jsonl> \
  <source-dataset-id|-> \
  <max-positions> <max-engine-calls> \
  <max-exact-nodes> <max-elapsed-ms> \
  <all|top-k> <top-k> <sampler-seed> \
  [engine-spec ...]
```

engine spec:

```text
manifest[,rating,rd,seed,search-config]
```

例:

```text
src/packages/random/manifest.json,1700,60,777,control
```

GameAuxを渡した場合、`accepted_move` at ply N+1をBoardState ply Nのoriginal labelとして復元する。

illegal attemptはoriginal correct labelとして扱わない。

## 弱いAI / Obake棋譜

Relabel inputはgenerator AI種別に依存しない。

Obake Kadoka / Maru、random、old checkpoint、人間棋譜、external AIのBoardStateを同じ形式で再評価できる。

元着手は `before` として残し、再評価結果と混同しない。

## 6x6 / 8x8 / 10x10

同じRelabel APIを使用する。

異なるのはboard-size parameter、とくにexact endgame threshold。

unit testでは3 sizeすべてを同じmulti-engine pathへ投入する。

## 設計原則

**弱いAIの手を正解にはしない。しかし弱いAIが到達した局面そのものは捨てない。**

**disagreementを消さない。exactと推定を混ぜない。計算量には明示budgetを持つ。**
