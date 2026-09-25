# Persistent External AI Session

## 目的

external/script AIは既定でlong-lived stdin/stdout sessionを使用する。

package load時にchild processを1回起動し、複数decisionで再利用する。

旧方式:

```text
temp request作成
-> process起動
-> temp response作成
-> parse
-> temp file削除
```

標準方式:

```text
package load
-> child processを1回起動
-> pipe上でrequest/result
-> 同じprocessでrequest/result
-> ...
-> package破棄 / stdin close
```

transportはRuntime責務。CreatorはRuntime経由で利用できるがRuntimeからCreatorへ依存しない。

## Manifest

external executable:

```json
{
  "interface": "external_process",
  "entry": "my_ai.exe",
  "transport": "persistent",
  "timeout_ms": 5000
}
```

script:

```json
{
  "interface": "python",
  "entry": "my_ai.py",
  "transport": "persistent",
  "executable": "python",
  "timeout_ms": 5000
}
```

`persistent` がdefault。

旧temp-file transportはcompatibility用:

```json
{"transport":"legacy_oneshot"}
```

新packageでは使用しない。

## Process lifetime

external executable:

```text
<entry> --kadoka-session
```

script:

```text
<executable> <entry> --kadoka-session
```

childはstdinを読み続ける。package/session破棄時のstdin EOFがnormal shutdown signal。

EOF後も終了しないchildはRuntimeがterminateできる。

crash済みchildへshutdown commandを書き込む方式は使わない。

## Request

request IDはsession内で単調増加。

```text
request 1
state {"format":"kadoka.core_state.v1","board_size":8,"cells":[...],"side_to_move":"black","time":{"black_remaining_ms":null,"white_remaining_ms":null,"move_limit_ms":null}}
end
```

state payloadはcanonical Core JSON。

legal-move listは送らない。必要なexternal AIがboard + side-to-moveから生成する。

6x6 / 8x8 / 10x10で同じcontractを使う。

## Response

```text
result 1
move 2 3
diag source=my_ai
candidate 2 3 1.25 0.80 1.05
candidate 3 2 1.10 0.20
end
```

必須:

- matching `result <request_id>`
- 1つの `move <row> <col>`
- terminating `end`

optional:

- `diag key=value`
- `candidate row col value policy [q]`

`q` はoptional。

unknown/malformed recordは黙って受理せずrejectする。

## Failure

次をRuntime errorにする。

- processを開始できない
- response完了前にprocess exit
- timeout
- response ID mismatch
- malformed response
- moveなし

POSIXではchildがstdinを閉じた後のwriteを通常の `EPIPE` errorとして扱い、`SIGPIPE` でgame process全体を終了させない。

external AIはcanonical boardを所有しない。moveはproposalで、Game Coreがlegality/state transitionを検証する。

illegal proposalではboard / side / ply不変 + invalid-move event。

## Performance boundary

persistent transportは毎手process launch/temp fileを除去するが、serialization + pipe I/Oは残る。

```text
native / native_in_process
-> dynamic_library
-> persistent external_process / script
-> legacy_oneshot
```

native AIをこのprotocolへ迂回させない。

## Parallel Dataset generation

`ExternalAISession` は1 child processを所有し、global shared sessionにしない。

Dataset workerごとに独立package/sessionを持たせ、simulation hot pathのglobal lockとfailure共有を避ける。

## Tests

`kadoka_external_ai_session_test`:

- 同一processを2 requestで再利用
- canonical JSONからexternal helper自身がlegal moveを生成
- illegal proposalでcanonical state不変
- malformed response reject
- process exit検出
- timeout検出

helper: `kadoka_external_ai_session_helper`
