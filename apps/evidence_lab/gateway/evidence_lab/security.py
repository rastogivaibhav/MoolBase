from __future__ import annotations

from dataclasses import asdict, dataclass, field
import io
import os
from pathlib import PurePosixPath
import re
import socket
import stat
import struct
import zipfile
from typing import Any, Mapping


class UploadSecurityError(ValueError):
    pass


@dataclass(frozen=True)
class SecurityFinding:
    code: str
    severity: str
    file: str
    detail: str


@dataclass
class UploadSecurityReport:
    files_scanned: int = 0
    bytes_scanned: int = 0
    archive_members_scanned: int = 0
    clamav_status: str = "not_configured"
    findings: list[SecurityFinding] = field(default_factory=list)

    @property
    def passed(self) -> bool:
        return not any(item.severity == "block" for item in self.findings)

    def to_dict(self) -> dict[str, Any]:
        return {
            "passed": self.passed,
            "files_scanned": self.files_scanned,
            "bytes_scanned": self.bytes_scanned,
            "archive_members_scanned": self.archive_members_scanned,
            "clamav_status": self.clamav_status,
            "findings": [asdict(item) for item in self.findings],
        }


_ALLOWED_EXTENSIONS = {".json", ".ndjson", ".jsonl", ".csv", ".tsv", ".zip"}
_NESTED_ARCHIVES = {".zip", ".tar", ".gz", ".tgz", ".bz2", ".xz", ".7z", ".rar"}
_SECRET_PATTERNS: tuple[tuple[str, re.Pattern[bytes]], ...] = (
    ("private_key", re.compile(rb"-----BEGIN (?:RSA |EC |OPENSSH |DSA )?PRIVATE KEY-----")),
    ("aws_access_key", re.compile(rb"\b(?:AKIA|ASIA)[A-Z0-9]{16}\b")),
    ("github_token", re.compile(rb"\bgh[pousr]_[A-Za-z0-9]{30,255}\b")),
    ("openai_key", re.compile(rb"\bsk-(?:proj-)?[A-Za-z0-9_-]{20,255}\b")),
    ("google_api_key", re.compile(rb"\bAIza[0-9A-Za-z_-]{30,50}\b")),
    ("slack_token", re.compile(rb"\bxox[baprs]-[A-Za-z0-9-]{20,255}\b")),
)


def _suffix(name: str) -> str:
    lowered = name.lower()
    for ext in sorted(_ALLOWED_EXTENSIONS | _NESTED_ARCHIVES, key=len, reverse=True):
        if lowered.endswith(ext):
            return ext
    return os.path.splitext(lowered)[1]


def _validate_filename(filename: str, max_length: int) -> None:
    if not filename or len(filename) > max_length:
        raise UploadSecurityError("invalid or overlong filename")
    path = PurePosixPath(filename.replace("\\", "/"))
    if path.is_absolute() or len(path.parts) != 1 or path.name != filename:
        raise UploadSecurityError(f"unsafe filename: {filename}")
    if filename in {".", ".."} or any(ord(char) < 32 for char in filename):
        raise UploadSecurityError(f"unsafe filename: {filename}")
    if _suffix(filename) not in _ALLOWED_EXTENSIONS:
        raise UploadSecurityError(f"unsupported file extension: {filename}")


def _looks_textual(data: bytes) -> bool:
    if b"\x00" in data:
        return False
    try:
        data.decode("utf-8")
    except UnicodeDecodeError:
        return False
    return True


def _validate_signature(filename: str, data: bytes) -> None:
    ext = _suffix(filename)
    if ext == ".zip":
        if not data.startswith((b"PK\x03\x04", b"PK\x05\x06", b"PK\x07\x08")):
            raise UploadSecurityError(f"ZIP signature mismatch: {filename}")
        return
    if not _looks_textual(data):
        raise UploadSecurityError(f"non UTF-8 or binary content rejected: {filename}")
    stripped = data.lstrip()
    if ext == ".json" and stripped[:1] not in {b"{", b"["}:
        raise UploadSecurityError(f"JSON signature mismatch: {filename}")


def _secret_findings(filename: str, data: bytes) -> list[SecurityFinding]:
    output: list[SecurityFinding] = []
    for code, pattern in _SECRET_PATTERNS:
        if pattern.search(data):
            output.append(SecurityFinding(code=code, severity="block", file=filename, detail="credential-like content detected"))
    return output


def _is_symlink(member: zipfile.ZipInfo) -> bool:
    mode = (member.external_attr >> 16) & 0xFFFF
    return stat.S_ISLNK(mode)


