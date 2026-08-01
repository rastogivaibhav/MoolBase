#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path
import re
import sys
from urllib.parse import urlparse


REQUIRED_MARKERS = (
    "kind: Namespace",
    "pod-security.kubernetes.io/enforce: restricted",
    "kind: Role",
    "resources: [\"jobs\"]",
    "kind: PersistentVolumeClaim",
    "accessModes: [\"ReadWriteMany\"]",
    "EVIDENCE_LAB_BACKEND: kubernetes",
    "EVIDENCE_LAB_CLAMAV_REQUIRED: \"true\"",
    "EVIDENCE_LAB_KUBERNETES_IMAGE_PULL_SECRET: ghcr-pull",
    "automountServiceAccountToken: false",
    "readOnlyRootFilesystem: true",
    "allowPrivilegeEscalation: false",
    "drop: [\"ALL\"]",
    "kind: NetworkPolicy",
    "evidence-lab-worker-deny-all",
    "ingress: []",
    "egress: []",
    "kind: Ingress",
)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest", type=Path)
    parser.add_argument("--hostname", required=True)
    parser.add_argument("--site-origin", required=True)
    parser.add_argument("--expected-commit", required=True)
    args = parser.parse_args()

    problems: list[str] = []
    text = args.manifest.read_text("utf-8")
    hostname = args.hostname.strip().lower()
    site = urlparse(args.site_origin)

    if not re.fullmatch(r"[a-z0-9](?:[a-z0-9.-]{0,251}[a-z0-9])?", hostname):
        problems.append("invalid public hostname")
    if hostname.endswith("example.com") or "localhost" in hostname:
        problems.append("placeholder or local public hostname is not deployable")
    if site.scheme != "https" or not site.netloc:
        problems.append("ChatGPT Sites origin must be an absolute HTTPS origin")
    if not re.fullmatch(r"[0-9a-f]{40}", args.expected_commit):
        problems.append("expected commit must be a full lowercase 40-character SHA")
    if "evidence-lab.example.com" in text or "REPLACE_WITH" in text:
        problems.append("rendered Kubernetes manifest still contains placeholders")
    if hostname not in text:
        problems.append("rendered Kubernetes manifest does not contain the public hostname")
    for marker in REQUIRED_MARKERS:
        if marker not in text:
            problems.append(f"missing deployment security marker: {marker}")

    if problems:
        for problem in problems:
            print(f"ERROR: {problem}", file=sys.stderr)
        return 1
    print("EVIDENCE_LAB_DEPLOYMENT_PREFLIGHT=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
