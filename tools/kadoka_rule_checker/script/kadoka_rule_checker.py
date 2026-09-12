from __future__ import annotations

import fnmatch
import re
import sys
from dataclasses import dataclass
from pathlib import Path

RUNTIME_TARGET = "kadoka_othello_runtime"
CREATOR_HEADERS = {
    "kadoka_othello/ai_creator.hpp",
    "kadoka_othello/package_import.hpp",
    "kadoka_othello/model_data.hpp",
    "kadoka_othello/conversion.hpp",
}
IGNORE_FILE = ".kadoka-check-ignore"
SOURCE_SUFFIXES = {".cpp", ".cc", ".cxx", ".hpp", ".h"}


@dataclass(frozen=True)
class Finding:
    path: str
    line: int
    code: str
    message: str


def load_ignores(root: Path) -> list[tuple[str, str, str]]:
    path = root / IGNORE_FILE
    if not path.exists():
        return []

    rules: list[tuple[str, str, str]] = []
    for number, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split(maxsplit=2)
        if len(parts) < 3:
            print(f"{IGNORE_FILE}:{number} KAD900 ignore requires: CODE GLOB reason")
            raise SystemExit(2)
        rules.append((parts[0], parts[1], parts[2]))
    return rules


def ignored(path: str, code: str, rules: list[tuple[str, str, str]]) -> bool:
    normalized = path.replace("\\", "/")
    return any(
        rule_code in {code, "*"} and fnmatch.fnmatch(normalized, pattern)
        for rule_code, pattern, _reason in rules
    )


def runtime_sources(cmake_text: str) -> set[str]:
    match = re.search(
        rf"add_library\s*\(\s*{RUNTIME_TARGET}\s+(.*?)\)",
        cmake_text,
        re.DOTALL,
    )
    if not match:
        return set()
    return {
        token.strip().replace("\\", "/")
        for token in match.group(1).split()
        if token.strip().startswith("src/")
    }


def scan_runtime_dependencies(root: Path, ignores: list[tuple[str, str, str]]) -> list[Finding]:
    cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    sources = runtime_sources(cmake)
    findings: list[Finding] = []

    if not sources:
        findings.append(Finding("CMakeLists.txt", 1, "KAD001", "runtime target not found"))
        return findings

    include_pattern = re.compile(r'^\s*#\s*include\s*[<\"]([^>\"]+)[>\"]')
    for rel in sorted(sources):
        path = root / rel
        if not path.exists() or path.suffix not in SOURCE_SUFFIXES:
            continue
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
            match = include_pattern.match(line)
            if not match:
                continue
            included = match.group(1).replace("\\", "/")
            if included in CREATOR_HEADERS and not ignored(rel, "KAD101", ignores):
                findings.append(Finding(rel, number, "KAD101", f"runtime must not include creator header: {included}"))
    return findings


def scan_cmake_boundary(root: Path, ignores: list[tuple[str, str, str]]) -> list[Finding]:
    path = root / "CMakeLists.txt"
    text = path.read_text(encoding="utf-8")
    findings: list[Finding] = []

    link_match = re.search(
        rf"target_link_libraries\s*\(\s*{RUNTIME_TARGET}\s+(.*?)\)",
        text,
        re.DOTALL,
    )
    if link_match and "kadoka_othello_creator_support" in link_match.group(1):
        line = text[: link_match.start()].count("\n") + 1
        if not ignored("CMakeLists.txt", "KAD102", ignores):
            findings.append(Finding("CMakeLists.txt", line, "KAD102", "runtime must not link creator support"))
    return findings


def scan(root: Path) -> list[Finding]:
    ignores = load_ignores(root)
    findings = scan_runtime_dependencies(root, ignores)
    findings.extend(scan_cmake_boundary(root, ignores))
    return sorted(findings, key=lambda item: (item.path, item.line, item.code))


def main() -> int:
    root = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    if not (root / "CMakeLists.txt").exists():
        print("KAD000 CMakeLists.txt not found")
        return 2

    findings = scan(root)
    for item in findings:
        print(f"{item.path}:{item.line} {item.code} {item.message}")

    if findings:
        print(f"Kadoka check: {len(findings)} error(s)")
        return 1

    print("Kadoka check: OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
