from __future__ import annotations

from datetime import datetime, timezone
from enum import Enum
from typing import Any, Literal

from pydantic import BaseModel, ConfigDict, Field, model_validator


class NodeRole(str, Enum):
    node = "node"
    root = "root"
    symptom = "symptom"
    impact = "impact"


class EdgeRole(str, Enum):
    mechanistic = "mechanistic"
    compressed = "compressed"
    analogical = "analogical"
    predictive = "predictive"
    causal = "causal"
    contradicts = "contradicts"
    supports = "supports"
    supersedes = "supersedes"


class Node(BaseModel):
    model_config = ConfigDict(extra="forbid")

    external_id: str = Field(min_length=1, max_length=4096)
    text: str = Field(min_length=1, max_length=1_048_576)
    role: NodeRole = NodeRole.node
    source_id: str = Field(default="dataset", min_length=1, max_length=4096)
    evidence_family_id: str = Field(default="unspecified", max_length=4096)
    derivation_id: str = Field(default="raw", max_length=4096)
    observed_at: datetime | None = None
    valid_from: datetime | None = None
    valid_to: datetime | None = None
    metadata: dict[str, str] = Field(default_factory=dict)

    @model_validator(mode="after")
    def validate_temporal_range(self) -> "Node":
        if self.valid_from and self.valid_to and self.valid_from > self.valid_to:
            raise ValueError("valid_from must not be after valid_to")
        return self


class Edge(BaseModel):
    model_config = ConfigDict(extra="forbid")

    edge_id: str = Field(min_length=1, max_length=4096)
    from_node: str = Field(min_length=1, max_length=4096)
    to_node: str = Field(min_length=1, max_length=4096)
    relation: str = Field(default="supports", min_length=1, max_length=256)
    role: EdgeRole = EdgeRole.supports
    confidence: float = Field(default=0.9, ge=0.0, le=1.0)
    critical: bool = False
    source_id: str = Field(default="dataset", min_length=1, max_length=4096)
    evidence_id: str = Field(default="", max_length=4096)
    evidence_family_id: str = Field(default="unspecified", max_length=4096)
    derivation_id: str = Field(default="raw", max_length=4096)
    evidence_text: str = Field(default="", max_length=1_048_576)
    metadata: dict[str, str] = Field(default_factory=dict)


class Query(BaseModel):
    model_config = ConfigDict(extra="forbid")

    query_id: str = Field(min_length=1, max_length=256)
    question: str = Field(min_length=1, max_length=65_536)
    target_node: str | None = Field(default=None, max_length=4096)
    mode: Literal["empirical", "balanced", "theoretical"] = "empirical"


class Manifest(BaseModel):
    model_config = ConfigDict(extra="allow")

    schema_version: int = 1
    dataset_id: str = Field(min_length=1, max_length=256)
    title: str = Field(min_length=1, max_length=1024)
    description: str = Field(default="", max_length=8192)
    licence: str = Field(default="user-supplied")
    created_at: datetime = Field(default_factory=lambda: datetime.now(timezone.utc))
    generator: str = Field(default="upload")
    node_count: int | None = Field(default=None, ge=0)
    edge_count: int | None = Field(default=None, ge=0)
    query_count: int | None = Field(default=None, ge=0)


class Dataset(BaseModel):
    model_config = ConfigDict(extra="forbid")

    manifest: Manifest
    nodes: list[Node]
    edges: list[Edge]
    queries: list[Query]
    expected_gates: dict[str, Any] = Field(default_factory=dict)

    @model_validator(mode="after")
    def validate_references(self) -> "Dataset":
        node_ids = [node.external_id for node in self.nodes]
        if len(node_ids) != len(set(node_ids)):
            raise ValueError("node external_id values must be unique")
        edge_ids = [edge.edge_id for edge in self.edges]
        if len(edge_ids) != len(set(edge_ids)):
            raise ValueError("edge_id values must be unique")
        known = set(node_ids)
        missing: list[str] = []
        for edge in self.edges:
            if edge.from_node not in known:
                missing.append(f"edge {edge.edge_id} from_node={edge.from_node}")
            if edge.to_node not in known:
                missing.append(f"edge {edge.edge_id} to_node={edge.to_node}")
        for query in self.queries:
            if query.target_node and query.target_node not in known:
                missing.append(f"query {query.query_id} target_node={query.target_node}")
        if missing:
            raise ValueError("unresolved references: " + "; ".join(missing[:20]))
        if not self.queries:
            raise ValueError("at least one query is required")
        return self


class PolicyName(str, Enum):
    one_pass = "one_pass"
    frontier_aware = "frontier_aware"
    broad_expansion = "broad_expansion"
    previous_stop_reference = "previous_stop_reference"


class SessionResponse(BaseModel):
    session_id: str
    expires_at: datetime
    retention_seconds: int


class UploadValidation(BaseModel):
    dataset_id: str
    dataset_hash: str
    source_files: list[str]
    node_count: int
    edge_count: int
    query_count: int
    evidence_family_count: int
    derivation_count: int
    warnings: list[str]
    security: dict[str, Any] = Field(default_factory=dict)
    executable: bool


class RunRequest(BaseModel):
    sample_id: str | None = None
    dataset_id: str | None = None
    query_id: str
    policy: PolicyName = PolicyName.frontier_aware

    @model_validator(mode="after")
    def exactly_one_dataset_source(self) -> "RunRequest":
        if bool(self.sample_id) == bool(self.dataset_id):
            raise ValueError("provide exactly one of sample_id or dataset_id")
        return self


class Metric(BaseModel):
    name: str
    value: int | float | str | bool | None
    unit: str | None = None


class PublicRunResult(BaseModel):
    run_id: str
    run_mode: Literal["live_graphenedb", "recorded_reference"]
    live: bool
    status: str
    primary_node: int | str | None
    confidence: float | None
    query_id: str
    question: str
    policy: PolicyName
    dataset_id: str
    dataset_hash: str
    graphenedb_version: str
    graphenedb_commit: str
    worker_image_digest: str | None = None
    started_at: datetime
    completed_at: datetime
    duration_ms: int
    metrics: list[Metric]
    evidence_edges: list[int | str]
    graph: dict[str, Any]
    receipt: dict[str, Any]
    residual_uncertainty: list[str]
    execution_events: list[dict[str, Any]]
    raw_engine_result: dict[str, Any] = Field(default_factory=dict)
    limitations: list[str] = Field(default_factory=list)


class ErrorResponse(BaseModel):
    error: str
    detail: str
    request_id: str | None = None
