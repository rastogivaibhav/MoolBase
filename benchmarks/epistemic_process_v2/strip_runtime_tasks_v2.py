#!/usr/bin/env python3
"""Create an oracle-free runtime copy of the V2 task universe."""
from __future__ import annotations

import argparse
import json
from pathlib import Path

ALLOWED = ("step", "id", "family", "kind", "bears_on", "depends_on", "revokes")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--tasks", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()
    source = json.loads(Path(args.tasks).read_text(encoding="utf-8"))

    episodes = []
    for task in source["episodes"]:
        events = []
        for event in task["events"]:
            row = {key: event[key] for key in ALLOWED if key in event}
            row.setdefault("depends_on", [])
            row.setdefault("revokes", [])
            events.append(row)
        episodes.append({"id": task["id"], "events": events})

    runtime = {
        "schema": "epistemic-process-v2-runtime-tasks-v1",
        "protocol": "epistemic-process-v2-blind-runtime",
        "score_bearing_allowed": False,
        "runtime_fields": list(ALLOWED),
        "episodes": episodes,
    }
    target = Path(args.out)
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(runtime, indent=2, sort_keys=True) + "\n",
                      encoding="utf-8")
    serialized = target.read_text(encoding="utf-8")
    for forbidden in (
        "terminal_operative", "terminal_committed", "challenge_warranted_steps",
        "\"independent\"", "\"decisive\"", "response_window"
    ):
        if forbidden in serialized:
            raise SystemExit(f"oracle leakage in runtime copy: {forbidden}")
    print(json.dumps({
        "runtime_tasks": len(episodes),
        "oracle_fields_present": False,
        "score_bearing_allowed": False,
    }, sort_keys=True))


if __name__ == "__main__":
    main()
