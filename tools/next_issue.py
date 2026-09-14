from __future__ import annotations

import json
import re
import subprocess
import sys
from pathlib import Path

REPO = "tomiya7688/Kadoka-othello-AI"
OUTPUT = Path(".codex") / "next_issue.md"

PRIORITY_LABELS = {
    "p0": 0, "priority:p0": 0, "critical": 0, "priority:critical": 0,
    "p1": 1, "priority:p1": 1, "high priority": 1, "priority:high": 1,
    "p2": 2, "priority:p2": 2, "medium priority": 2, "priority:medium": 2,
    "p3": 3, "priority:p3": 3, "low priority": 3, "priority:low": 3,
}

SPEC_MAP = {
    "spec:architecture": ["doc/architecture.md"],
    "spec:build": ["doc/build.md", "doc/coding-rules.md"],
    "spec:runtime": ["doc/architecture.md", "doc/runtime-creator-boundary.md"],
    "spec:creator": ["doc/ai-creator.md", "doc/runtime-creator-boundary.md"],
    "spec:package": ["doc/ai-package.md", "doc/model-format.md"],
    "spec:protocol": ["doc/ai-protocol.md", "doc/external-ai-protocol.md"],
    "spec:data": ["doc/data-format.md", "doc/data-conversion.md"],
    "spec:model": ["doc/model-format.md", "doc/model-data.md"],
    "spec:obake-kadoka": ["doc/obake-kadoka.md"],
    "spec:obake-maru": ["doc/obake-maru.md"],
}

LABEL_ROUTE_MAP = {
    "spec:build": "build-policy",
    "spec:runtime": "runtime",
    "spec:creator": "creator",
    "spec:package": "model-package",
    "spec:protocol": "protocol",
    "spec:data": "data-conversion",
    "spec:model": "model-package",
    "spec:obake-kadoka": "obake-kadoka",
    "spec:obake-maru": "obake-maru",
}

KEYWORD_ROUTES = [
    (("script evaluator", "script-evaluator", "wasm evaluator", "in-process evaluator"), "script-evaluator"),
    (("checker", "coding rule", "cmake", "build", "ci"), "build-policy"),
    (("obake kadoka", "おばけかどか"), "obake-kadoka"),
    (("obake maru", "おばけまる"), "obake-maru"),
    (("model", "package", "asset", "manifest"), "model-package"),
    (("protocol", "adapter"), "protocol"),
    (("dataset", "conversion", "jsonl", "record"), "data-conversion"),
    (("ai creator", "creator"), "creator"),
    (("board", "rules", "game state", "core"), "core"),
    (("runtime", "headless"), "runtime"),
]


def gh_issues() -> list[dict]:
    cmd = ["gh", "issue", "list", "--repo", REPO, "--state", "open", "--limit", "100",
           "--json", "number,title,body,labels,url"]
    try:
        cp = subprocess.run(cmd, check=True, capture_output=True, text=True, encoding="utf-8")
    except FileNotFoundError:
        sys.exit("ERROR: gh was not found. Install GitHub CLI and run `gh auth login`.")
    except subprocess.CalledProcessError as exc:
        sys.exit((exc.stderr or exc.stdout or "gh failed").strip())
    return json.loads(cp.stdout)


def priority(issue: dict) -> tuple[int, int]:
    labels = {x.get("name", "").strip().lower() for x in issue.get("labels", [])}
    label_rank = min((PRIORITY_LABELS[x] for x in labels if x in PRIORITY_LABELS), default=50)

    title = issue.get("title", "")
    title_match = re.match(r"\s*\[P([0-3])\]", title, re.IGNORECASE)
    title_rank = int(title_match.group(1)) if title_match else 50

    return min(label_rank, title_rank), int(issue["number"])


def compact(text: str, limit: int = 900) -> str:
    if not text:
        return "No description provided."
    out = []
    code = False
    for raw in text.splitlines():
        line = raw.strip()
        if line.startswith("```"):
            code = not code
            continue
        if code or not line:
            continue
        line = re.sub(r"^#{1,6}\s*", "", line)
        line = re.sub(r"^[-*+]\s+", "", line)
        line = re.sub(r"^\d+[.)]\s+", "", line)
        line = re.sub(r"\[(.*?)\]\([^)]*\)", r"\1", line)
        out.append(line)
    result = re.sub(r"\s+", " ", " ".join(out)).strip() or "No usable description provided."
    return result if len(result) <= limit else result[:limit - 1].rstrip() + "…"


