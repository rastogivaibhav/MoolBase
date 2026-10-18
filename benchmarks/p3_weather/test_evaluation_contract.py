#!/usr/bin/env python3
"""Adversarial P3 data integrity and chronological leakage guards."""
import argparse
import csv
import sys
import tempfile
from pathlib import Path

from evaluate import logistic_features, native_rows, source_rows


def fails(callable_, label):
    try:
        callable_()
    except (ValueError, KeyError, IndexError, OSError):
        return
    raise AssertionError(f"integrity check did not reject {label}")


def write_csv(path, header, rows):
    with open(path, "w", newline="", encoding="utf-8") as f:
        out = csv.DictWriter(f, fieldnames=header)
        out.writeheader()
        out.writerows(rows)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--source", required=True)
    p.add_argument("--native", required=True)
    a = p.parse_args()
    source = source_rows(a.source)
    native = native_rows(a.native, source)
    with open(a.native, newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        nheader = reader.fieldnames
        nrows = list(reader)
    with open(a.source, newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        sheader = reader.fieldnames
        srows = list(reader)

    # Derive a forecast x(today) only from calendar date and previous day.
    # Changing today's weather outcome + meteorology cannot alter x(today).
    prev, today = source["Seattle"][200], source["Seattle"][201]
    altered = dict(today)
    altered["precipitation"] = 999
    altered["temp_max"] = -99
    altered["temp_min"] = -120
    altered["wind"] = 900
    assert logistic_features(today, prev) == logistic_features(altered, prev)

    with tempfile.TemporaryDirectory() as directory:
        tmp = Path(directory) / "mutated.csv"
        altered_n = [row.copy() for row in nrows]
        altered_n[100]["p2"] = "0.99123"
        write_csv(tmp, nheader, altered_n)
        fails(lambda: native_rows(tmp, source), "forged native probability")

        altered_n = [row.copy() for row in nrows]
        altered_n[100]["observed"] = str(1 - int(altered_n[100]["observed"]))
        write_csv(tmp, nheader, altered_n)
        fails(lambda: native_rows(tmp, source), "native outcome mismatch")

        altered_n = [row.copy() for row in nrows]
        altered_n.append(altered_n[100].copy())
        write_csv(tmp, nheader, altered_n)
        fails(lambda: native_rows(tmp, source), "duplicate native prediction")

        altered_s = [row.copy() for row in srows]
        altered_s[100]["date"] = altered_s[99]["date"]
        write_csv(tmp, sheader, altered_s)
        fails(lambda: source_rows(tmp), "duplicate source date")

        altered_s = [row.copy() for row in srows]
        altered_s[100]["precipitation"] = "nan"
        write_csv(tmp, sheader, altered_s)
        fails(lambda: source_rows(tmp), "non-finite precipitation")

        altered_s = [row.copy() for row in srows]
        altered_s[100]["weather"] = ""
        write_csv(tmp, sheader, altered_s)
        fails(lambda: source_rows(tmp), "missing source field")
    assert len(native["New York"]) >= 1461
    print("p3_negative_controls=PASS checked=7")


if __name__ == "__main__":
    main()
