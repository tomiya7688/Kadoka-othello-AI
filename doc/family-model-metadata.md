# Kadoka AI Family Model Metadata

`kadoka.ai_metadata.v1` is the sibling-project metadata contract shared by Kadoka Othello AI, Kadoka Shougi AI, and Kadoka Tetris AI.

It does **not** replace a game's runtime manifest, model descriptor, weights, opening book, search configuration, or other game-specific assets. It exists so model catalogs, AI Creator/Training tools, benchmark tooling, and a future cross-project Model Hub can inspect the same high-level fields without understanding every game-specific model format.

## Separation of responsibilities

```text
runtime manifest / model descriptor
  -> how this game loads and executes the AI

metadata.json (kadoka.ai_metadata.v1)
  -> identity / origin / requirements / reproducibility / license / benchmark metadata
```

Runtime hot paths must not require this file after package loading unless a feature explicitly needs it.

## Required fields

```json
{
  "format": "kadoka.ai_metadata.v1",
  "model_id": "kadoka.obake_kadoka",
  "model_name": "Obake Kadoka",
  "model_version": "0.1.0",
  "architecture": "character_evaluator_randomizer",
  "game": "othello",
  "license": {
    "technical": "MIT",
    "character": "Kadoka AI Character License v1.0"
  }
}
```

Required meanings:

- `format`: exactly `kadoka.ai_metadata.v1`
- `model_id`: stable machine-readable ID
- `model_name`: display name
- `model_version`: model/package version
- `architecture`: high-level model family, not a C++ class name
- `game`: `othello`, `shogi`, or `tetris`
- `license`: string or object. An object is recommended when technical/model and character licenses differ.

## Shared optional fields

- `format_version`: version of a game-specific model/weight format when separate from this metadata format
- `variant`: `base`, `pretrained`, `user_trained`, `external_base`, `character`, or another documented value
- `runtime_requirements`: runtime name/version, CPU/GPU/VRAM/RAM/platform requirements
- `search_config`: inline search profile or reference to a game-specific search asset
- `training_recipe`: training recipe ID/version/reference
- `dataset_provenance`: datasets, self-play sources, relabel sources, hashes, or generation IDs
- `determinism`: seed support, deterministic mode, default seed policy
- `benchmark_results`: versioned benchmark records; benchmark meaning stays game-specific
- `source`: repository, revision, upstream model/source and sibling-project provenance
- `distribution`: bundled/downloaded/user/external origin, hash and redistribution information
- `created_at`: ISO-8601 creation or publication timestamp when known

Unknown fields are allowed so a game can add namespaced information without forcing a family format revision.

## Compatibility rules

1. A runtime manifest remains authoritative for execution.
2. `model_id`, `model_name`, and `model_version` should match the runtime package identity when both exist.
3. Do not put large weights or datasets in metadata.
4. Do not put secrets, machine-local absolute paths, or transient benchmark logs in metadata.
5. Benchmark records must include enough context to avoid comparing unlike runs as if they were equivalent.
6. Dataset provenance should prefer stable IDs/hashes over only human-readable names.
7. Character licensing must remain distinguishable from technical model/code licensing.

## Othello package integration

Othello `manifest.json` may reference the file with:

```json
"metadata": "metadata.json"
```

The package loader records this path but does not require metadata for legacy packages. New distributable Kadoka models should include it.

## Sibling-project rule

When this format changes, review the same revision in:

- `tomiya7688/Kadoka-othello-AI`
- `tomiya7688/Kadoka-shougi-ai`
- `tomiya7688/kadoka_tetris_ai`

Binary/model formats may remain different; the family metadata meaning should remain compatible.
