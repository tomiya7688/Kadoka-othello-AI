# External AI Protocol v1

AI Creator can import Python scripts and external executables as Kadoka AI packages.

## Import

```bat
kadoka_othello_ai_creator.exe import python my_ai.py packages\my_ai my.ai "My AI"
```

or:

```bat
kadoka_othello_ai_creator.exe import external_process my_ai.exe packages\my_ai my.ai "My AI"
```

The source file is copied into the package directory and a `manifest.json` is generated.

## Runtime invocation

Imported external AIs are invoked with:

```text
<entry> --kadoka-input <request-file> --kadoka-output <response-file>
```

Python packages are invoked through `python <entry> ...`.

## Request format

```text
KADOKA_AI_PROTOCOL 1
size 8
........
........
........
...WB...
...BW...
........
........
........
legal_count 4
2 3
3 2
4 5
5 4
```

If the package uses `drop_legal_moves`, `legal_count` is 0 and the list is omitted.

## Response format

Minimum response:

```text
move 2 3
```

Optional development output:

```text
candidate 2 3 0.42 0.70
candidate 3 2 0.38 0.30
diag nodes=12000
diag depth=7
```

The game runtime consumes only `move`. AI Creator can consume `candidate` and `diag` records.

## Performance note

The external-process protocol starts a process and uses temporary files, so it is intended for compatibility, prototyping and model development. High-speed engines should use the future dynamic-library/native interface so board and legal-move data can be passed without serialization or process startup overhead.
