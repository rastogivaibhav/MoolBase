#!/usr/bin/env python3
"""Seeded contradiction / stale-evidence benchmark for MoolBase.

Generates synthetic evidence streams with an unambiguous intended answer,
runs the real native adapter (build/moolbase_customer_showcase), and scores it
against simple baselines. No model or network is used.

READ THE LIMITS: scenarios and ground truth are authored here, so this measures
whether MoolBase implements the evidence policy it documents (independent
verified families beat echoes; revoked evidence stops counting; true ties are
not forced). It is not an external benchmark and says nothing about natural
language extraction. Graphiti/Letta/BeliefMem are NOT run here (they need
LLM keys and network access).
"""
import argparse, json, random, subprocess, tempfile, collections
from pathlib import Path

ABSTAIN = None
COMMIT = {"resolved", "provisionally_resolved"}


def ev(i, fam, tgt, kind="support", verified=1, retire=""):
    return dict(id=f"e{i}", family=fam, target=tgt, kind=kind, verified=verified, retire=retire)


def gen(kind, rng):
    """Return (events, truth) where truth is 0, 1 or ABSTAIN."""
    n = [0]

    def E(*a, **k):
        n[0] += 1
        return ev(n[0], *a, **k)
    flip = rng.random() < 0.5            # randomise which target is correct
    T = (lambda t: 1 - t) if flip else (lambda t: t)
    if kind == "update_chain":           # last-write-wins is the right answer
        evs, cur, prev = [], T(0), None
        first = E("src0", cur); evs.append(first); prev = first
        for k in range(rng.randint(1, 4)):
            cur = 1 - cur
            e = E(f"src{k+1}", cur, "supersede", retire=prev["id"]); evs.append(e); prev = e
        return evs, cur
    if kind == "echo_flood":             # many copies of one source vs fewer independent sources
        wrong = [E("cache", T(0)) for _ in range(rng.randint(3, 8))]
        right = [E(f"indep{k}", T(1)) for k in range(rng.randint(2, 3))]
        evs = wrong + right; rng.shuffle(evs); return evs, T(1)
    if kind == "revoked_majority":       # the apparent majority is later revoked
        a = [E(f"a{k}", T(0)) for k in range(3)]
        b = [E(f"b{k}", T(1)) for k in range(2)]
        rev = [E(f"audit{k}", T(0), "revoke", retire=a[k]["id"]) for k in range(2)]
        evs = a + b; rng.shuffle(evs); return evs + rev, T(1)
    if kind == "unverified_flood":       # many unverified claims vs fewer verified ones
        w = [E(f"u{k}", T(0), verified=0) for k in range(rng.randint(4, 6))]
        r = [E(f"v{k}", T(1)) for k in range(2)]
        evs = w + r; rng.shuffle(evs); return evs, T(1)
    if kind == "true_tie":               # equal independent evidence: correct behaviour is not to commit
        evs = [E(f"a{k}", 0) for k in range(2)] + [E(f"b{k}", 1) for k in range(2)]
        rng.shuffle(evs); return evs, ABSTAIN
    raise ValueError(kind)


KINDS = ["update_chain", "echo_flood", "revoked_majority", "unverified_flood", "true_tie"]


# ---- baselines: each returns 0/1/None from the event list ----
def active(evs):
    dead = {e["retire"] for e in evs if e["retire"]}
    return [e for e in evs if e["kind"] in ("support", "supersede") and e["id"] not in dead]


def b_lww(evs):
    a = active(evs); return a[-1]["target"] if a else None


def b_majority(evs):
    c = collections.Counter(e["target"] for e in active(evs))
    return None if len(c) != 1 and len(set(c.values())) == 1 else (c.most_common(1)[0][0] if c else None)


def b_family_verified(evs):  # strong non-LLM rule: distinct verified families, tie => abstain
    c = {0: set(), 1: set()}
    for e in active(evs):
        if e["verified"]: c[e["target"]].add(e["family"])
    if len(c[0]) == len(c[1]): return None
    return 0 if len(c[0]) > len(c[1]) else 1


BASELINES = {"last_write_wins": b_lww, "count_majority": b_majority, "verified_family_rule": b_family_verified}


