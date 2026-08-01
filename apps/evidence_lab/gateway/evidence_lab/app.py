from __future__ import annotations

from datetime import datetime, timezone
import hashlib
import io
import json
from pathlib import Path
import secrets
import time
import zipfile
from typing import Annotated, Any

from fastapi import Depends, FastAPI, File, Header, HTTPException, Request, UploadFile
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse, Response
from starlette.middleware.trustedhost import TrustedHostMiddleware

from .backend import BackendError, create_backend
from .bundles import build_reproduction_bundle
from .config import Settings
from .datasets import (
    DatasetError,
    dataset_hash,
    dataset_warnings,
    load_sample_directory,
    load_uploaded_files,
    sample_files,
    validate_limits,
)
from .middleware import RequestSecurityMiddleware, SlidingWindowRateLimitMiddleware
from .models import (
    Dataset,
    Metric,
    PolicyName,
    PublicRunResult,
    RunRequest,
    SessionResponse,
    UploadValidation,
)
from .security import UploadSecurityError, scan_uploads
from .store import FileStore, StoreError


settings = Settings.from_env()
store = FileStore(settings.data_dir, settings.session_ttl_seconds)
backend = create_backend(settings)

app = FastAPI(
    title="GrapheneDB Public Evidence Lab Gateway",
    version="0.2.0",
    description=(
        "Public sandbox gateway for verified samples, bounded structured uploads, "
        "real or explicitly recorded GrapheneDB runs, and reproducibility bundles."
    ),
)
app.add_middleware(TrustedHostMiddleware, allowed_hosts=list(settings.allowed_hosts))
app.add_middleware(
    SlidingWindowRateLimitMiddleware,
    requests=settings.rate_limit_requests,
    window_seconds=settings.rate_limit_window_seconds,
)
app.add_middleware(
    RequestSecurityMiddleware,
    max_content_length=settings.max_upload_bytes + 1024 * 1024,
)
app.add_middleware(
    CORSMiddleware,
    allow_origins=list(settings.allowed_origins),
    allow_credentials=False,
    allow_methods=["GET", "POST", "DELETE", "OPTIONS"],
    allow_headers=["Content-Type", "X-Session-ID", "X-Request-ID"],
    expose_headers=["Content-Disposition", "X-Request-ID"],
)


def session_id_dependency(x_session_id: Annotated[str | None, Header()] = None) -> str:
    if not x_session_id:
        raise HTTPException(status_code=401, detail="X-Session-ID header is required")
    try:
        store._session_dir(x_session_id)
    except StoreError as exc:
        raise HTTPException(status_code=401, detail=str(exc)) from exc
    return x_session_id


def _sample_path(sample_id: str) -> Path:
    if not sample_id or "/" in sample_id or ".." in sample_id:
        raise HTTPException(status_code=400, detail="invalid sample id")
    path = settings.samples_dir / sample_id
    if not path.is_dir():
        raise HTTPException(status_code=404, detail="sample not found")
    return path


def _sample_summary(path: Path) -> dict[str, Any]:
    dataset = load_sample_directory(path)
    return {
        "sample_id": path.name,
        "title": dataset.manifest.title,
        "description": dataset.manifest.description,
        "licence": dataset.manifest.licence,
        "node_count": len(dataset.nodes),
        "edge_count": len(dataset.edges),
        "query_count": len(dataset.queries),
        "evidence_family_count": len({edge.evidence_family_id for edge in dataset.edges}),
        "dataset_hash": dataset_hash(dataset),
        "queries": [query.model_dump(mode="json") for query in dataset.queries],
        "expected_gates": dataset.expected_gates,
        "download_url": f"/v1/public/samples/{path.name}/download",
    }


