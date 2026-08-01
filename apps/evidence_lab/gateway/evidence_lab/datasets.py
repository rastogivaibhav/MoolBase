from __future__ import annotations

import csv
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import zipfile
from typing import Iterable

from pydantic import ValidationError

from .models import Dataset, Edge, Manifest, Node, Query


class DatasetError(ValueError):
    pass


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def canonical_dataset_bytes(dataset: Dataset) -> bytes:
    return json.dumps(
        dataset.model_dump(mode="json"),
        sort_keys=True,
        separators=(",", ":"),
        ensure_ascii=False,
    ).encode("utf-8")


def dataset_hash(dataset: Dataset) -> str:
    return _sha256(canonical_dataset_bytes(dataset))


def _coerce_bool(value: str | bool | None) -> bool:
    if isinstance(value, bool):
        return value
    return str(value or "").strip().lower() in {"1", "true", "yes", "y"}


def _clean_metadata(row: dict[str, str], known: set[str]) -> dict[str, str]:
    return {key: value for key, value in row.items() if key not in known and value not in (None, "")}


def _parse_nodes(text: str, delimiter: str = ",") -> list[Node]:
    reader = csv.DictReader(io.StringIO(text), delimiter=delimiter)
    required = {"external_id", "text"}
    if not reader.fieldnames or not required.issubset(set(reader.fieldnames)):
        raise DatasetError("nodes file requires external_id and text columns")
    known = {
        "external_id", "text", "role", "source_id", "evidence_family_id",
        "derivation_id", "observed_at", "valid_from", "valid_to",
    }
    output: list[Node] = []
    for row in reader:
        output.append(
            Node(
                external_id=row.get("external_id", "").strip(),
                text=row.get("text", "").strip(),
                role=(row.get("role") or "node").strip(),
                source_id=(row.get("source_id") or "dataset").strip(),
                evidence_family_id=(row.get("evidence_family_id") or "unspecified").strip(),
                derivation_id=(row.get("derivation_id") or "raw").strip(),
                observed_at=(row.get("observed_at") or None),
                valid_from=(row.get("valid_from") or None),
                valid_to=(row.get("valid_to") or None),
                metadata=_clean_metadata(row, known),
            )
        )
    return output


def _parse_edges(text: str, delimiter: str = ",") -> list[Edge]:
    reader = csv.DictReader(io.StringIO(text), delimiter=delimiter)
    required = {"edge_id", "from_node", "to_node"}
    if not reader.fieldnames or not required.issubset(set(reader.fieldnames)):
        raise DatasetError("edges file requires edge_id, from_node and to_node columns")
    known = {
        "edge_id", "from_node", "to_node", "relation", "role", "confidence",
        "critical", "source_id", "evidence_id", "evidence_family_id",
        "derivation_id", "evidence_text",
    }
    output: list[Edge] = []
    for row in reader:
        confidence = float(row.get("confidence") or 0.9)
        output.append(
            Edge(
                edge_id=row.get("edge_id", "").strip(),
                from_node=row.get("from_node", "").strip(),
                to_node=row.get("to_node", "").strip(),
                relation=(row.get("relation") or "supports").strip(),
                role=(row.get("role") or "supports").strip(),
                confidence=confidence,
                critical=_coerce_bool(row.get("critical")),
                source_id=(row.get("source_id") or "dataset").strip(),
                evidence_id=(row.get("evidence_id") or row.get("edge_id") or "").strip(),
                evidence_family_id=(row.get("evidence_family_id") or "unspecified").strip(),
                derivation_id=(row.get("derivation_id") or "raw").strip(),
                evidence_text=(row.get("evidence_text") or "").strip(),
                metadata=_clean_metadata(row, known),
            )
        )
    return output


def _queries_from_json(data: bytes) -> list[Query]:
    raw = json.loads(data.decode("utf-8"))
    if isinstance(raw, dict) and "queries" in raw:
        raw = raw["queries"]
    if not isinstance(raw, list):
        raise DatasetError("queries.json must contain a list or {queries: [...]} object")
    return [Query.model_validate(item) for item in raw]


