#!/usr/bin/env python3
import sys
from pathlib import Path


def load_metrics(path: Path) -> dict[str, float]:
    metrics: dict[str, float] = {}
    raw = path.read_bytes()
    if raw.startswith(b"\xff\xfe") or raw.startswith(b"\xfe\xff") or raw[:200].count(b"\x00") > 20:
        text = raw.decode("utf-16", errors="replace")
    else:
        text = raw.decode("utf-8", errors="replace")
    for line in text.splitlines():
        for token in line.split():
            if "=" not in token:
                continue
            key, value = token.split("=", 1)
            key = key.strip()
            value = value.strip().strip(",")
            if not key:
                continue
            try:
                metrics[key] = float(value)
            except ValueError:
                pass
    return metrics


def parse_rule(rule: str) -> tuple[str, str, float]:
    for op in (">=", "<=", ">", "<", "=="):
        if op in rule:
            key, value = rule.split(op, 1)
            key = key.strip()
            if not key:
                raise ValueError(f"missing metric key in rule: {rule}")
            return key, op, float(value.strip())
    raise ValueError(f"unsupported threshold rule: {rule}")


def passes(actual: float, op: str, expected: float) -> bool:
    if op == ">=":
        return actual >= expected
    if op == "<=":
        return actual <= expected
    if op == ">":
        return actual > expected
    if op == "<":
        return actual < expected
    if op == "==":
        return actual == expected
    raise AssertionError(op)


def main(argv: list[str]) -> int:
    if len(argv) < 3:
        print("usage: check_benchmark_thresholds.py <log> <metric>=<threshold> ...", file=sys.stderr)
        print("example: check_benchmark_thresholds.py bench.log extract_nodes_per_sec>=1000 causal_lattice_p95_ms<=50", file=sys.stderr)
        return 2
    log = Path(argv[1])
    metrics = load_metrics(log)
    failed = False
    raw_rules: list[str] = []
    for arg in argv[2:]:
        raw_rules.extend(part.strip() for part in arg.split(",") if part.strip())
    for raw in raw_rules:
        key, op, expected = parse_rule(raw)
        if key not in metrics:
            print(f"FAIL missing metric {key} for rule {raw}")
            failed = True
            continue
        actual = metrics[key]
        if passes(actual, op, expected):
            print(f"PASS {key} {op} {expected:g} actual={actual:g}")
        else:
            print(f"FAIL {key} {op} {expected:g} actual={actual:g}")
            failed = True
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
