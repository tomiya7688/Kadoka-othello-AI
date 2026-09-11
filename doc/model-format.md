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

Assets are not limited to JSON. A future complex AI may reference evaluator parameters, neural-network files, opening books, search settings, endgame tables, calibration data, learned trees, or other binary/model assets.

Example complex layout:

```text
hybrid_ai/
  manifest.json
  model.json
  evaluator.json
  network.onnx
  opening.bin
  search.json
  endgame.bin
```

The game still sees only the common AI protocol. The model engine loads whichever assets it understands.

Obake Kadoka and Obake Maru are reference models using the same root format. Their evaluator and behavior/randomizer settings are separate so tuning can change weights without changing engine code.

Version `format` and asset `type` independently. Unknown optional assets may be ignored; unknown required assets should fail loading.
