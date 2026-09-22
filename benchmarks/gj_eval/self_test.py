#!/usr/bin/env python3
"""Self-test for the GJ-Eval v1 harness. No model/provider calls."""

from __future__ import annotations

import json
import pathlib
import subprocess
import sys
import tempfile


HERE = pathlib.Path(__file__).resolve().parent
GENERATOR = HERE / "generate_worldshift.py"
RUNNER = HERE / "run_command_adapter.py"
SCORER = HERE / "score.py"


def run(*args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, *args],
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )


def ids(path: pathlib.Path) -> set[str]:
    return {
        json.loads(line)["world_id"]
        for line in path.read_text(encoding="utf-8").splitlines()
        if line.strip()
    }


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="gj-eval-self-test-") as tmp:
        root = pathlib.Path(tmp)
        a = root / "a.jsonl"
        b = root / "b.jsonl"
        c = root / "c.jsonl"

        common = ["--split", "development", "--count", "24"]
        run(str(GENERATOR), *common, "--seed", "1729", "--output", str(a))
        run(str(GENERATOR), *common, "--seed", "1729", "--output", str(b))
        run(str(GENERATOR), *common, "--seed", "1730", "--output", str(c))

        assert a.read_bytes() == b.read_bytes(), "same seed must be byte-identical"
        assert ids(a).isdisjoint(ids(c)), "different seeds must not reuse world IDs"

        good_adapter = root / "good_adapter.py"
        good_adapter.write_text(
            "import json,sys\n"
            "task=json.load(sys.stdin)\n"
            "assert 'oracle' not in task\n"
            "print(json.dumps({'root_choice':'unknown','act':'review','selected_confidence':0.1}))\n",
            encoding="utf-8",
        )
        good_out = root / "good.jsonl"
        run(
            str(RUNNER),
            "--worlds", str(a),
            "--adapter-cmd", f"{sys.executable} {good_adapter}",
            "--system", "self_test_good",
            "--state-mode", "structured",
            "--output", str(good_out),
        )
        good_rows = [
            json.loads(line)
            for line in good_out.read_text(encoding="utf-8").splitlines()
            if line.strip()
        ]
        assert len(good_rows) == 24 * 8
        assert all(row["adapter_status"] == "ok" for row in good_rows)

        bad_adapter = root / "bad_adapter.py"
        bad_adapter.write_text("print('not-json')\n", encoding="utf-8")
        bad_out = root / "bad.jsonl"
        run(
            str(RUNNER),
            "--worlds", str(a),
            "--adapter-cmd", f"{sys.executable} {bad_adapter}",
            "--system", "self_test_error",
            "--output", str(bad_out),
        )
        bad_rows = [
            json.loads(line)
            for line in bad_out.read_text(encoding="utf-8").splitlines()
            if line.strip()
        ]
        assert all(row["adapter_status"] == "error" for row in bad_rows)

        scored = run(str(SCORER), "--worlds", str(a), "--predictions", str(bad_out))
        report = json.loads(scored.stdout)
        metrics = report["systems"]["self_test_error"]
        assert metrics["final_accuracy"] == 0.0
        assert metrics["abstention_recall"] == 0.0

    print(json.dumps({
        "gj_eval_self_test": True,
        "determinism": True,
        "oracle_redaction": True,
        "adapter_failure_not_abstention": True,
    }, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
