from __future__ import annotations

import argparse
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ROUTING_DOC = ROOT / "doc" / "context-routing.md"


def load_routes() -> dict[str, str]:
    text = ROUTING_DOC.read_text(encoding="utf-8")
    matches = list(re.finditer(r"^### ([a-z0-9-]+)\s*$", text, re.MULTILINE))
    routes: dict[str, str] = {}
    for index, match in enumerate(matches):
        start = match.end()
        end = matches[index + 1].start() if index + 1 < len(matches) else len(text)
        body = text[start:end]
        next_h2 = re.search(r"^## ", body, re.MULTILINE)
        if next_h2:
            body = body[: next_h2.start()]
        routes[match.group(1)] = body.strip()
    return routes


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Print the smallest source/test/doc/validation route for a Kadoka Othello AI task."
    )
    parser.add_argument("route", nargs="?", help="Route name, e.g. runtime or script-evaluator")
    parser.add_argument("--list", action="store_true", help="List available routes")
    args = parser.parse_args()

    routes = load_routes()
    if args.list:
        for name in routes:
            print(name)
        return 0

    if not args.route:
        parser.error("route is required unless --list is used")

    body = routes.get(args.route)
    if body is None:
        available = ", ".join(routes)
        print(f"Unknown route: {args.route}")
        print(f"Available: {available}")
        return 2

    print(f"Context route: {args.route}")
    print(body)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
