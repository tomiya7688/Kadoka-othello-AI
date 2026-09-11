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
    "spec:build": ["doc/build.md"],
    "spec:runtime": ["doc/architecture.md", "doc/runtime-creator-boundary.md"],
    "spec:creator": ["doc/ai-creator.md", "doc/runtime-creator-boundary.md"],
    "spec:package": ["doc/ai-package.md"],
    "spec:protocol": ["doc/ai-protocol.md", "doc/external-ai-protocol.md"],
    "spec:data": ["doc/data-format.md", "doc/data-conversion.md"],
    "spec:model": ["doc/model-format.md", "doc/model-data.md"],
    "spec:obake-kadoka": ["doc/obake-kadoka.md"],
    "spec:obake-maru": ["doc/obake-maru.md"],
}


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
    rank = min((PRIORITY_LABELS[x] for x in labels if x in PRIORITY_LABELS), default=50)
    return rank, int(issue["number"])


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


def main() -> int:
    issues = gh_issues()
    if not issues:
        print("No open issues.")
        return 0
    issue = min(issues, key=priority)
    labels = [x.get("name", "") for x in issue.get("labels", [])]
    lower = {x.lower() for x in labels}
    docs: list[str] = []
    for label, paths in SPEC_MAP.items():
        if label in lower:
            for path in paths:
                if path not in docs:
                    docs.append(path)
    rank, _ = priority(issue)
    ptxt = f"P{rank}" if rank < 4 else "unlabeled"
    summary = compact(issue.get("body") or "")
    doc_lines = [f"- `{x}`" for x in docs] or ["- No spec label matched. Read only directly relevant `doc/*.md` files."]
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text("\n".join([
        "# Next Issue", "",
        f"Issue: #{issue['number']} — {issue['title']}",
        f"Priority: {ptxt}",
        f"Labels: {', '.join(labels) if labels else '(none)'}",
        f"URL: {issue['url']}", "",
        "## Compact summary", summary, "",
        "## Relevant documents", *doc_lines, "",
        "## Codex instruction",
        "Treat this as the current highest-priority task.",
        "Read AGENTS.md first, then only the documents listed above and source files directly needed for the change.",
        "Do not preload unrelated docs or issues.",
        "Run build.bat before preparing a pull request.", ""
    ]), encoding="utf-8")
    print(f"[{ptxt}] #{issue['number']} {issue['title']}")
    print(summary)
    print(f"\nCodex context written to: {OUTPUT}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
