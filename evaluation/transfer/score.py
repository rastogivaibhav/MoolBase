#!/usr/bin/env python3
"""Independent, label-separated paired QA scoring. Does not run inference."""
import argparse
from collections import Counter, defaultdict
import hashlib
import json
import math
from pathlib import Path
import random
import re
import string
import statistics
import tempfile
import unittest

ARMS = ("A", "B", "C", "D")
PAIRS = (("C", "B"), ("C", "A"), ("D", "B"), ("D", "C"))
COSTS = ("seconds", "model_calls", "prompt_tokens", "completion_tokens")

def sha(data):
    return hashlib.sha256(data).hexdigest()

def ids_digest(ids):
    return sha(("\n".join(sorted(ids)) + "\n").encode())

def read_rows(path):
    raw = Path(path).read_bytes()
    result = {}
    for line in raw.splitlines():
        if not line.strip():
            continue
        row = json.loads(line)
        if not isinstance(row, dict):
            raise ValueError("expected JSON object")
        qid = row.get("id")
        if not isinstance(qid, str) or not qid.strip() or "\n" in qid or "\r" in qid or qid in result:
            raise ValueError("invalid/duplicate ID")
        result[qid] = row
    if not result:
        raise ValueError("empty input")
    return result, sha(raw)

def norm(text):
    text = text.lower().translate(str.maketrans("", "", string.punctuation))
    return " ".join(re.sub(r"\b(a|an|the)\b", " ", text).split())

def exact(answer, aliases):
    return int(answer is not None and any(norm(answer) == norm(a) for a in aliases))

def f1(answer, aliases):
    if answer is None:
        return 0.0
    def one(alias):
        x, y = norm(answer).split(), norm(alias).split()
        n = sum((Counter(x) & Counter(y)).values())
        return 2 * n / (len(x) + len(y)) if n else 0.0
    return max(map(one, aliases))

def validate_gold(rows):
    for qid, row in rows.items():
        a, s = row.get("aliases"), row.get("stratum")
        if not isinstance(a, list) or not a or not all(isinstance(v, str) and norm(v) for v in a):
            raise ValueError("invalid aliases: " + qid)
        if not isinstance(s, str) or not s.strip():
            raise ValueError("invalid stratum: " + qid)

def validate_pred(rows, gold):
    if rows.keys() != gold.keys():
        raise ValueError("prediction ID set differs from gold")
    for qid, row in rows.items():
        answer = row.get("answer")
        if "answer" not in row or (answer is not None and
            (not isinstance(answer, str) or not norm(answer))):
            raise ValueError("invalid answer: " + qid)
        for name in COSTS:
            n = row.get(name)
            if type(n) not in (int, float) or not math.isfinite(n) or n < 0:
                raise ValueError("invalid cost: " + qid + " " + name)
            if name != "seconds" and type(n) != int:
                raise ValueError("count must be integer")

def paired(x, y, groups, seed=20261126, repetitions=10000):
    diffs = [a-b for a, b in zip(x, y)]
    rng = random.Random(seed)
    boot = sorted(sum(diffs[rng.choice(group)] for group in groups for _ in group) / len(diffs)
                  for _ in range(repetitions))
    wins, losses = diffs.count(1), diffs.count(-1)
    discord = wins + losses
    p = sum(math.comb(discord, k) for k in range(wins, discord+1)) / (2 ** discord) if discord else 1.0
    return {"difference": statistics.mean(diffs), "ci95": [boot[int(.025*repetitions)],
        boot[min(repetitions-1, int(.975*repetitions))]], "wins": wins, "losses": losses,
        "mcnemar_one_sided_p": p}

