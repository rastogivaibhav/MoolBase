#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import io
import json
import sys
import zipfile

import httpx


def fail(message: str) -> None:
    raise RuntimeError(message)


def verify_bundle(data: bytes, expected_run_id: str) -> dict:
    with zipfile.ZipFile(io.BytesIO(data)) as archive:
        names = set(archive.namelist())
        required = {
            "normalised/dataset.json",
            "execution/configuration.json",
            "execution/events.json",
            "execution/raw-engine-result.json",
            "result/public-result.json",
            "result/compact-receipt.json",
            "result/evidence-graph.json",
            "README.md",
            "SHA256SUMS",
        }
        missing = required - names
        if missing:
            fail("bundle missing files: " + ", ".join(sorted(missing)))
        expected = {}
        for line in archive.read("SHA256SUMS").decode("utf-8").splitlines():
            digest, path = line.split("  ", 1)
            expected[path] = digest
        for path, digest in expected.items():
            actual = hashlib.sha256(archive.read(path)).hexdigest()
            if actual != digest:
                fail(f"bundle checksum mismatch: {path}")
        result = json.loads(archive.read("result/public-result.json"))
        if result["run_id"] != expected_run_id:
            fail("bundle run id does not match API result")
        return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("base_url")
    parser.add_argument("--expected-commit")
    parser.add_argument("--timeout", type=float, default=180.0)
    args = parser.parse_args()
    base = args.base_url.rstrip("/")

    with httpx.Client(base_url=base, timeout=args.timeout, follow_redirects=True) as client:
        health = client.get("/v1/public/health")
        health.raise_for_status()
        health_body = health.json()
        if not health_body.get("live_backend_configured"):
            fail("gateway does not report a live backend")
        session = client.post("/v1/public/sessions")
        session.raise_for_status()
        session_id = session.json()["session_id"]
        headers = {"X-Session-ID": session_id}
        results = []
        try:
            samples_response = client.get("/v1/public/samples")
            samples_response.raise_for_status()
            samples = [item for item in samples_response.json()["samples"] if not item.get("invalid")]
            if len(samples) < 2:
                fail("at least two valid public samples are required")

            first_dataset = None
            for sample in samples[:2]:
                detail_response = client.get(f"/v1/public/samples/{sample['sample_id']}")
                detail_response.raise_for_status()
                detail = detail_response.json()
                first_dataset = first_dataset or detail["dataset"]
                query_id = detail["dataset"]["queries"][0]["query_id"]
                run_response = client.post(
                    "/v1/public/runs",
                    headers=headers,
                    json={
                        "sample_id": sample["sample_id"],
                        "query_id": query_id,
                        "policy": "frontier_aware",
                    },
                )
                run_response.raise_for_status()
                run = run_response.json()
                if not run.get("live") or run.get("run_mode") != "live_graphenedb":
                    fail(f"sample {sample['sample_id']} did not execute live GrapheneDB")
                if not run.get("receipt", {}).get("graphene_executed"):
                    fail("live run receipt does not confirm GrapheneDB execution")
                if args.expected_commit and run.get("graphenedb_commit") != args.expected_commit:
                    fail(
                        f"commit mismatch: expected {args.expected_commit}, got {run.get('graphenedb_commit')}"
                    )
                bundle_response = client.get(
                    f"/v1/public/runs/{run['run_id']}/bundle",
                    headers=headers,
                )
                bundle_response.raise_for_status()
                verify_bundle(bundle_response.content, run["run_id"])
                results.append(
                    {
                        "source": sample["sample_id"],
                        "run_id": run["run_id"],
                        "status": run["status"],
                        "commit": run["graphenedb_commit"],
                        "bundle_bytes": len(bundle_response.content),
                    }
                )

            if first_dataset is None:
                fail("no sample dataset available for upload validation")
            upload_response = client.post(
                "/v1/public/uploads",
                headers=headers,
                files={
                    "files": (
                        "partner-upload.json",
                        json.dumps(first_dataset, sort_keys=True).encode("utf-8"),
                        "application/json",
                    )
                },
            )
            upload_response.raise_for_status()
            upload = upload_response.json()
            if not upload.get("security", {}).get("passed"):
                fail("uploaded dataset did not pass the public security scan")
            uploaded_detail = client.get(
                f"/v1/public/datasets/{upload['dataset_id']}",
                headers=headers,
            )
            uploaded_detail.raise_for_status()
            uploaded_query = uploaded_detail.json()["dataset"]["queries"][0]["query_id"]
            uploaded_run_response = client.post(
                "/v1/public/runs",
                headers=headers,
                json={
                    "dataset_id": upload["dataset_id"],
                    "query_id": uploaded_query,
                    "policy": "frontier_aware",
                },
            )
            uploaded_run_response.raise_for_status()
            uploaded_run = uploaded_run_response.json()
            if not uploaded_run.get("live"):
                fail("uploaded dataset did not execute live GrapheneDB")
            uploaded_bundle = client.get(
                f"/v1/public/runs/{uploaded_run['run_id']}/bundle",
                headers=headers,
            )
            uploaded_bundle.raise_for_status()
            verify_bundle(uploaded_bundle.content, uploaded_run["run_id"])
            results.append(
                {
                    "source": "uploaded-dataset",
                    "run_id": uploaded_run["run_id"],
                    "status": uploaded_run["status"],
                    "commit": uploaded_run["graphenedb_commit"],
                    "bundle_bytes": len(uploaded_bundle.content),
                }
            )
        finally:
            client.delete(f"/v1/public/sessions/{session_id}", headers=headers)

    print(
        json.dumps(
            {
                "evidence_lab_live_validation": "PASS",
                "base_url": base,
                "health": health_body,
                "runs": results,
            },
            indent=2,
            sort_keys=True,
        )
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"EVIDENCE_LAB_LIVE_VALIDATION_FAILED: {exc}", file=sys.stderr)
        raise
