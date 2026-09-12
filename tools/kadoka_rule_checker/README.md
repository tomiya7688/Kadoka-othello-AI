# Kadoka Rule Checker

UPD Commander checkerを参考にした、Kadoka Othello AI向けの軽量静的チェッカー。

## 目的

機械的に確定できる規約だけを短い出力で検出する。
性能上の都合で許可されるhot pathの実装差は、過剰に警告しない。

## 実行

```bash
python tools/kadoka_rule_checker/script/kadoka_rule_checker.py .
```

成功:

```text
Kadoka check: OK
```

違反:

```text
src/example.cpp:12 KAD101 runtime must not include creator header: kadoka_othello/ai_creator.hpp
Kadoka check: 1 error(s)
```

## 現在の規則

- `KAD101`: Runtime sourceからCreator-only headerへの依存禁止
- `KAD102`: `kadoka_othello_runtime` から `kadoka_othello_creator_support` へのリンク禁止
- `KAD900`: ignore定義不正

## Ignore

ルートの `.kadoka-check-ignore` に記載する。

```text
CODE GLOB reason
```

例:

```text
KAD101 src/optimized/** measured hot-path exception
```

性能例外は理由必須。依存境界違反のignoreは原則使わない。

## 方針

1ファイル1責務や1関数1動作のように静的解析だけでは確定しにくい規約は、誤検知を避けるため現時点では自動エラー化しない。