def score(gold, predictions):
    validate_gold(gold)
    if set(predictions) != set(ARMS):
        raise ValueError("exactly A,B,C,D required")
    ids = sorted(gold)
    strata = defaultdict(list)
    for i, qid in enumerate(ids):
        strata[gold[qid]["stratum"]].append(i)
    groups = [strata[k] for k in sorted(strata)]
    em, report = {}, {}
    for name in ARMS:
        rows = predictions[name]
        validate_pred(rows, gold)
        answers = [rows[q]["answer"] for q in ids]
        em[name] = [exact(a, gold[q]["aliases"]) for q, a in zip(ids, answers)]
        n, correct, covered = len(ids), sum(em[name]), sum(a is not None for a in answers)
        report[name] = {"n": n, "correct": correct, "em": correct/n,
            "f1": statistics.mean(f1(a, gold[q]["aliases"]) for q,a in zip(ids,answers)),
            "coverage": covered/n, "wrong_answer_rate": (covered-correct)/n,
            "selective_accuracy": correct/covered if covered else None,
            "cost_totals": {k: sum(rows[q][k] for q in ids) for k in COSTS}}
    comparisons = {a+"-"+b: paired(em[a],em[b],groups) for a,b in PAIRS}
    # Separate predeclared primary and secondary families; preserve raw p values.
    for family in (("C-B","C-A"),("D-B","D-C")):
        ordered = sorted(family, key=lambda k: comparisons[k]["mcnemar_one_sided_p"])
        previous = 0
        for i, k in enumerate(ordered):
            previous = max(previous, min(1, (len(ordered)-i)*comparisons[k]["mcnemar_one_sided_p"]))
            comparisons[k]["holm_p"] = previous
    return {"scope": "reviewer-supplied predictions, mechanical scoring only", "arms": report,
        "strata": {s:len(v) for s,v in strata.items()}, "comparisons": comparisons,
        "bootstrap_seed": 20261126, "bootstrap_repetitions": 10000}

class Tests(unittest.TestCase):
    def test_alias_and_abstention(self):
        self.assertEqual(exact("The UK!", ["UK"]), 1)
        self.assertEqual(exact(None, ["UK"]), 0)
        self.assertAlmostEqual(f1("red blue", ["red green"]), .5)
    def test_reject_invalid(self):
        gold = {"q": {"aliases": ["x"], "stratum": "2hop"}}
        row = {"answer": "x", "seconds": 1, "model_calls": 1, "prompt_tokens": 1, "completion_tokens": 1}
        validate_pred({"q": row}, gold)
        for bad in ({"z":row}, {"q":{**row,"seconds":float("nan")}}, {"q":{**row,"model_calls":True}}, {"q":{**row,"answer":""}}):
            with self.assertRaises(ValueError):
                validate_pred(bad,gold)
        with tempfile.TemporaryDirectory() as d:
            f = Path(d)/"input"
            f.write_text('{"id":"q"}\n{"id":"q"}\n')
            with self.assertRaises(ValueError):
                read_rows(f)
    def test_paired(self):
        result = paired([1,1,1,1],[0,0,0,0],[[0,1],[2,3]],repetitions=100)
        self.assertEqual(result["difference"],1)
        self.assertEqual(result["ci95"],[1,1])
        self.assertEqual(result["mcnemar_one_sided_p"],1/16)
    def test_full_score(self):
        gold = {"q": {"aliases":["x"],"stratum":"2hop"}}
        base = {"seconds":1,"model_calls":1,"prompt_tokens":2,"completion_tokens":1}
        pred = {a:{"q":{**base,"answer": ("x" if a=="C" else None)}} for a in ARMS}
        result = score(gold,pred)
        self.assertEqual(result["arms"]["C"]["em"],1)
        self.assertEqual(result["arms"]["B"]["coverage"],0)
        self.assertIsNone(result["arms"]["B"]["selective_accuracy"])

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--self-test",action="store_true")
    p.add_argument("--gold")
    p.add_argument("--gold-sha256")
    p.add_argument("--ids-sha256")
    p.add_argument("--pred",action="append",default=[])
    p.add_argument("--out",type=Path)
    a = p.parse_args()
    if a.self_test:
        return 0 if unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(Tests)).wasSuccessful() else 1
    if not all((a.gold,a.gold_sha256,a.ids_sha256,a.out)):
        p.error("gold, both hashes and out required")
    gold,h = read_rows(a.gold)
    if h != a.gold_sha256 or ids_digest(gold) != a.ids_sha256:
        raise ValueError("gold or ID digest mismatch")
    predictions, hashes = {}, {}
    for spec in a.pred:
        arm, sep, path = spec.partition("=")
        if not sep or arm not in ARMS or arm in predictions:
            raise ValueError("invalid or duplicate prediction arm")
        predictions[arm], hashes[arm] = read_rows(path)
    result = score(gold,predictions)
    result.update(gold_sha256=h,ids_sha256=ids_digest(gold),prediction_sha256=hashes)
    with a.out.open("x") as stream:
        json.dump(result,stream,indent=2,allow_nan=False)
        stream.write("\n")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
