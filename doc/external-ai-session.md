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
-> destroy package / close stdin
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

## Process startup and shutdown

Persistent executable packages are launched as:

```text
<entry> --kadoka-session
```

Script packages are launched as:

```text
<executable> <entry> --kadoka-session
```

The child process remains alive and reads requests from stdin. Normal shutdown is signaled by stdin EOF when the package/session is destroyed. A child that does not exit promptly after EOF may be terminated by the runtime.

This avoids writing a shutdown command to a pipe after a crashed child has already closed it.

## Request protocol

Each request has a monotonically increasing ID and carries the canonical JSON state.

Conceptually:

```text
request 1
json {"board_size":8,"cells":[...],"current_player":"black","time":{"black_ms":300000,"white_ms":300000}}
end
```

The session framing may evolve, but the request payload semantics must remain identical to the canonical Core JSON state: board, side to move and time information only.

Legal moves are not transmitted as part of the canonical request. An external AI that needs them derives them locally from the board.

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

On POSIX, writes to a child that has already closed stdin are converted to normal `EPIPE` errors instead of allowing `SIGPIPE` to terminate the game process.

The external AI does not own the canonical board. Its move remains a proposal and the game core performs legality/state-transition validation.

An illegal proposal leaves board/turn state unchanged and produces an invalid-move event. A retry therefore observes the same canonical state except for time changes that result from the configured time-control policy.

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
- canonical request contains board / turn / time only;
- illegal proposal does not alter canonical board/turn state and emits an invalid-move event;
- malformed response rejection;
- process exit detection;
- timeout detection.

The test helper is `kadoka_external_ai_session_helper`.
