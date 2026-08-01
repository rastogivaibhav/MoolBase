from __future__ import annotations

import json

from .config import Settings
from .store import FileStore


def main() -> int:
    settings = Settings.from_env()
    store = FileStore(settings.data_dir, settings.session_ttl_seconds)
    removed = store.cleanup_expired()
    print(json.dumps({"evidence_lab_cleanup": "PASS", "sessions_removed": removed}, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
