# Kadoka Model Format

`model.json` is a root descriptor, not the whole model.

Standard package shape:

```text
package/
  manifest.json
  model.json
  evaluator.json
  behavior.json
  other assets...
```

`manifest.json` describes package loading and protocol details.
`model.json` lists the assets that form one model instance.

Root format: `kadoka.model.v1`

Example:

```json
{
  "format": "kadoka.model.v1",
  "model_id": "kadoka.obake_kadoka",
  "model_kind": "character_evaluator_randomizer",
  "engine": "builtin:kadoka.obake_kadoka",
  "assets": [
    {"id":"evaluator","type":"kadoka.evaluator.linear.v1","path":"evaluator.json","required":true},
    {"id":"behavior","type":"kadoka.character_behavior.v1","path":"behavior.json","required":true}
  ]
}
```

Each asset has a stable `id`, a versioned `type`, a relative `path`, and a `required` flag.

Assets are not limited to JSON. A complex AI may reference evaluator parameters, neural-network files, opening books, search settings, endgame tables, calibration data, learned trees, executable evaluator modules, or other binary/model assets.

## Script evaluator asset

A model can own executable evaluation logic through a `kadoka.script_evaluator.v1` asset:

```json
{
  "id": "evaluator",
  "type": "kadoka.script_evaluator.v1",
  "path": "evaluator.json",
  "required": true
}
```

`evaluator.json` selects the runtime independently from the model root.

Examples:

```json
{"format":"kadoka.script_evaluator.v1","runtime":"python_process","script":"evaluator.py"}
```

```json
{"format":"kadoka.script_evaluator.v1","runtime":"native_process","program":"evaluator.exe"}
```

```json
{
  "format":"kadoka.script_evaluator.v1",
  "runtime":"native_in_process",
  "library":"evaluator.dll",
  "symbol":"kadoka_script_evaluator_v1"
}
```

The logical `features batch -> values/diagnostics` contract remains the same across runtimes. See `doc/script-evaluator-runtime.md` for the process protocol and native in-process ABI.

## Complex model layout

Example:

```text
hybrid_ai/
  manifest.json
  model.json
  evaluator.json
  evaluator.dll
  network.onnx
  opening.bin
  search.json
  endgame.bin
```

The game still sees only the common AI protocol. The model engine loads whichever assets it understands.

Obake Kadoka and Obake Maru are reference models using the same root format. Their evaluator and behavior/randomizer settings are separate so tuning can change weights without changing engine code.

Version `format` and asset `type` independently. Unknown optional assets may be ignored; unknown required assets should fail loading.

Native in-process evaluator modules contain executable code and therefore belong to the package trust boundary. A package should not silently elevate an untrusted model asset into an in-process native module.
