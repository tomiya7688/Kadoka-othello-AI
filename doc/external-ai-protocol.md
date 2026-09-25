# External AI Protocol

AI CreatorはPython scriptやexternal executableをKadoka AI packageとしてimportできる。

通常実行はpersistent sessionを使用する。旧temp-file方式は `legacy_oneshot` compatibilityのみ。

## Import

```bat
kadoka_othello_ai_creator.exe import python my_ai.py packages\my_ai my.ai "My AI"
```

```bat
kadoka_othello_ai_creator.exe import external_process my_ai.exe packages\my_ai my.ai "My AI"
```

sourceはpackage directoryへcopyされ、`manifest.json` を生成する。

## 標準transport: persistent

manifest:

```json
{
  "interface": "external_process",
  "entry": "my_ai.exe",
  "transport": "persistent",
  "timeout_ms": 5000
}
```

起動後はstdin/stdoutを同一processで再利用する。

Request:

```text
request 1
state {"format":"kadoka.core_state.v1","board_size":8,"cells":[...],"side_to_move":"black","time":{"black_remaining_ms":null,"white_remaining_ms":null,"move_limit_ms":null}}
end
```

Core stateはboard / side-to-move / timeのみ。legal move listは送らない。

Response:

```text
result 1
move 2 3
diag source=my_ai
candidate 2 3 0.42 0.70 0.38
end
```

必須:

- request IDと一致する `result <id>`
- `move <row> <col>`
- `end`

optional:

- `candidate row col value policy [q]`
- `diag key=value`

末尾 `q` はoptional。従来の4数値candidateも有効。

Game Runtimeがauthorityとしてmoveを検証する。

## legacy_oneshot

互換性用にtemp-file方式を残す。

manifest:

```json
{"transport":"legacy_oneshot"}
```

起動形:

```text
<entry> --kadoka-input <request-file> --kadoka-output <response-file>
```

request fileにはcanonical `kadoka.core_state.v1` JSONを1行で書く。

旧 `KADOKA_AI_PROTOCOL 1` + `legal_count` 形式は現行contractではない。

新packageは `legacy_oneshot` を使用しない。

## Failure

Runtimeは次をerrorとして扱う。

- process start失敗
- timeout
- response完了前のprocess exit
- request ID mismatch
- malformed/unknown response record
- move欠落

illegal move proposalはtransport errorではない。

Game Coreがrejectし、board / side / plyを変えず `InvalidMove` eventを発行する。

## Performance

優先順の目安:

```text
native / native_in_process
-> dynamic_library
-> persistent external_process / script
-> legacy_oneshot
```

native AIをexternal protocol経由にしない。

詳細は `doc/external-ai-session.md`。