def _graph(dataset: Dataset, evidence_edges: list[int | str]) -> dict[str, Any]:
    selected = {str(item) for item in evidence_edges}
    return {
        "nodes": [
            {
                "id": node.external_id,
                "label": node.text,
                "role": node.role.value,
                "source_id": node.source_id,
                "evidence_family_id": node.evidence_family_id,
                "derivation_id": node.derivation_id,
            }
            for node in dataset.nodes
        ],
        "edges": [
            {
                "id": edge.edge_id,
                "from": edge.from_node,
                "to": edge.to_node,
                "relation": edge.relation,
                "role": edge.role.value,
                "confidence": edge.confidence,
                "critical": edge.critical,
                "source_id": edge.source_id,
                "evidence_family_id": edge.evidence_family_id,
                "derivation_id": edge.derivation_id,
                "selected": edge.edge_id in selected,
            }
            for edge in dataset.edges
        ],
    }


def _extract_metrics(raw: dict[str, Any], duration_ms: int) -> list[Metric]:
    receipt = raw.get("receipt") or {}
    lyapunov = raw.get("lyapunov") or {}
    observations = lyapunov.get("observations") or []
    return [
        Metric(name="duration", value=duration_ms, unit="ms"),
        Metric(name="recursive_cycles", value=receipt.get("recursive_cycles", max(0, len(observations) - 1))),
        Metric(name="visited_states", value=receipt.get("visited_states")),
        Metric(name="edges_examined", value=receipt.get("edges_examined")),
        Metric(name="deepest_hop", value=receipt.get("deepest_hop")),
        Metric(name="final_energy", value=lyapunov.get("final_energy")),
        Metric(name="stability_total", value=(raw.get("stability") or {}).get("total")),
    ]


def _public_result(
    dataset: Dataset,
    query_id: str,
    policy: PolicyName,
    raw: dict[str, Any],
    events: list[dict[str, Any]],
    started: datetime,
    duration_ms: int,
) -> PublicRunResult:
    query = next(query for query in dataset.queries if query.query_id == query_id)
    receipt = dict(raw.get("receipt") or {})
    live = bool(receipt.get("graphene_executed")) and not bool(receipt.get("recorded_reference"))
    mode = "live_graphenedb" if live else "recorded_reference"
    version_info = raw.get("_evidence_lab_version") or {}
    version = str(version_info.get("server_version") or settings.public_version)
    commit = str(version_info.get("source_commit") or settings.source_commit)
    evidence_edges = list(raw.get("evidence_edges") or [])
    if "content_hash" not in receipt:
        receipt["content_hash"] = hashlib.sha256(
            json.dumps(raw, sort_keys=True, default=str).encode("utf-8")
        ).hexdigest()
    return PublicRunResult(
        run_id=f"run_{secrets.token_urlsafe(18)}",
        run_mode=mode,
        live=live,
        status=str(raw.get("status") or "unknown"),
        primary_node=raw.get("primary_node"),
        confidence=(float(raw["confidence"]) if raw.get("confidence") is not None else None),
        query_id=query.query_id,
        question=query.question,
        policy=policy,
        dataset_id=dataset.manifest.dataset_id,
        dataset_hash=dataset_hash(dataset),
        graphenedb_version=version,
        graphenedb_commit=commit,
        worker_image_digest=raw.get("_evidence_lab_worker_image_digest"),
        started_at=started,
        completed_at=datetime.now(timezone.utc),
        duration_ms=duration_ms,
        metrics=_extract_metrics(raw, duration_ms),
        evidence_edges=evidence_edges,
        graph=_graph(dataset, evidence_edges),
        receipt=receipt,
        residual_uncertainty=list(raw.get("residual_uncertainty") or []),
        execution_events=events,
        raw_engine_result=raw,
        limitations=[
            "Experimental developer alpha; not a semantic truth engine or production decision authority.",
            "Recorded-reference mode is not proof of live database execution."
            if not live
            else "Live run validates this bounded dataset and configuration only.",
        ],
    )


