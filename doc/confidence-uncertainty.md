# Confidence / Uncertainty / 再評価優先度

この機能はAI Creator / Dataset Tooling側で、学習sample・局面・候補評価の「どの程度信用できるか」と「追加計算をどこへ使うか」を扱う。

Core APIのfieldではない。

## 設計原則

**正本はraw factor。confidenceの計算式は差し替え可能。**

```text
Game Record / Dataset / AI analysis
  -> raw confidence factors
  -> IConfidenceCalculator
  -> confidence / uncertainty / reanalysis priority
  -> priority sampler
  -> relabel / exact solver / additional search
```

`BaselineConfidenceCalculator` は初期reference implementationであり、将来の学習型calculatorやmodel別calculatorを固定しない。

## Sample format

```text
format = kadoka.confidence_sample.v1
```

例:

```json
{
  "format": "kadoka.confidence_sample.v1",
  "game_id": "01ARZ3NDEKTSV4RRFFQ69G5FAV",
  "ply": 24,
  "board_size": 8,
  "factors": {
    "source_rating_deviation": 70,
    "search_depth": 11,
    "nodes": 150000,
    "top_candidate_gap": 0.08,
    "policy_entropy": 0.73,
    "multi_ai_move_disagreement": 0.5,
    "monte_carlo_variance": 0.12,
    "empty_count": 36
  }
}
```

factor objectはnumeric key/value map。

未知factorを保存・round-trip可能にし、新factor追加でschemaを毎回breaking changeしない。

API:

- `ConfidenceFactors::set()`
- `ConfidenceFactors::get()`
- `confidence_sample_to_json()`
- `parse_confidence_sample_json()`

## 標準factor

### 生成元 / 探索量

- `source_rating`
- `source_rating_deviation`
- `search_depth`
- `simulations`
- `nodes`
- `search_time_ms`

### candidate / policy

- `top_candidate_gap`
- `policy_entropy`
- `multi_ai_move_disagreement`
- `multi_ai_value_disagreement`
- `eval_model_disagreement`
- `monte_carlo_variance`

### 局面複雑度

- `legal_move_count`
- `mobility_self`
- `mobility_opponent`
- `potential_mobility_self`
- `potential_mobility_opponent`
- `frontier_self`
- `frontier_opponent`
- `corner_risk`
- `x_square_risk`
- `c_square_risk`
- `parity_damage`
- `region_parity_uncertainty`
- `stable_discs_self`
- `stable_discs_opponent`

### 終盤 / 結果

- `empty_count`
- `exact_endgame_available`
- `exact_endgame_agreement`
- `result_consistency`

すべて必須ではない。

生成できるfactorだけ保存し、calculatorが存在する要素から推定する。

## Disagreement helper

### Policy entropy

`normalized_policy_entropy()`

policyを正規化し、0〜1へnormalizeしたentropyを返す。

- 0: ほぼ1手へ集中
- 1: uniformに近い

### move disagreement

`move_disagreement()`

複数AIのselected moveから、最頻手以外の割合を0〜1で返す。

全AI同じ手なら0。

### value disagreement

`value_disagreement_stddev()`

複数engine valueのpopulation standard deviation。

各engineの詳細結果・model versionはMulti-engine Relabeling側で別途provenance保存する。

### Monte Carlo variance

`sample_variance()` を提供する。

rollout result等からvarianceを計算し `monte_carlo_variance` factorへ保存できる。

## Calculator

`IConfidenceCalculator`:

```text
ConfidenceSample
+ ConfidenceBoardParameters
  -> ConfidenceEstimate
```

`ConfidenceEstimate`:

- `confidence`
- `uncertainty`
- `reanalysis_priority`
- `exact_endgame_candidate`

### Baseline calculator

`BaselineConfidenceCalculator` は初期reference。

利用可能なfactorだけを正規化してconfidenceを作り、以下をpriorityへ加味する。

- uncertainty
- multi-AI disagreement
- value disagreement
- Monte Carlo variance
- policy entropy
- candidate gapの小ささ
- midgame重点
- exact endgame候補
- exact resultとの不一致

このweight/scaleを「正しいconfidence定義」とみなさない。

後からcalculatorを差し替えてよい。

raw factorを保存する理由はこのため。

## 盤面size別parameter

`ConfidenceBoardParameters` を6x6 / 8x8 / 10x10で別設定できる。

設定例:

- exact solver候補とするempty count閾値
- midgame ply範囲
- high-confidence audit threshold/rate
- depth/nodes/simulations normalize scale
- disagreement/variance scale

`default_confidence_board_parameters()` は初期値を返す。

現在のexact empty threshold初期値:

- 6x6: 12
- 8x8: 14
- 10x10: 10

これらはsolver性能や計算環境に合わせて変更可能なparameterであり、Game ruleではない。

## Priority sampler

`select_reanalysis_samples()` はbudget `max_samples` 内で再評価対象を選ぶ。

基本順:

1. sampleごとにcalculatorでestimate
2. high-confidence sampleを設定確率でrandom audit
3. 残り枠を `reanalysis_priority` 降順で埋める

random auditはexplicit seedを受け、test/experimentを再現可能にする。

## High-confidence audit

高confidenceを完全には信用しない。

`high_confidence_threshold` 以上のsampleも `high_confidence_audit_rate` の確率で再評価対象へ入れる。

これにより、

- confidence model自体の系統誤差
- Dataset shift
- 未知の難局
- 「自信満々に間違う」sample

を継続監査できる。

## Exact endgame routing

次のいずれかで `exact_endgame_candidate` にできる。

- `empty_count <= exact_endgame_empty_threshold`
- `exact_endgame_available >= 0.5`

exact candidateで `exact_endgame_agreement` がまだ無い場合、baseline priorityを上げる。

exact result取得後はagreementをfactorへ保存し、推定評価との一致度を後から再計算できる。

## Core / Runtime境界

confidence factorはCore stateへ追加しない。

Coreからの正規入力は引き続き:

- board
- side-to-move
- time

legal moves、mobility、candidate、confidence等はCreator/Dataset/AI内部で派生する。

大量対局のRuntime hot pathへconfidence calculator/samplerを依存させない。

## 次工程

#3 Multi-engine Relabelingはこのsample/factor/samplerを利用して、

- AI間で割れた局面
- Monte Carlo varianceの高い局面
- exact候補
- low-confidence局面
- random audit局面

へ再評価budgetを集中できる。
