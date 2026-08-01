from __future__ import annotations

import io
import zipfile

import pytest

from evidence_lab.security import UploadSecurityError, scan_uploads


def archive(entries: dict[str, bytes | str]) -> bytes:
    output = io.BytesIO()
    with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED) as handle:
        for name, value in entries.items():
            handle.writestr(name, value)
    return output.getvalue()


def test_clean_json_passes() -> None:
    report = scan_uploads(
        {"dataset.json": b'{"manifest":{},"nodes":[],"edges":[],"queries":[]}'},
    )
    assert report.passed
    assert report.files_scanned == 1


def test_filename_path_traversal_is_rejected() -> None:
    with pytest.raises(UploadSecurityError):
        scan_uploads({"../dataset.json": b"{}"})


def test_zip_slip_is_rejected() -> None:
    with pytest.raises(UploadSecurityError):
        scan_uploads({"dataset.zip": archive({"../evil.json": "{}"})})


def test_nested_archive_is_rejected() -> None:
    with pytest.raises(UploadSecurityError):
        scan_uploads({"dataset.zip": archive({"nested.zip": b"PK\x03\x04"})})


def test_credential_like_material_is_rejected() -> None:
    with pytest.raises(UploadSecurityError):
        scan_uploads(
            {"dataset.json": b'{"key":"sk-proj-abcdefghijklmnopqrstuvwxyz123456"}'},
        )


def test_binary_content_is_rejected() -> None:
    with pytest.raises(UploadSecurityError):
        scan_uploads({"dataset.json": b"\x00\x01\x02"})
