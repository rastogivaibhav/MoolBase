from __future__ import annotations

from datetime import datetime, timedelta, timezone
import json

from evidence_lab.store import FileStore


def test_cleanup_removes_expired_session(tmp_path) -> None:
    store = FileStore(tmp_path, ttl_seconds=3600)
    session_id, _ = store.create_session()
    session_file = tmp_path / "sessions" / session_id / "session.json"
    session_file.write_text(
        json.dumps(
            {
                "session_id": session_id,
                "expires_at": (datetime.now(timezone.utc) - timedelta(minutes=1)).isoformat(),
            }
        ),
        encoding="utf-8",
    )
    assert store.cleanup_expired() == 1
    assert not session_file.parent.exists()
