# Obake Maru AI

## Package

```text
src/packages/obake_maru/
  manifest.json
  metadata.json
  model.json
  evaluator.json
  behavior.json
```

Obake Kadokaと同じnative package contractを使う。

## Character behavior

- Coreからlegal-move listを受け取らない。
- Maruは合法手を知らない。
- 全empty squareを小さいlocal heuristicで評価する。
- candidate間に強いrandomnessを入れる。
- 覚えるのは直前placement attemptだけ。
- 同じboardを再度見ると直前attemptがrejectされたと推定する。
- その1 squareだけ次attemptで強く避ける。
- 別squareを試すと古い失敗は忘れる。

つまり「ここだめなのだ？」と別の場所へ行く程度で、古い失敗を長く覚えない。

## Evaluation

Kadokaよりさらに単純。

好む傾向:

- existing stoneに近い
- local densityが高い
- 両色に少し触れる
- 弱いcenter tendency

Kadokaほどbracket structureを見ず、randomizer temperatureとexploration floorを大きくする。

non-uniformだが弱く、かなり雑なplayを狙う。

## Strength

原則Kadokaより弱くする。

illegal retryをGameが処理した後でもuniform randomより少しだけ特徴がある程度を想定するが、strengthはLeagueで測る。

## Creator

```bat
build\Release\kadoka_othello_ai_creator.exe analyze ^
  src\packages\obake_maru\manifest.json ^
  samples\position_8x8.txt
```
