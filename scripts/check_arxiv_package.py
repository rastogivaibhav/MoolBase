#!/usr/bin/env python3
"""Validate the GrapheneDB paper source and arXiv metadata using stdlib only."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ALLOWED_FILENAME = re.compile(r"^[A-Za-z0-9_+\-.,=]+$")
CITATION_RE = re.compile(r"\\cite(?:t|p)?\{([^}]+)\}")
BIBKEY_RE = re.compile(r"^@[A-Za-z]+\{([^,]+),", re.MULTILINE)
PENDING_MARKERS = ("PENDING", "TO_BE_", "[public artifact URL")


def fail(message: str) -> None:
    print(f"ERROR: {message}", file=sys.stderr)
    raise SystemExit(1)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--paper-dir", default="paper")
    parser.add_argument("--strict", action="store_true", help="fail on unresolved submission placeholders")
    args = parser.parse_args()

    paper_dir = Path(args.paper_dir).resolve()
    tex_path = paper_dir / "main.tex"
    bib_path = paper_dir / "references.bib"
    metadata_path = paper_dir / "arxiv_metadata.json"

    for path in (tex_path, bib_path, metadata_path):
        if not path.is_file():
            fail(f"missing required file: {path}")

    for path in paper_dir.rglob("*"):
        if path.is_file() and not ALLOWED_FILENAME.fullmatch(path.name):
            fail(f"arXiv-incompatible file name: {path.name}")

    tex = tex_path.read_text(encoding="utf-8")
    bib = bib_path.read_text(encoding="utf-8")
    metadata = json.loads(metadata_path.read_text(encoding="utf-8"))

    abstract = metadata.get("abstract", "")
    if not abstract:
        fail("metadata abstract is empty")
    try:
        abstract.encode("ascii")
    except UnicodeEncodeError as exc:
        fail(f"metadata abstract is not ASCII: {exc}")
    if len(abstract) > 1920:
        fail(f"metadata abstract is {len(abstract)} characters; arXiv maximum is 1920")

    for required in ("title", "authors", "primary_category", "comments"):
        if not metadata.get(required):
            fail(f"metadata field is missing: {required}")

    cited: set[str] = set()
    for match in CITATION_RE.finditer(tex):
        cited.update(key.strip() for key in match.group(1).split(","))
    available = set(BIBKEY_RE.findall(bib))
    missing = sorted(cited - available)
    if missing:
        fail("missing bibliography entries: " + ", ".join(missing))

    if "\\bibliography{references}" not in tex:
        fail("main.tex does not load references.bib")
    if "semantic truth engine" not in tex:
        fail("mandatory semantic-truth non-claim is missing")
    if "global asymptotic" not in tex:
        fail("mandatory convergence boundary is missing")

    if args.strict:
        joined = tex + "\n" + json.dumps(metadata, sort_keys=True)
        unresolved = [marker for marker in PENDING_MARKERS if marker in joined]
        if unresolved:
            fail("unresolved strict-submission placeholders: " + ", ".join(unresolved))

    print(f"metadata_abstract_chars={len(abstract)}")
    print(f"citations={len(cited)} bibliography_entries={len(available)}")
    print("arxiv_package_check=PASS")


if __name__ == "__main__":
    main()