@app.exception_handler(DatasetError)
def dataset_error_handler(request: Request, exc: DatasetError) -> JSONResponse:
    return JSONResponse(
        status_code=422,
        content={"error": "invalid_dataset", "detail": str(exc), "request_id": getattr(request.state, "request_id", None)},
    )


@app.exception_handler(UploadSecurityError)
def security_error_handler(request: Request, exc: UploadSecurityError) -> JSONResponse:
    return JSONResponse(
        status_code=422,
        content={"error": "upload_security_rejection", "detail": str(exc), "request_id": getattr(request.state, "request_id", None)},
    )


@app.exception_handler(BackendError)
def backend_error_handler(request: Request, exc: BackendError) -> JSONResponse:
    detail = str(exc) if not settings.public_mode else "isolated GrapheneDB execution failed"
    return JSONResponse(
        status_code=502,
        content={"error": "backend_failure", "detail": detail, "request_id": getattr(request.state, "request_id", None)},
    )


@app.get("/v1/public/health")
def health() -> dict[str, Any]:
    return {
        "status": "ok",
        "service": "graphenedb-evidence-lab",
        "backend_mode": settings.backend_mode,
        "live_backend_configured": settings.backend_mode in {"live", "subprocess", "kubernetes"},
        "security_profile": "public" if settings.public_mode else "development",
        "malware_scan_required": settings.clamav_required,
    }


@app.post("/v1/public/sessions", response_model=SessionResponse)
def create_session() -> SessionResponse:
    store.cleanup_expired()
    session_id, expires_at = store.create_session()
    return SessionResponse(
        session_id=session_id,
        expires_at=expires_at,
        retention_seconds=settings.session_ttl_seconds,
    )


@app.delete("/v1/public/sessions/{session_id}", status_code=204)
def delete_session(session_id: str, current: str = Depends(session_id_dependency)) -> Response:
    if current != session_id:
        raise HTTPException(status_code=403, detail="session id does not match authentication header")
    store.delete_session(session_id)
    return Response(status_code=204)


@app.get("/v1/public/samples")
def list_samples() -> dict[str, Any]:
    samples = []
    if settings.samples_dir.exists():
        for path in sorted(settings.samples_dir.iterdir()):
            if path.is_dir():
                try:
                    samples.append(_sample_summary(path))
                except DatasetError as exc:
                    samples.append({"sample_id": path.name, "invalid": True, "error": str(exc)})
    return {"samples": samples}


@app.get("/v1/public/samples/{sample_id}")
def get_sample(sample_id: str) -> dict[str, Any]:
    path = _sample_path(sample_id)
    dataset = load_sample_directory(path)
    return {
        **_sample_summary(path),
        "dataset": dataset.model_dump(mode="json"),
        "warnings": dataset_warnings(dataset),
    }


@app.get("/v1/public/samples/{sample_id}/download")
def download_sample(sample_id: str) -> Response:
    path = _sample_path(sample_id)
    output = io.BytesIO()
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for file_path in sample_files(path):
            archive.write(file_path, arcname=file_path.name)
    return Response(
        output.getvalue(),
        media_type="application/zip",
        headers={"Content-Disposition": f'attachment; filename="{sample_id}.zip"'},
    )


