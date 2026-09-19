#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

FORMAT = "kadoka.ai_metadata.v1"
VALID_GAMES = {"othello", "shogi", "tetris"}
REQUIRED_STRING_FIELDS = (
    "model_id",
    "model_name",
    "model_version",
    "architecture",
    "game",
)


def validate(path: Path, expected_game: str | None) -> list[str]:
    errors: list[str] = []
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        return [f"{path}: META000 {exc}"]

    if not isinstance(data, dict):
        return [f"{path}: META001 root must be an object"]

    if data.get("format") != FORMAT:
        errors.append(f"{path}: META101 format must be {FORMAT}")

    for field in REQUIRED_STRING_FIELDS:
        value = data.get(field)
        if not isinstance(value, str) or not value.strip():
            errors.append(f"{path}: META102 {field} must be a non-empty string")

    game = data.get("game")
    if isinstance(game, str) and game not in VALID_GAMES:
        errors.append(f"{path}: META103 unknown game: {game}")
    if expected_game and game != expected_game:
        errors.append(f"{path}: META104 expected game {expected_game}, got {game!r}")

    license_value = data.get("license")
    if isinstance(license_value, str):
        if not license_value.strip():
            errors.append(f"{path}: META105 license must not be empty")
    elif isinstance(license_value, dict):
        technical = license_value.get("technical")
        if not isinstance(technical, str) or not technical.strip():
            errors.append(f"{path}: META106 license.technical must be a non-empty string")
    else:
        errors.append(f"{path}: META105 license must be a string or object")

    for field in ("runtime_requirements", "determinism", "source", "distribution"):
        if field in data and not isinstance(data[field], dict):
            errors.append(f"{path}: META107 {field} must be an object")

    for field in ("dataset_provenance", "benchmark_results"):
        if field in data and not isinstance(data[field], list):
            errors.append(f"{path}: META108 {field} must be an array")

    training_recipe = data.get("training_recipe")
    if training_recipe is not None:
        if not isinstance(training_recipe, dict):
            errors.append(f"{path}: META109 training_recipe must be an object")
        else:
            if training_recipe.get("format") != "kadoka.dataset_recipe.v1":
                errors.append(
                    f"{path}: META110 training_recipe.format must be kadoka.dataset_recipe.v1"
                )
            for field in ("recipe_id", "model_id", "usage"):
                value = training_recipe.get(field)
                if not isinstance(value, str) or not value.strip():
                    errors.append(
                        f"{path}: META111 training_recipe.{field} must be a non-empty string"
                    )
            datasets = training_recipe.get("datasets")
            if not isinstance(datasets, list) or not datasets:
                errors.append(
                    f"{path}: META112 training_recipe.datasets must be a non-empty array"
                )
            else:
                for index, item in enumerate(datasets):
                    if not isinstance(item, dict):
                        errors.append(
                            f"{path}: META113 training_recipe.datasets[{index}] must be an object"
                        )
                        continue
                    dataset_id = item.get("dataset_id")
                    ratio = item.get("ratio")
                    if not isinstance(dataset_id, str) or not dataset_id.strip():
                        errors.append(
                            f"{path}: META114 training_recipe.datasets[{index}].dataset_id must be a non-empty string"
                        )
                    if not isinstance(ratio, (int, float)) or isinstance(ratio, bool) or ratio <= 0:
                        errors.append(
                            f"{path}: META115 training_recipe.datasets[{index}].ratio must be > 0"
                        )

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate kadoka.ai_metadata.v1 files")
    parser.add_argument("paths", nargs="+", type=Path)
    parser.add_argument("--game", choices=sorted(VALID_GAMES))
    args = parser.parse_args()

    errors: list[str] = []
    for path in args.paths:
        errors.extend(validate(path, args.game))

    for error in errors:
        print(error)
    if errors:
        print(f"Family metadata check: {len(errors)} error(s)")
        return 1

    print(f"Family metadata check: OK ({len(args.paths)} file(s))")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