def extract_section(body: str, names: tuple[str, ...], limit: int = 700) -> str | None:
    if not body:
        return None
    lines = body.splitlines()
    wanted = {name.lower() for name in names}
    for index, raw in enumerate(lines):
        heading = re.sub(r"^#{1,6}\s*", "", raw.strip()).strip().lower()
        if heading not in wanted:
            continue
        section: list[str] = []
        for line in lines[index + 1:]:
            if re.match(r"^#{1,6}\s+", line.strip()):
                break
            section.append(line)
        text = compact("\n".join(section), limit)
        if text in {"No description provided.", "No usable description provided."}:
            return None
        return text
    return None


def choose_route(issue: dict) -> str | None:
    labels = {x.get("name", "").strip().lower() for x in issue.get("labels", [])}
    for label, route in LABEL_ROUTE_MAP.items():
        if label in labels:
            return route

    haystack = f"{issue.get('title', '')}\n{issue.get('body', '')}".lower()
    for keywords, route in KEYWORD_ROUTES:
        if any(keyword in haystack for keyword in keywords):
            return route
    return None


def relevant_docs(labels: list[str], route: str | None) -> list[str]:
    lower = {x.lower() for x in labels}
    docs: list[str] = []
    for label, paths in SPEC_MAP.items():
        if label in lower:
            for path in paths:
                if path not in docs:
                    docs.append(path)
    if route and "doc/context-routing.md" not in docs:
        docs.insert(0, "doc/context-routing.md")
    if "AI_CONTEXT.md" not in docs:
        docs.insert(0, "AI_CONTEXT.md")
    return docs


def main() -> int:
    issues = gh_issues()
    if not issues:
        print("No open issues.")
        return 0

    issue = min(issues, key=priority)
    labels = [x.get("name", "") for x in issue.get("labels", [])]
    rank, _ = priority(issue)
    ptxt = f"P{rank}" if rank < 4 else "unlabeled"
    body = issue.get("body") or ""
    summary = compact(body)
    required = extract_section(body, ("要件", "required", "requirements")) or summary
    acceptance = extract_section(body, ("完了条件", "acceptance", "acceptance criteria")) or \
        "Use the Issue as source of truth and verify the requested behavior directly."
    route = choose_route(issue)
    docs = relevant_docs(labels, route)
    doc_lines = [f"- `{x}`" for x in docs]

    route_lines = ["- No route matched. Search only the directly named subsystem first."]
    if route:
        route_lines = [
            f"- Route: `{route}`",
            f"- Inspect with: `python tools/context_route.py {route}`",
        ]

    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text("\n".join([
        "# Next Issue", "",
        f"Issue: #{issue['number']} — {issue['title']}",
        f"Priority: {ptxt}",
        f"Labels: {', '.join(labels) if labels else '(none)'}",
        f"URL: {issue['url']}", "",
        "## Goal", issue["title"], "",
        "## Required", required, "",
        "## Acceptance", acceptance, "",
        "## Context route", *route_lines, "",
        "## Relevant documents", *doc_lines, "",
        "## Exploration stop condition",
        "Stop broad exploration once Goal / Required / Acceptance, target source, matching validation, and the affected contract boundary are clear.",
        "Reopen exploration only when implementation or validation reveals a concrete missing dependency.", "",
        "## Validation",
        "Start with the route-specific smallest sufficient evidence. Broaden for shared/public/core/build-contract changes.",
        "Before PR/handoff, run `build.bat` unless the task explicitly defines a narrower valid completion gate.", "",
        "## Codex instruction",
        "Read `AI_CONTEXT.md` first, then this capsule, then only the routed source/tests/docs needed for the task.",
        "Do not preload unrelated docs, Issues, history, generated datasets or successful logs.",
        "Treat the GitHub Issue and source/spec files as source of truth; this capsule is only an index.", ""
    ]), encoding="utf-8")

    print(f"[{ptxt}] #{issue['number']} {issue['title']}")
    if route:
        print(f"route={route}")
    print(summary)
    print(f"\nCodex context written to: {OUTPUT}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
