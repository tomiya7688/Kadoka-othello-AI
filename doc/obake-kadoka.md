# Obake Kadoka AI

## Package

Obake KadokaはKadoka-native character modelのreference package。

```text
src/packages/obake_kadoka/
  manifest.json
  metadata.json
  model.json
  evaluator.json
  behavior.json
```

## Character behavior

- Coreからlegal-move listを受け取らない。
- Kadokaは合法性を知らず、全empty squareを候補として見る。
- evaluator自体は一貫しているが通常Othello engine evaluatorではない。
- evaluator scoreへweighted randomizerをかける。
- 同じboardでも毎回同じmoveとは限らない。
- 直近1〜2回程度のplacement attemptだけ覚える。
- memoryはattempt squareとattempt前board hashを持つ。
- 同じboard hashで再度呼ばれると、直前attemptがrejectされたと推定する。
- reject推定されたrecent squareへ強いretry penalty。
- illegal attemptも新しいattemptとして短期memoryを上書きする。
- 忘れた場所へ後で再挑戦することはある。

## Obake-style evaluator

通常戦略知識を意図的に持たない。

含めない:

- opening book
- corner/X/C table
- parity strategy
- mobility search
- exact legal-move filtering

代わりに、見えている局所石配置から「置きたそうな場所」を評価する。

現在signal:

- occupied neighbor数
- isolated/open surroundings penalty
- black/white両色との接触
- ray上のlocal color transition
- line/bracket interest
- local density
- early-gameの小さいcenter tendency

line/bracket signalを強めにすることで、合法性は理解しないまま「石列に関係がありそうな場所」を好む。

Gameがrejectすると短期memoryで即retryしにくくなる。

## 強さの扱い

uniform randomより局所的に意味のある場所を好むため完全randomよりは強くなり得る。

ただし戦略engineとしてdan-level/search AIのように振る舞わせない。

実strengthはheuristicから断定せずAI Leagueで測定する。

## Dataset generation performance

native C++ engine。

hot `think()` path:

- JSON serializeしない
- process起動しない
- candidate vectorをallocateしない
- 10x10=100 cellまでfixed `std::array`
- diagnosticsを構築しない
- Coreからlegal move listを受け取らない
- empty countは1 inferenceで1回
- attempt historyは最大2 entry fixed-size

`inspect()` はanalysis pathなのでcandidate/diagnostics用allocationを許可する。

大量self-playでは通常Game/Headlessの `think()` pathを使う。
