#!/usr/bin/env python3
import json
import pathlib
import re
import sys

root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else pathlib.Path(__file__).resolve().parents[1])
dockerfile = (root / "Dockerfile").read_text()
compose = (root / "docker-compose.secure.yml").read_text()
ignore = (root / ".dockerignore").read_text()

checks = {
    "multi_stage": dockerfile.upper().count("FROM ") >= 2,
    "non_root_user": bool(re.search(r"(?m)^USER\s+(?!root\b)(?:\d+|graphenedb)", dockerfile)),
    "no_baked_api_key": "ENV GRAPHENEDB_API_KEY" not in dockerfile and "--api-key" not in dockerfile,
    "binary_hardening": all(x in dockerfile for x in ["-fstack-protector-strong", "-D_FORTIFY_SOURCE=2", "-Wl,-z,relro", "-z,now"]),
    "healthcheck": "HEALTHCHECK" in dockerfile and "graphenedb_healthcheck" in dockerfile,
    "minimal_apt": "--no-install-recommends" in dockerfile and "rm -rf /var/lib/apt/lists" in dockerfile,
    "compose_read_only": "read_only: true" in compose,
    "compose_drop_caps": re.search(r"cap_drop:\s*\n\s*- ALL", compose) is not None,
    "compose_no_new_privileges": "no-new-privileges:true" in compose,
    "compose_pid_limit": "pids_limit:" in compose,
    "db_port_not_published": "\n    ports:" not in compose.split("\n  caddy:", 1)[0],
    "caddy_hostname_env": "GRAPHENEDB_HOST: ${GRAPHENEDB_HOST:?Set GRAPHENEDB_HOST}" in compose,
    "dockerignore_builds": "build*" in ignore,
}
result = {"ok": all(checks.values()), "checks": checks, "note": "Static structural validation only; release CI must also perform an up-to-date CVE scan and pin approved base-image digests."}
print(json.dumps(result, indent=2, sort_keys=True))
sys.exit(0 if result["ok"] else 1)
