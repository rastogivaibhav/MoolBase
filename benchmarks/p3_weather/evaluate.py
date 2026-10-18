#!/usr/bin/env python3
"""Frozen P3 retrospective chronological evaluation; Python 3 stdlib only.

P1 and P2 forecasts MUST come from the native C++ model-world runner.
No use of the synthetic 'weather' label or present-day climate variables.
"""
import argparse
import csv
import datetime as dt
import hashlib
import json
import math
import random
import statistics
from collections import defaultdict, deque
from pathlib import Path

SEED = 20261018
BLOCK = 14
BOOTSTRAPS = 5000
LOCATIONS = ("Seattle", "New York")
HEADER = ["location", "date", "precipitation", "temp_max", "temp_min", "wind", "weather"]
NATIVE_HEADER = ["location", "date", "observed", "p1", "p2"]


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def logistic_features(today, yesterday):
    """Use only yesterday's observations and today's known calendar."""
    d = today["date"]
    day_of_year = d.timetuple().tm_yday
    angle = 2 * math.pi * day_of_year / 365.25
    return [
        1.0,
        1.0 if yesterday["precipitation"] > 0.0 else 0.0,
        yesterday["temp_max"] / 40.0,
        yesterday["temp_min"] / 40.0,
        yesterday["wind"] / 15.0,
        math.sin(angle),
        math.cos(angle),
    ]


def source_rows(path):
    records = defaultdict(list)
    with open(path, encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle)
        if reader.fieldnames != HEADER:
            raise ValueError(f"wrong dataset schema: {reader.fieldnames}")
        for raw in reader:
            if None in raw or any(v is None or v == "" for v in raw.values()):
                raise ValueError("missing source field")
            if raw["location"] not in LOCATIONS:
                raise ValueError("unknown geographic source")
            date = dt.date.fromisoformat(raw["date"])
            fields = {name: float(raw[name]) for name in HEADER[2:-1]}
            if not all(math.isfinite(v) for v in fields.values()):
                raise ValueError("nonfinite meteorological value")
            if (fields["precipitation"] < 0 or fields["wind"] < 0 or
                    fields["temp_max"] < fields["temp_min"]):
                raise ValueError("invalid weather observation")
            records[raw["location"]].append(dict(location=raw["location"], date=date, **fields))
    if set(records) != set(LOCATIONS):
        raise ValueError("missing location")
    for city, entries in records.items():
        if len(entries) != 1461:
            raise ValueError(f"{city}: expected 1461 dates; got {len(entries)}")
        if entries[0]["date"] != dt.date(2012, 1, 1) or entries[-1]["date"] != dt.date(2015, 12, 31):
            raise ValueError("unexpected source date boundaries")
        for a, b in zip(entries, entries[1:]):
            if b["date"] != a["date"] + dt.timedelta(days=1):
                raise ValueError(f"{city}: duplicate or missing date")
    return records


def native_rows(path, source):
    native = defaultdict(dict)
    with open(path, encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle)
        if reader.fieldnames != NATIVE_HEADER:
            raise ValueError("unexpected native runner schema")
        for row in reader:
            city = row["location"]
            date = dt.date.fromisoformat(row["date"])
            if city not in LOCATIONS or date in native[city]:
                raise ValueError("unknown / repeated native forecast")
            y = int(row["observed"])
            if y not in (0, 1) or str(y) != row["observed"]:
                raise ValueError("invalid native outcome label")
            p, q = float(row["p1"]), float(row["p2"])
            if not all(math.isfinite(v) and 0 <= v <= 1 for v in (p, q)):
                raise ValueError("invalid native probability")
            native[city][date] = (y, p, q)
    for city in LOCATIONS:
        entries = source[city]
        if set(native[city]) != {entry["date"] for entry in entries}:
            raise ValueError("native/source date alignment mismatch")
        # Parity guard: native P1 lifetime Beta forecast must reproduce the
        # closed-form Bayesian update using *past* labels only.
        positive = 0
        alpha = beta = 1.0
        streak = last_surprise = 0
        changes = 0
        for i, entry in enumerate(entries):
            y, native_p1, native_p2 = native[city][entry["date"]]
            if y != int(entry["precipitation"] > 0):
                raise ValueError(f"native/source outcome mismatch: {city} {entry['date']}")
            expected = (positive + 1) / (i + 2)
            expected_p2 = alpha / (alpha + beta)
            if abs(native_p1 - expected) > 1e-11:
                raise ValueError(f"P1 native parity violation: {city} {entry['date']}")
            if abs(native_p2 - expected_p2) > 1e-11:
                raise ValueError(f"P2 native parity violation: {city} {entry['date']}")
            positive += y
            surprise = (y == 1 and native_p2 <= 0.25) or (y == 0 and native_p2 >= 0.75)
            if surprise:
                if streak > 0 and y == last_surprise:
                    streak += 1
                else:
                    streak = 1
                last_surprise = y
            else:
                streak = 0
            alpha += y
            beta += 1 - y
            if streak >= 2:
                changes += 1
                alpha = 1 + 2*y
                beta = 1 + 2*(1-y)
                streak = 0
        native[city]["__native_regime_changes"] = changes
    return native


def sigmoid(z):
    z = max(-15.0, min(15.0, z))
    return 1.0 / (1.0 + math.exp(-z))


