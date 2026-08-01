from __future__ import annotations

from datetime import datetime, timedelta, timezone
import json
from pathlib import Path
import secrets
import shutil
import threading

from .models import Dataset, PublicRunResult


class StoreError(RuntimeError):
    pass


class FileStore:
    def __init__(self, root: Path, ttl_seconds: int) -> None:
        self.root = root
        self.ttl_seconds = ttl_seconds
        self._lock = threading.RLock()
        self.root.mkdir(parents=True, exist_ok=True)

    @staticmethod
    def _now() -> datetime:
        return datetime.now(timezone.utc)

    @staticmethod
    def _id(prefix: str) -> str:
        return f"{prefix}_{secrets.token_urlsafe(18)}"

    def create_session(self) -> tuple[str, datetime]:
        session_id = self._id("ses")
        expires_at = self._now() + timedelta(seconds=self.ttl_seconds)
        directory = self.root / "sessions" / session_id
        directory.mkdir(parents=True, exist_ok=False)
        (directory / "session.json").write_text(
            json.dumps({"session_id": session_id, "expires_at": expires_at.isoformat()}),
            encoding="utf-8",
        )
        return session_id, expires_at

    def _session_dir(self, session_id: str) -> Path:
        if not session_id.startswith("ses_") or "/" in session_id or ".." in session_id:
            raise StoreError("invalid session id")
        directory = self.root / "sessions" / session_id
        metadata = directory / "session.json"
        if not metadata.exists():
            raise StoreError("session not found")
        raw = json.loads(metadata.read_text("utf-8"))
        expires_at = datetime.fromisoformat(raw["expires_at"])
        if expires_at <= self._now():
            shutil.rmtree(directory, ignore_errors=True)
            raise StoreError("session expired")
        return directory

    @staticmethod
    def _validate_dataset_id(dataset_id: str) -> None:
        if not dataset_id.startswith("dat_") or "/" in dataset_id or ".." in dataset_id:
            raise StoreError("invalid dataset id")

    def save_dataset(self, session_id: str, dataset: Dataset, source_files: dict[str, bytes]) -> str:
        with self._lock:
            session_dir = self._session_dir(session_id)
            dataset_id = self._id("dat")
            directory = session_dir / "datasets" / dataset_id
            (directory / "source").mkdir(parents=True)
            for filename, data in source_files.items():
                safe_name = Path(filename).name
                if safe_name != filename:
                    raise StoreError("unsafe source filename")
                (directory / "source" / safe_name).write_bytes(data)
            (directory / "dataset.json").write_text(
                json.dumps(dataset.model_dump(mode="json"), indent=2, sort_keys=True),
                encoding="utf-8",
            )
            return dataset_id

    def load_dataset(self, session_id: str, dataset_id: str) -> Dataset:
        session_dir = self._session_dir(session_id)
        self._validate_dataset_id(dataset_id)
        path = session_dir / "datasets" / dataset_id / "dataset.json"
        if not path.exists():
            raise StoreError("dataset not found")
        return Dataset.model_validate_json(path.read_text("utf-8"))

    def load_dataset_source_files(self, session_id: str, dataset_id: str) -> dict[str, bytes]:
        session_dir = self._session_dir(session_id)
        self._validate_dataset_id(dataset_id)
        source_dir = session_dir / "datasets" / dataset_id / "source"
        if not source_dir.is_dir():
            raise StoreError("dataset source files not found")
        output: dict[str, bytes] = {}
        for path in sorted(source_dir.iterdir()):
            if path.is_file() and path.name == Path(path.name).name:
                output[path.name] = path.read_bytes()
        if not output:
            raise StoreError("dataset source files not found")
        return output

    def save_run(self, session_id: str, result: PublicRunResult, bundle: bytes) -> None:
        with self._lock:
            session_dir = self._session_dir(session_id)
            directory = session_dir / "runs" / result.run_id
            directory.mkdir(parents=True, exist_ok=False)
            (directory / "result.json").write_text(
                json.dumps(result.model_dump(mode="json"), indent=2, sort_keys=True),
                encoding="utf-8",
            )
            (directory / "bundle.zip").write_bytes(bundle)

    def load_run(self, session_id: str, run_id: str) -> PublicRunResult:
        session_dir = self._session_dir(session_id)
        if not run_id.startswith("run_") or "/" in run_id or ".." in run_id:
            raise StoreError("invalid run id")
        path = session_dir / "runs" / run_id / "result.json"
        if not path.exists():
            raise StoreError("run not found")
        return PublicRunResult.model_validate_json(path.read_text("utf-8"))

    def load_bundle(self, session_id: str, run_id: str) -> bytes:
        session_dir = self._session_dir(session_id)
        path = session_dir / "runs" / run_id / "bundle.zip"
        if not path.exists():
            raise StoreError("run bundle not found")
        return path.read_bytes()

    def delete_session(self, session_id: str) -> None:
        with self._lock:
            if not session_id.startswith("ses_") or "/" in session_id or ".." in session_id:
                raise StoreError("invalid session id")
            directory = self.root / "sessions" / session_id
            shutil.rmtree(directory, ignore_errors=True)

    def cleanup_expired(self) -> int:
        removed = 0
        root = self.root / "sessions"
        if not root.exists():
            return removed
        for directory in root.iterdir():
            try:
                self._session_dir(directory.name)
            except StoreError:
                if directory.exists():
                    shutil.rmtree(directory, ignore_errors=True)
                removed += 1
        return removed
