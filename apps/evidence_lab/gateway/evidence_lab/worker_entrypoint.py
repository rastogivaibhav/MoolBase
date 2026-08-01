from __future__ import annotations

import json
import os
from pathlib import Path
import traceback

from .backend import SubprocessGrapheneDBBackend
from .config import Settings
from .models import Dataset, PolicyName, Query


def main() -> int:
    input_path = Path(os.environ["EVIDENCE_LAB_JOB_INPUT"])
    output_path = Path(os.environ["EVIDENCE_LAB_JOB_OUTPUT"])
    output_path.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
    try:
        payload = json.loads(input_path.read_text("utf-8"))
        dataset = Dataset.model_validate(payload["dataset"])
        query = Query.model_validate(payload["query"])
        policy = PolicyName(payload["policy"])
        settings = Settings.from_env()
        backend = SubprocessGrapheneDBBackend(settings)
        raw, events = backend.run(dataset, query, policy)
        result = {"raw": raw, "events": events}
        status = 0
    except Exception as exc:
        result = {
            "error": f"{type(exc).__name__}: {exc}",
            "traceback": traceback.format_exc(limit=12),
        }
        status = 1
    temporary = output_path.with_suffix(".tmp")
    temporary.write_text(json.dumps(result, indent=2, sort_keys=True), encoding="utf-8")
    temporary.chmod(0o600)
    temporary.replace(output_path)
    return status


if __name__ == "__main__":
    raise SystemExit(main())