def predictions(source, native):
    output = []
    for city in LOCATIONS:
        w = [0.0] * 7
        last = deque(maxlen=14)
        previous = None
        for rec in source[city]:
            date = rec["date"]
            y, p1, p2 = native[city][date]
            if previous is not None:
                x = logistic_features(rec, previous)
                logistic = sigmoid(sum(a*b for a, b in zip(w, x)))
                rolling = (1 + sum(last)) / (2 + len(last))
                if date.year >= 2014:
                    output.append(dict(city=city, date=date.isoformat(), year=date.year,
                                       observed=y, P1=p1, P2=p2,
                                       Rolling14=rolling, LaggedLogistic=logistic))
                # Train only AFTER making prediction for today. No use of t's
                # precipitation or t's meteorological variables in x(t).
                error = logistic - y
                w = [w[j] - 0.12 * (error*x[j] +
                                    (0.0005*w[j] if j else 0.0))
                     for j in range(len(w))]
            last.append(y)
            previous = rec
    return output


def loss(p, y, name="brier"):
    if name == "brier":
        return (p - y)**2
    p = max(1e-12, min(1 - 1e-12, p))
    return -(y*math.log(p) + (1-y)*math.log(1-p))


def results(rows):
    scores = {}
    for city in ("Seattle", "New York", "Pooled"):
        subset = rows if city == "Pooled" else [r for r in rows if r["city"] == city]
        if len(subset) != (730 if city == "Pooled" else 365):
            raise ValueError(f"invalid holdout count for {city}")
        metrics = dict(days=len(subset), event_prevalence=sum(r["observed"] for r in subset)/len(subset))
        for model in ("P1", "P2", "Rolling14", "LaggedLogistic"):
            probs = [r[model] for r in subset]
            labels = [r["observed"] for r in subset]
            bins = [[] for _ in range(10)]
            for p,y in zip(probs, labels):
                bins[min(9, int(p*10))].append((p,y))
            ece = sum(len(b)/len(subset) *
                      abs(statistics.mean(p for p,_ in b) -
                          statistics.mean(y for _,y in b))
                      for b in bins if b)
            metrics[model] = dict(brier=statistics.mean(loss(p,y) for p,y in zip(probs,labels)),
                                  log_loss=statistics.mean(loss(p,y,"log") for p,y in zip(probs,labels)),
                                  ece_10bins=ece)
        metrics["P2_minus_P1_brier_improvement"] = metrics["P1"]["brier"]-metrics["P2"]["brier"]
        scores[city] = metrics
    return scores


def bootstrapped_delta(test_rows):
    # Resample paired 14-day date blocks; both cities remain paired in each day.
    byday = defaultdict(list)
    for r in test_rows:
        byday[r["date"]].append(r)
    dates = sorted(byday)
    if len(dates) != 365 or any(len(byday[d]) != 2 for d in dates):
        raise ValueError("invalid bootstrap sampling frame")
    day_delta = [statistics.mean(loss(r["P1"],r["observed"]) -
                                 loss(r["P2"],r["observed"]) for r in byday[d])
                 for d in dates]
    rng = random.Random(SEED)
    samples = []
    for _ in range(BOOTSTRAPS):
        sampled = []
        while len(sampled) < len(dates):
            j = rng.randrange(len(dates))
            sampled.extend(day_delta[(j+k) % len(dates)] for k in range(BLOCK))
        samples.append(statistics.mean(sampled[:len(dates)]))
    samples.sort()
    return [samples[int(0.025*BOOTSTRAPS)], samples[int(0.975*BOOTSTRAPS)]]


def evaluate(source_path, native_path):
    source = source_rows(source_path)
    native = native_rows(native_path, source)
    full = predictions(source, native)
    dev = [r for r in full if r["year"] == 2014]
    test = [r for r in full if r["year"] == 2015]
    metrics = results(test)
    ci = bootstrapped_delta(test)
    delta = metrics["Pooled"]["P2_minus_P1_brier_improvement"]
    noninferior = all(metrics[city]["P2_minus_P1_brier_improvement"] >= -0.02
                      for city in LOCATIONS)
    earned = (delta >= 0.01 and ci[0] > 0 and noninferior)
    return dict(
        experiment="MOOL-P3-WEATHER-001",
        status="COMPLETE",
        primary_claim="EARNED" if earned else "NOT_EARNED",
        protocol="benchmarks/p3_weather/PREREGISTRATION.md",
        source_sha256=digest(source_path),
        native_sha256=digest(native_path),
        split=dict(warmup="2012-2013",dev="2014",test="2015"),
        number_of_rows=sum(len(source[x]) for x in LOCATIONS),
        test=metrics,
        dev=results(dev),
        ci95_p2_vs_p1_pooled_block14=ci,
        primary_thresholds=dict(delta_min=0.01,ci_lower_gt=0,city_regression_min=-0.02),
        p2_regime_changes_full_chronology={
            city: native[city]["__native_regime_changes"] for city in LOCATIONS},
        limitations=["public instructional NOAA-derived transform not authenticated source",
                     "online backtest with sequential holdout feedback",
                     "event frequency, not causal world-transition prediction",
                     "sequential autocorrelation limits bootstrap interpretation"],
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source",required=True)
    parser.add_argument("--native",required=True)
    parser.add_argument("--output",required=True)
    args = parser.parse_args()
    result = evaluate(args.source, args.native)
    Path(args.output).parent.mkdir(parents=True,exist_ok=True)
    Path(args.output).write_text(json.dumps(result,indent=2,sort_keys=True)+"\n")
    print(json.dumps({k:result[k] for k in ("experiment","primary_claim","number_of_rows",
                                          "ci95_p2_vs_p1_pooled_block14")},
                     sort_keys=True))
    for city,metrics in result["test"].items():
        print(f'{city}: n={metrics["days"]} Brier P1={metrics["P1"]["brier"]:.6f} '
              f'P2={metrics["P2"]["brier"]:.6f} '
              f'rolling={metrics["Rolling14"]["brier"]:.6f} '
              f'logistic={metrics["LaggedLogistic"]["brier"]:.6f}')


if __name__=="__main__":
    main()
