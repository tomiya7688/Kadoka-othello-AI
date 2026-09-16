# Persistent External AI Session

## Purpose

External/script AIs use a long-lived stdin/stdout session by default.

The runtime starts one child process for the lifetime of the loaded AI package and reuses it for subsequent decisions. This avoids the old per-move path:

```text
create temp request
-> spawn process
-> create temp response
-> parse
-> delete temp files
```

The normal path is now:

```text
load package
-> start child process once
-> request/result over pipes
-> request/result over the same pipes
-> ...
-> destroy package / stop process
```

This transport belongs to Runtime. AI Creator may load packages through it, but Runtime does not depend on Creator.

## Manifest

External executable:

```json
{
  "interface": "external_process",
  "entry": "my_ai.exe",
  "transport": "persistent",
  "timeout_ms": 5000
}
```

Script:

```json
{
  "interface": "python",
  "entry": "my_ai.py",
  "transport": "persistent",
  "executable": "python",
  "timeout_ms": 5000
}
```

`persistent` is the default transport.

The old temp-file transport remains only for compatibility:

```json
{
  "transport": "legacy_oneshot"
}
```

New packages should not use `legacy_oneshot`.

## Process startup

Persistent executable packages are launched as:

```text
<entry> --kadoka-session
```

Script packages are launched as:

```text
<executable> <entry> --kadoka-session
```

The child process must remain alive and read requests from stdin until it receives:

```text
quit
```

## Request protocol

Each request has a monotonically increasing ID.

```text
request 1
size 8
row ........
row ........
row ...WB...
row ...BW...
row ........
row ........
row ........
row ........
legal_count 4
legal 2 3
legal 3 2
legal 4 5
legal 5 4
end
```

When an adapter removes legal moves, `legal_count` is zero and no `legal` records follow.

## Response protocol

```text
result 1
move 2 3
diag source=my_ai
candidate 2 3 1.25 0.80
candidate 3 2 1.10 0.20
end
```

Required:

- matching `result <request_id>`
- one `move <row> <col>`
- terminating `end`

Optional:

- `diag key=value`
- `candidate row col value policy`

Unknown or malformed records are rejected instead of being silently accepted.

## Failure behavior

The runtime reports an error when:

- the process cannot start;
- the process exits before completing a response;
- the response times out;
- the response ID does not match the request;
- the response is malformed;
- no move is returned.

The external AI does not own the canonical board. Its move remains a proposal and the game core performs legality/state-transition validation.

## Performance boundary

Persistent transport removes per-move process startup and temporary files, but serialization and pipe I/O still exist.

Use these in descending performance preference when practical:

```text
native / native_in_process
-> dynamic_library
-> persistent external_process / script
-> legacy_oneshot compatibility path
```

Do not route native AIs through this protocol.

## Parallel dataset generation

An `ExternalAISession` owns one child process and is not a shared global session. Dataset workers should own independent package/session instances. This avoids a global lock in the simulation hot path and gives each worker an independent failure boundary.

## Tests

`kadoka_external_ai_session_test` verifies:

- two requests reuse the same process;
- normal legal response;
- illegal proposal does not alter canonical game state;
- malformed response rejection;
- process exit detection;
- timeout detection.

The test helper is `kadoka_external_ai_session_helper`.
