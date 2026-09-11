from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

REPO = "tomiya7688/Kadoka-othello-AI"
BASE = "main"
NEXT_ISSUE = Path(".codex") / "next_issue.md"


def run(args: list[str], check: bool = True) -> subprocess.CompletedProcess[str]:
    cp = subprocess.run(args, text=True, capture_output=True, encoding="utf-8")
    if check and cp.returncode:
        sys.stderr.write(cp.stdout)
        sys.stderr.write(cp.stderr)
        raise subprocess.CalledProcessError(cp.returncode, args)
    return cp


def issue_number() -> str | None:
    if not NEXT_ISSUE.exists():
        return None
    m = re.search(r"Issue:\s+#(\d+)", NEXT_ISSUE.read_text(encoding="utf-8"))
    return m.group(1) if m else None


def issue_title() -> str | None:
    if not NEXT_ISSUE.exists():
        return None
    m = re.search(r"Issue:\s+#\d+\s+[—-]\s+(.+)", NEXT_ISSUE.read_text(encoding="utf-8"))
    return m.group(1).strip() if m else None


def main() -> int:
    try:
        run(["git", "rev-parse", "--show-toplevel"])
        num = issue_number()
        title = issue_title()
        branch = run(["git", "branch", "--show-current"]).stdout.strip()

        if branch == BASE:
            if not num:
                sys.exit("ERROR: On main and no .codex/next_issue.md issue number was found. Run next_issue.bat first.")
            branch = f"codex/issue-{num}"
            run(["git", "switch", "-c", branch])

        print("[1/5] Building and running CTest via build.bat...")
        cp = subprocess.run(["cmd", "/c", "build.bat"])
        if cp.returncode:
            sys.exit("ERROR: build.bat failed. PR was not created.")

        status = run(["git", "status", "--porcelain"]).stdout.strip()
        if status:
            print("[2/5] Committing tested working tree...")
            run(["git", "add", "-A"])
            message = f"Resolve issue #{num}" if num else "Prepare tested changes"
            run(["git", "commit", "-m", message])
        else:
            print("[2/5] Working tree is clean; using existing commits.")

        print("[3/5] Collecting compact change summary...")
        files = run(["git", "diff", "--name-only", f"{BASE}...HEAD"]).stdout.splitlines()
        shortstat = run(["git", "diff", "--shortstat", f"{BASE}...HEAD"]).stdout.strip() or "No diff stat available."
        commits = run(["git", "log", "--oneline", f"{BASE}..HEAD"]).stdout.splitlines()
        if not files and not commits:
            sys.exit("ERROR: No changes relative to main.")

        print("[4/5] Pushing branch...")
        run(["git", "push", "-u", "origin", branch])

        pr_title = title or (commits[0].split(" ", 1)[1] if commits and " " in commits[0] else "Update")
        body_lines = [
            "## Summary",
            shortstat,
            "",
            "## Changed files",
            *[f"- `{x}`" for x in files[:40]],
            "",
            "## Validation",
            "- `build.bat` passed (CMake build + CTest)",
        ]
        if len(files) > 40:
            body_lines.append(f"- {len(files) - 40} additional changed files omitted from this compact summary")
        if num:
            body_lines.extend(["", f"Closes #{num}"])

        print("[5/5] Creating pull request...")
        existing = run(["gh", "pr", "view", branch, "--repo", REPO, "--json", "url", "--jq", ".url"], check=False)
        if existing.returncode == 0 and existing.stdout.strip():
            print(existing.stdout.strip())
            return 0
        cp = run(["gh", "pr", "create", "--repo", REPO, "--base", BASE, "--head", branch,
                  "--title", pr_title, "--body", "\n".join(body_lines)])
        print(cp.stdout.strip())
        return 0
    except FileNotFoundError as exc:
        sys.exit(f"ERROR: Required command was not found: {exc.filename}")
    except subprocess.CalledProcessError:
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
