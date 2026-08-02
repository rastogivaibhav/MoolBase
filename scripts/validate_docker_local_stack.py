#!/usr/bin/env python3
from __future__ import annotations

import json
import pathlib
import re
import sys

root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else pathlib.Path(__file__).resolve().parents[1])
proof = (root / "Dockerfile.proof").read_text(encoding="utf-8")
compose = (root / "docker-compose.local.yml").read_text(encoding="utf-8")
runner = (root / "scripts/docker/run_proof_bundle.sh").read_text(encoding="utf-8")
remote = (root / "scripts/docker/remote_runtime_smoke.py").read_text(encoding="utf-8")

checks = {
    "proof_multistage": proof.upper().count("FROM ") >= 2,
    "proof_pinned_source_identity": "SOURCE_COMMIT" in proof and "GRAPHENEDB_SOURCE_COMMIT" in proof,
    "proof_no_baked_api_key": "ENV GRAPHENEDB_API_KEY" not in proof,
    "compose_server_localhost_only": re.search(r'127\.0\.0\.1:\$\{GRAPHENEDB_PORT', compose) is not None,
    "compose_server_read_only": "read_only: true" in compose,
    "compose_drop_all_caps": len(re.findall(r"cap_drop:\s*\n\s*- ALL", compose)) >= 2,
    "compose_no_new_privileges": compose.count("no-new-privileges:true") >= 2,
    "compose_resource_limits": all(value in compose for value in ("pids_limit:", "mem_limit:", "cpus:")),
    "compose_evidence_mount": "./docker-evidence:/evidence" in compose,
    "runner_modes": all(mode in runner for mode in ("smoke|full|security|public|api-smoke", "run_smoke", "run_full", "run_security", "run_public")),
    "runner_exact_commit_override": "GRAPHENEDB_SOURCE_COMMIT_OVERRIDE" in runner,
    "runner_checksums": "checksums.sha256" in runner and "sha256sum" in runner,
    "runner_archive": "graphenedb-proof-" in runner and "tar.gz" in runner,
    "api_contract_determinism": "deterministic_repeat" in remote and "final_bundle_hash" in remote,
    "api_contract_no_silent_promotion": "no_silent_promotion" in remote,
}

result = {
    "ok": all(checks.values()),
    "checks": checks,
    "note": "Static structural validation only. A real Docker daemon must build and run the stack before release claims are made.",
}
print(json.dumps(result, indent=2, sort_keys=True))
raise SystemExit(0 if result["ok"] else 1)
