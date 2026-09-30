#!/usr/bin/env python3
"""Structural and anti-leakage validation for Epistemic Process V2."""
from __future__ import annotations
import argparse, json
from collections import Counter
from pathlib import Path

REQUIRED_STRATA = {
    "duplicate_correlation","independent_corroboration","decisive_refutation",
    "late_revocation","insufficient_replacement","abstention",
    "temporary_contradiction","misleading_majority","irrelevant_noise",
    "order_permutation","negative_control_no_change",
}
ALLOWED_KINDS = {"support","refute","revoke","noise"}
ALLOWED_TERMINALS = {"H1","H2","ABSTAIN"}
ALLOWED_TEMPORAL = {"to_H1","to_H2","to_abstain","no_change"}
RUNTIME_FIELDS = {"step","id","family","kind","bears_on","depends_on","revokes"}

def load(path: str):
    return json.loads(Path(path).read_text(encoding="utf-8"))

def fail(errors):
    if errors:
        raise SystemExit("V2 validation failed:\n- " + "\n- ".join(errors))

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--prereg",required=True)
    ap.add_argument("--tasks",required=True)
    args=ap.parse_args()
    prereg=load(args.prereg); tasks=load(args.tasks); errors=[]
    if prereg.get("protocol")!="epistemic-process-v2": errors.append("bad prereg protocol")
    if str(prereg.get("status","")).upper()!="PREREGISTERED_UNSCORED": errors.append("prereg is not unscored")
    if prereg.get("score_bearing_runs_allowed") is not False: errors.append("score bearing must remain disabled")
    if prereg.get("post_freeze_tuning_forbidden") is not True: errors.append("post-freeze tuning prohibition missing")
    if tasks.get("protocol")!="epistemic-process-v2": errors.append("bad tasks protocol")
    if str(tasks.get("status","")).upper()!="PREREGISTERED_UNSCORED": errors.append("tasks are not unscored")
    episodes=tasks.get("episodes") or []
    if len(episodes)<64: errors.append(f"need >=64 episodes; found {len(episodes)}")
    if tasks.get("episode_count")!=len(episodes): errors.append("episode_count mismatch")
    ids=[str(x.get("id")) for x in episodes]
    if len(ids)!=len(set(ids)): errors.append("duplicate episode ids")
    strata={str(x.get("stratum")) for x in episodes}
    missing=sorted(REQUIRED_STRATA-strata)
    if missing: errors.append("missing strata: "+",".join(missing))
    for ep in episodes:
        eid=str(ep.get("id")); seen=set(); last=0
        if ep.get("terminal_supported") not in ALLOWED_TERMINALS:
            errors.append(f"{eid}: invalid terminal")
        temporal=ep.get("temporal_oracle") or {}
        if temporal.get("mode") not in ALLOWED_TEMPORAL:
            errors.append(f"{eid}: invalid temporal oracle mode")
        for ev in ep.get("events") or []:
            step=int(ev.get("step",0)); evid=str(ev.get("id",""))
            if step<=last: errors.append(f"{eid}: non-increasing step {step}")
            last=step
            if not evid or evid in seen: errors.append(f"{eid}: duplicate/empty event id {evid}")
            if ev.get("kind") not in ALLOWED_KINDS: errors.append(f"{eid}/{evid}: invalid kind")
            if ev.get("bears_on") not in {"H1","H2"}: errors.append(f"{eid}/{evid}: invalid bears_on")
            for ref in list(ev.get("depends_on") or [])+list(ev.get("revokes") or []):
                if str(ref) not in seen: errors.append(f"{eid}/{evid}: forward/unknown reference {ref}")
            if ev.get("kind")=="revoke" and not ev.get("revokes"):
                errors.append(f"{eid}/{evid}: revoke without revokes")
            seen.add(evid)
    hidden=set(tasks.get("oracle_fields_not_sent_to_sut") or [])
    required_hidden={"terminal_supported","minimum_independent_families_for_resolution","temporal_oracle","notes","stratum","variant","independent","decisive"}
    if not required_hidden.issubset(hidden): errors.append("oracle leakage declaration incomplete")
    if not RUNTIME_FIELDS.isdisjoint(hidden):
        errors.append("runtime fields overlap declared hidden oracle fields")
    fail(errors)
    counts=Counter(str(ep.get("stratum")) for ep in episodes)
    print(json.dumps({
        "valid":True,"protocol":"epistemic-process-v2","episodes":len(episodes),
        "strata":dict(sorted(counts.items())),"score_bearing":False
    },sort_keys=True))

if __name__=="__main__":
    main()