def run_moolbase(binary, evs):
    with tempfile.TemporaryDirectory(prefix="mb-contra-") as t:
        tsv = Path(t) / "e.tsv"
        tsv.write_text("\n".join("\t".join([e["id"], f"claim {e['id']}", e["family"], str(e["target"]),
                                           e["kind"], str(e["verified"]), e["retire"]]) for e in evs) + "\n")
        r = subprocess.run([str(binary), str(Path(t) / "db"), str(tsv), "H0", "H1"], text=True, capture_output=True)
        if r.returncode:
            return "ERROR", r.stderr.strip()[:200]
        st = [json.loads(l) for l in r.stdout.splitlines()]
        if not all(s["ok"] for s in st):
            return "ERROR", "event rejected"
        last = st[-2]                                   # state after final event (last line is reopen replay)
        ids = [x["id"] for x in last["targets"]]
        if last["status"] in COMMIT and last["answer"] in ids:
            return ids.index(last["answer"]), last["status"]
        return ABSTAIN, last["status"]


def score(rows):
    out = {}
    for name, preds in rows.items():
        tot = len(preds); ok = wrong = abst_ok = over = err = 0
        for pred, truth in preds:
            if pred == "ERROR": err += 1; continue
            if truth is ABSTAIN:
                if pred is ABSTAIN: ok += 1; abst_ok += 1
                else: wrong += 1
            else:
                if pred == truth: ok += 1
                elif pred is ABSTAIN: over += 1
                else: wrong += 1
        out[name] = dict(n=tot, correct=ok, wrong_commit=wrong, over_abstain=over, errors=err,
                         accuracy=round(ok / tot, 4), wrong_commit_rate=round(wrong / tot, 4))
    return out


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--binary", type=Path, default=Path("build/moolbase_customer_showcase"))
    p.add_argument("--per-kind", type=int, default=100)
    p.add_argument("--seed", type=int, default=20261122)
    p.add_argument("--output", type=Path, default=Path("reports/contradiction-bench/results.json"))
    a = p.parse_args()
    rng = random.Random(a.seed)
    per = {k: collections.defaultdict(list) for k in KINDS}
    allrows = collections.defaultdict(list)
    detail = collections.defaultdict(collections.Counter)
    wrong_resolved = collections.Counter()   # wrong answer AND status == "resolved" (the high-confidence tier)
    for kind in KINDS:
        for _ in range(a.per_kind):
            evs, truth = gen(kind, rng)
            preds = {"moolbase": run_moolbase(a.binary, evs)}
            detail[kind][str(preds["moolbase"][1])] += 1
            mp, mstatus = preds["moolbase"]
            if mp != "ERROR" and mstatus == "resolved" and (mp != truth): wrong_resolved[kind] += 1
            preds = {"moolbase": preds["moolbase"][0], **{n: f(evs) for n, f in BASELINES.items()}}
            for n, pr in preds.items():
                per[kind][n].append((pr, truth)); allrows[n].append((pr, truth))
    result = dict(seed=a.seed, per_kind_n=a.per_kind, overall=score(allrows),
                  by_scenario={k: score(v) for k, v in per.items()},
                  moolbase_wrong_at_resolved_status=dict(wrong_resolved),
                  moolbase_status_counts={k: dict(v) for k, v in detail.items()},
                  limits="Self-authored synthetic scenarios; tests documented policy, not external validity.")
    a.output.parent.mkdir(parents=True, exist_ok=True)
    a.output.write_text(json.dumps(result, indent=2) + "\n")
    print(f"{'scenario':18}" + "".join(f"{n:>22}" for n in ["moolbase", *BASELINES]))
    for k in KINDS:
        print(f"{k:18}" + "".join(f"{v['accuracy']*100:>16.0f}% (w{v['wrong_commit']:>3})" if False else
                                  f"{v['accuracy']*100:>14.0f}% wrong={v['wrong_commit']:<3}"
                                  for v in result["by_scenario"][k].values()))
    print(f"{'OVERALL':18}" + "".join(f"{v['accuracy']*100:>14.0f}% wrong={v['wrong_commit']:<3}" for v in result["overall"].values()))
    print("MoolBase wrong answers at status=resolved:", dict(wrong_resolved) or "none")
    print("MoolBase statuses:", json.dumps(result["moolbase_status_counts"]))
    print("Saved", a.output)


if __name__ == "__main__":
    main()