def load_json_dataset(data: bytes) -> Dataset:
    try:
        raw = json.loads(data.decode("utf-8"))
        return Dataset.model_validate(raw)
    except (UnicodeDecodeError, json.JSONDecodeError, ValidationError) as exc:
        raise DatasetError(f"invalid JSON dataset: {exc}") from exc


def load_ndjson_dataset(data: bytes, filename: str = "upload.ndjson") -> Dataset:
    manifest: dict | None = None
    nodes: list[dict] = []
    edges: list[dict] = []
    queries: list[dict] = []
    expected_gates: dict = {}
    try:
        for line_number, line in enumerate(data.decode("utf-8").splitlines(), start=1):
            if not line.strip():
                continue
            record = json.loads(line)
            record_type = record.pop("record_type", None)
            if record_type == "manifest":
                manifest = record
            elif record_type == "node":
                nodes.append(record)
            elif record_type == "edge":
                edges.append(record)
            elif record_type == "query":
                queries.append(record)
            elif record_type == "expected_gates":
                expected_gates = record
            else:
                raise DatasetError(f"{filename}:{line_number}: unsupported record_type {record_type!r}")
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise DatasetError(f"invalid NDJSON dataset: {exc}") from exc
    if manifest is None:
        manifest = {
            "schema_version": 1,
            "dataset_id": Path(filename).stem,
            "title": Path(filename).stem.replace("-", " ").title(),
            "licence": "user-supplied",
        }
    try:
        return Dataset.model_validate(
            {"manifest": manifest, "nodes": nodes, "edges": edges, "queries": queries,
             "expected_gates": expected_gates}
        )
    except ValidationError as exc:
        raise DatasetError(f"invalid NDJSON records: {exc}") from exc


def _safe_zip_members(archive: zipfile.ZipFile, max_uncompressed: int) -> list[zipfile.ZipInfo]:
    members: list[zipfile.ZipInfo] = []
    total = 0
    for member in archive.infolist():
        path = PurePosixPath(member.filename)
        if path.is_absolute() or ".." in path.parts:
            raise DatasetError(f"unsafe archive path: {member.filename}")
        if member.is_dir() or member.filename.endswith("/"):
            continue
        total += member.file_size
        if total > max_uncompressed:
            raise DatasetError("archive exceeds maximum uncompressed size")
        members.append(member)
    return members


def load_zip_dataset(data: bytes, max_uncompressed: int) -> Dataset:
    try:
        with zipfile.ZipFile(io.BytesIO(data)) as archive:
            members = _safe_zip_members(archive, max_uncompressed)
            by_name = {PurePosixPath(item.filename).name: item for item in members}
            required = {"manifest.json", "nodes.csv", "edges.csv", "queries.json"}
            missing = required - by_name.keys()
            if missing:
                raise DatasetError("dataset ZIP missing: " + ", ".join(sorted(missing)))
            manifest = json.loads(archive.read(by_name["manifest.json"]).decode("utf-8"))
            nodes = _parse_nodes(archive.read(by_name["nodes.csv"]).decode("utf-8"))
            edges = _parse_edges(archive.read(by_name["edges.csv"]).decode("utf-8"))
            queries = _queries_from_json(archive.read(by_name["queries.json"]))
            expected = {}
            if "expected-gates.json" in by_name:
                expected = json.loads(archive.read(by_name["expected-gates.json"]).decode("utf-8"))
            return Dataset(
                manifest=Manifest.model_validate(manifest),
                nodes=nodes,
                edges=edges,
                queries=queries,
                expected_gates=expected,
            )
    except zipfile.BadZipFile as exc:
        raise DatasetError("invalid ZIP archive") from exc
    except (UnicodeDecodeError, json.JSONDecodeError, ValidationError) as exc:
        raise DatasetError(f"invalid dataset ZIP: {exc}") from exc


