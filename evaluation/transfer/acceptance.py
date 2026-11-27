#!/usr/bin/env python3
"""Run existing native contracts from a clean checkout; preserve evidence."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]
BASE = "9a6f8266e650867133d9fc9b424923274adcbc6c"

def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()

def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True).strip()

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--jobs", type=int, default=2)
    a = p.parse_args()
    if not 1 <= a.jobs <= 16:
        p.error("jobs must be 1..16")
    out = a.out.resolve()
    if out == ROOT or ROOT in out.parents:
        p.error("output must be outside source tree")
    if git("status", "--porcelain", "--untracked-files=normal"):
        p.error("use a clean checkout")
    if subprocess.run(["git", "merge-base", "--is-ancestor", BASE, "HEAD"], cwd=ROOT).returncode:
        p.error("source baseline is not an ancestor; use full clone")
    changed = git("diff", "--name-only", BASE, "HEAD").splitlines()
    allowed = lambda name: name.startswith("evaluation/transfer/") or name == ".github/workflows/transfer-acceptance.yml"
    if not all(allowed(name) for name in changed):
        p.error("core or other source changes detected; review and explicitly re-pin first")
    out.mkdir(parents=True, exist_ok=False)
    build = out / "build"
    install = out / "install"
    consumer = out / "consumer"
    summary = {"source_commit": git("rev-parse", "HEAD"), "baseline": BASE,
               "scope": "engineering acceptance only; not scientific benchmark", "steps": []}
    def save():
        (out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    def run(name, args, requires=True):
        record = {"name": name, "command": [str(x) for x in args]}
        summary["steps"].append(record)
        if not requires:
            record["status"] = "SKIP"
            save()
            return False
        start = time.monotonic()
        try:
            with (out / (name + ".log")).open("w") as log:
                result = subprocess.run(record["command"], cwd=ROOT, stdout=log,
                                        stderr=subprocess.STDOUT, timeout=3600,
                                        env={**os.environ, "PYTHONUNBUFFERED": "1"})
            record["exit_code"] = result.returncode
            record["status"] = "PASS" if result.returncode == 0 else "FAIL"
        except (OSError, subprocess.TimeoutExpired) as e:
            record.update(status="FAIL", error=str(e))
        record["seconds"] = time.monotonic() - start
        save()
        print(name, record["status"], flush=True)
        return record["status"] == "PASS"
    run("compiler", ["c++", "--version"])
    run("cmake-version", ["cmake", "--version"])
    run("python-version", ["python3", "--version"])
    run("platform", ["uname", "-a"])
    run("scorer-self-test", ["python3", ROOT / "evaluation/transfer/score.py", "--self-test"])
    configured = run("configure", ["cmake", "-S", ROOT, "-B", build,
        "-DCMAKE_BUILD_TYPE=Release", "-DGRAPHENEDB_BUILD_TESTS=ON",
        "-DGRAPHENEDB_BUILD_SERVER=ON", "-DGRAPHENEDB_BUILD_EXAMPLES=ON",
        "-DGRAPHENEDB_BUILD_BENCH=OFF"])
    built = run("build", ["cmake", "--build", build, "-j", a.jobs], configured)
    run("ctest", ["ctest", "--test-dir", build, "--output-on-failure", "--no-tests=error", "-j", a.jobs], built)
    binary = build / "moolbase_customer_showcase"
    server = build / "graphenedb_server"
    run("native-lifecycle", ["python3", ROOT / "examples/customer_showcase/verify.py", binary], built)
    run("native-invalid", ["python3", ROOT / "examples/customer_showcase/verify_cli.py", binary], built)
    run("http-restart", ["python3", ROOT / "examples/customer_showcase/python_http.py",
        "--server", server, "--output", out / "http-receipts.json"], built)
    run("http-contract", ["python3", ROOT / "scripts/server_pilot_contract_test.py", server], built)
    installed = run("install", ["cmake", "--install", build, "--prefix", install], built)
    consumer_config = run("consumer-configure", ["cmake", "-S", ROOT / "tests/package_consumer",
        "-B", consumer, "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_PREFIX_PATH=" + str(install)], installed)
    consumer_built = run("consumer-build", ["cmake", "--build", consumer, "-j", a.jobs], consumer_config)
    run("consumer-run", [consumer / "graphenedb_package_consumer"], consumer_built)
    summary["binaries"] = {str(f.relative_to(out)): digest(f) for f in (binary, server) if f.is_file()}
    summary["accepted"] = all(s["status"] == "PASS" for s in summary["steps"])
    save()
    # Evidence manifest includes logs and receipts; excludes build/install trees.
    files = sorted(f for f in out.iterdir() if f.is_file() and f.name != "SHA256SUMS")
    (out / "SHA256SUMS").write_text("".join(f"{digest(f)}  {f.name}\n" for f in files))
    return 0 if summary["accepted"] else 1

if __name__ == "__main__":
    raise SystemExit(main())