@app.post("/v1/public/uploads", response_model=UploadValidation)
async def upload_dataset(
    files: Annotated[list[UploadFile], File(description="One canonical JSON/NDJSON/ZIP or a nodes+edges+queries file set")],
    session_id: str = Depends(session_id_dependency),
) -> UploadValidation:
    source_files: dict[str, bytes] = {}
    total = 0
    for upload in files:
        if not upload.filename:
            raise HTTPException(status_code=400, detail="every upload requires a filename")
        filename = Path(upload.filename).name
        if filename.casefold() in {name.casefold() for name in source_files}:
            raise HTTPException(status_code=400, detail="duplicate filenames are not accepted")
        data = await upload.read(settings.max_upload_bytes + 1)
        total += len(data)
        if len(data) > settings.max_upload_bytes or total > settings.max_upload_bytes:
            raise HTTPException(status_code=413, detail="upload exceeds public size limit")
        source_files[filename] = data

    security = scan_uploads(
        source_files,
        max_files=settings.max_upload_files,
        max_filename_length=settings.max_filename_length,
        max_archive_members=settings.max_archive_members,
        max_archive_member_bytes=settings.max_archive_member_bytes,
        max_uncompressed_bytes=settings.max_archive_uncompressed_bytes,
        max_compression_ratio=settings.max_archive_compression_ratio,
        block_secrets=settings.block_secrets,
        clamav_host=settings.clamav_host,
        clamav_port=settings.clamav_port,
        clamav_required=settings.clamav_required,
        clamav_timeout_seconds=settings.clamav_timeout_seconds,
    )
    dataset = load_uploaded_files(source_files, max_uncompressed=settings.max_archive_uncompressed_bytes)
    validate_limits(dataset, settings.max_nodes, settings.max_edges, settings.max_queries)
    stored_id = store.save_dataset(session_id, dataset, source_files)
    return UploadValidation(
        dataset_id=stored_id,
        dataset_hash=dataset_hash(dataset),
        source_files=sorted(source_files),
        node_count=len(dataset.nodes),
        edge_count=len(dataset.edges),
        query_count=len(dataset.queries),
        evidence_family_count=len({edge.evidence_family_id for edge in dataset.edges}),
        derivation_count=len({edge.derivation_id for edge in dataset.edges}),
        warnings=dataset_warnings(dataset),
        security=security.to_dict(),
        executable=True,
    )


@app.get("/v1/public/datasets/{dataset_id}")
def get_dataset(dataset_id: str, session_id: str = Depends(session_id_dependency)) -> dict[str, Any]:
    try:
        dataset = store.load_dataset(session_id, dataset_id)
    except StoreError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc
    return {
        "dataset_id": dataset_id,
        "dataset_hash": dataset_hash(dataset),
        "dataset": dataset.model_dump(mode="json"),
        "warnings": dataset_warnings(dataset),
    }


@app.post("/v1/public/runs", response_model=PublicRunResult)
def create_run(request: RunRequest, session_id: str = Depends(session_id_dependency)) -> PublicRunResult:
    if request.sample_id:
        dataset = load_sample_directory(_sample_path(request.sample_id))
    else:
        try:
            dataset = store.load_dataset(session_id, request.dataset_id or "")
        except StoreError as exc:
            raise HTTPException(status_code=404, detail=str(exc)) from exc
    query = next((item for item in dataset.queries if item.query_id == request.query_id), None)
    if query is None:
        raise HTTPException(status_code=404, detail="query not found in dataset")
    started = datetime.now(timezone.utc)
    start_clock = time.perf_counter()
    raw, events = backend.run(dataset, query, request.policy)
    duration_ms = max(0, int((time.perf_counter() - start_clock) * 1000))
    result = _public_result(dataset, query.query_id, request.policy, raw, events, started, duration_ms)
    bundle = build_reproduction_bundle(dataset, result)
    store.save_run(session_id, result, bundle)
    return result


@app.get("/v1/public/runs/{run_id}", response_model=PublicRunResult)
def get_run(run_id: str, session_id: str = Depends(session_id_dependency)) -> PublicRunResult:
    try:
        return store.load_run(session_id, run_id)
    except StoreError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc


@app.get("/v1/public/runs/{run_id}/bundle")
def get_run_bundle(run_id: str, session_id: str = Depends(session_id_dependency)) -> Response:
    try:
        bundle = store.load_bundle(session_id, run_id)
    except StoreError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc
    return Response(
        bundle,
        media_type="application/zip",
        headers={"Content-Disposition": f'attachment; filename="graphenedb-{run_id}.zip"'},
    )