def load_multifile_dataset(files: dict[str, bytes]) -> Dataset:
    normalized = {PurePosixPath(name).name.lower(): data for name, data in files.items()}
    node_name = next((name for name in ("nodes.csv", "nodes.tsv") if name in normalized), None)
    edge_name = next((name for name in ("edges.csv", "edges.tsv") if name in normalized), None)
    if not node_name or not edge_name or "queries.json" not in normalized:
        raise DatasetError("multi-file upload requires nodes.csv/tsv, edges.csv/tsv and queries.json")
    manifest_data = normalized.get("manifest.json")
    manifest = (
        json.loads(manifest_data.decode("utf-8"))
        if manifest_data
        else {
            "schema_version": 1,
            "dataset_id": "uploaded-dataset",
            "title": "Uploaded dataset",
            "licence": "user-supplied",
        }
    )
    nodes = _parse_nodes(normalized[node_name].decode("utf-8"), "\t" if node_name.endswith(".tsv") else ",")
    edges = _parse_edges(normalized[edge_name].decode("utf-8"), "\t" if edge_name.endswith(".tsv") else ",")
    queries = _queries_from_json(normalized["queries.json"])
    expected = {}
    if "expected-gates.json" in normalized:
        expected = json.loads(normalized["expected-gates.json"].decode("utf-8"))
    return Dataset(
        manifest=Manifest.model_validate(manifest),
        nodes=nodes,
        edges=edges,
        queries=queries,
        expected_gates=expected,
    )


def load_uploaded_files(files: dict[str, bytes], max_uncompressed: int) -> Dataset:
    if not files:
        raise DatasetError("no files supplied")
    if len(files) > 1:
        return load_multifile_dataset(files)
    filename, data = next(iter(files.items()))
    lower = filename.lower()
    if lower.endswith(".zip"):
        return load_zip_dataset(data, max_uncompressed=max_uncompressed)
    if lower.endswith(".json"):
        return load_json_dataset(data)
    if lower.endswith(".ndjson") or lower.endswith(".jsonl"):
        return load_ndjson_dataset(data, filename=filename)
    raise DatasetError("single-file upload must be JSON, NDJSON/JSONL or GrapheneDB dataset ZIP")


def validate_limits(dataset: Dataset, max_nodes: int, max_edges: int, max_queries: int) -> None:
    problems = []
    if len(dataset.nodes) > max_nodes:
        problems.append(f"nodes {len(dataset.nodes)} exceeds limit {max_nodes}")
    if len(dataset.edges) > max_edges:
        problems.append(f"edges {len(dataset.edges)} exceeds limit {max_edges}")
    if len(dataset.queries) > max_queries:
        problems.append(f"queries {len(dataset.queries)} exceeds limit {max_queries}")
    if problems:
        raise DatasetError("; ".join(problems))


def dataset_warnings(dataset: Dataset) -> list[str]:
    warnings: list[str] = []
    evidence_families = {edge.evidence_family_id for edge in dataset.edges}
    if "unspecified" in evidence_families:
        warnings.append("one or more edges have unspecified evidence-family lineage")
    derivations = {edge.derivation_id for edge in dataset.edges}
    if "raw" not in derivations:
        warnings.append("no edge is explicitly identified as a raw derivation")
    if not any(edge.critical for edge in dataset.edges):
        warnings.append("no critical evidence edge is identified")
    if not any(edge.role == "contradicts" for edge in dataset.edges):
        warnings.append("dataset contains no explicit contradiction edge")
    return warnings


def load_sample_directory(path: Path) -> Dataset:
    required = [path / "manifest.json", path / "nodes.csv", path / "edges.csv", path / "queries.json"]
    missing = [item.name for item in required if not item.exists()]
    if missing:
        raise DatasetError(f"sample {path.name} missing files: {', '.join(missing)}")
    expected = path / "expected-gates.json"
    return Dataset(
        manifest=Manifest.model_validate_json((path / "manifest.json").read_text("utf-8")),
        nodes=_parse_nodes((path / "nodes.csv").read_text("utf-8")),
        edges=_parse_edges((path / "edges.csv").read_text("utf-8")),
        queries=_queries_from_json((path / "queries.json").read_bytes()),
        expected_gates=json.loads(expected.read_text("utf-8")) if expected.exists() else {},
    )


def sample_files(path: Path) -> Iterable[Path]:
    for name in ("README.md", "manifest.json", "nodes.csv", "edges.csv", "queries.json", "expected-gates.json", "SHA256SUMS"):
        candidate = path / name
        if candidate.exists():
            yield candidate