def _scan_zip(
    filename: str,
    data: bytes,
    report: UploadSecurityReport,
    *,
    max_archive_members: int,
    max_archive_member_bytes: int,
    max_uncompressed_bytes: int,
    max_compression_ratio: float,
    max_filename_length: int,
) -> None:
    try:
        archive = zipfile.ZipFile(io.BytesIO(data))
    except zipfile.BadZipFile as exc:
        raise UploadSecurityError(f"invalid ZIP archive: {filename}") from exc
    with archive:
        members = archive.infolist()
        if len(members) > max_archive_members:
            raise UploadSecurityError(f"archive has too many members: {filename}")
        total = 0
        for member in members:
            if member.is_dir() or member.filename.endswith("/"):
                continue
            path = PurePosixPath(member.filename.replace("\\", "/"))
            if path.is_absolute() or ".." in path.parts or not path.name:
                raise UploadSecurityError(f"unsafe archive path: {member.filename}")
            if len(member.filename) > max_filename_length:
                raise UploadSecurityError(f"overlong archive filename: {member.filename}")
            if member.flag_bits & 0x1:
                raise UploadSecurityError(f"encrypted archive members are not accepted: {member.filename}")
            if _is_symlink(member):
                raise UploadSecurityError(f"archive symlink rejected: {member.filename}")
            if _suffix(path.name) in _NESTED_ARCHIVES:
                raise UploadSecurityError(f"nested archive rejected: {member.filename}")
            if member.file_size > max_archive_member_bytes:
                raise UploadSecurityError(f"archive member exceeds size limit: {member.filename}")
            total += member.file_size
            if total > max_uncompressed_bytes:
                raise UploadSecurityError("archive exceeds maximum uncompressed size")
            compressed = max(1, member.compress_size)
            if member.file_size / compressed > max_compression_ratio:
                raise UploadSecurityError(f"archive compression ratio exceeds limit: {member.filename}")
            payload = archive.read(member)
            if len(payload) != member.file_size:
                raise UploadSecurityError(f"archive member size mismatch: {member.filename}")
            _validate_signature(path.name, payload)
            report.archive_members_scanned += 1
            report.bytes_scanned += len(payload)
            report.findings.extend(_secret_findings(f"{filename}:{member.filename}", payload))


def _clamav_scan(host: str, port: int, data: bytes, timeout: float) -> str:
    with socket.create_connection((host, port), timeout=timeout) as sock:
        sock.sendall(b"zINSTREAM\0")
        view = memoryview(data)
        for offset in range(0, len(data), 64 * 1024):
            chunk = view[offset : offset + 64 * 1024]
            sock.sendall(struct.pack("!I", len(chunk)))
            sock.sendall(chunk)
        sock.sendall(struct.pack("!I", 0))
        response = bytearray()
        while len(response) < 8192:
            block = sock.recv(4096)
            if not block:
                break
            response.extend(block)
            if b"\0" in block or b"\n" in block:
                break
    text = bytes(response).replace(b"\0", b"").decode("utf-8", "replace").strip()
    if text.endswith("OK"):
        return "clean"
    if "FOUND" in text:
        raise UploadSecurityError("malware scanner rejected the upload")
    raise UploadSecurityError(f"unexpected malware scanner response: {text[:200]}")


def scan_uploads(
    files: Mapping[str, bytes],
    *,
    max_files: int = 8,
    max_filename_length: int = 180,
    max_archive_members: int = 64,
    max_archive_member_bytes: int = 5 * 1024 * 1024,
    max_uncompressed_bytes: int = 20 * 1024 * 1024,
    max_compression_ratio: float = 100.0,
    block_secrets: bool = True,
    clamav_host: str | None = None,
    clamav_port: int = 3310,
    clamav_required: bool = False,
    clamav_timeout_seconds: float = 8.0,
) -> UploadSecurityReport:
    if not files:
        raise UploadSecurityError("no files supplied")
    if len(files) > max_files:
        raise UploadSecurityError("too many uploaded files")
    names = list(files)
    if len({name.casefold() for name in names}) != len(names):
        raise UploadSecurityError("duplicate filenames are not accepted")

    report = UploadSecurityReport(files_scanned=len(files))
    for filename, data in files.items():
        _validate_filename(filename, max_filename_length)
        _validate_signature(filename, data)
        report.bytes_scanned += len(data)
        report.findings.extend(_secret_findings(filename, data))
        if _suffix(filename) == ".zip":
            _scan_zip(
                filename,
                data,
                report,
                max_archive_members=max_archive_members,
                max_archive_member_bytes=max_archive_member_bytes,
                max_uncompressed_bytes=max_uncompressed_bytes,
                max_compression_ratio=max_compression_ratio,
                max_filename_length=max_filename_length,
            )
        if clamav_host:
            try:
                report.clamav_status = _clamav_scan(clamav_host, clamav_port, data, clamav_timeout_seconds)
            except (OSError, UploadSecurityError) as exc:
                if clamav_required:
                    raise UploadSecurityError(f"malware scan unavailable or failed: {exc}") from exc
                report.clamav_status = "unavailable"
                report.findings.append(SecurityFinding(code="clamav_unavailable", severity="warn", file=filename, detail=str(exc)))
    if block_secrets and any(item.code != "clamav_unavailable" and item.severity == "block" for item in report.findings):
        raise UploadSecurityError("upload contains credential-like material")
    if not report.passed:
        raise UploadSecurityError("upload failed security validation")
    return report
