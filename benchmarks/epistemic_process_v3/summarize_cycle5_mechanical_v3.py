#!/usr/bin/env python3
"""Emit mechanical-only V3 Cycle-5 summary; no outcome metrics."""
from __future__ import annotations
import argparse, collections, json
from pathlib import Path

def load(p): return json.loads(Path(p).read_text(encoding="utf-8"))

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--raw",required=True)
    p.add_argument("--environment",required=True)
    p.add_argument("--out",required=True)
    a=p.parse_args()
    raw=load(a.raw); env=load(a.environment)
    latent={e["id"]:{x["id"] for x in e.get("latent_events",[])} for e in env["episodes"]}
    by=collections.Counter(); events=collections.Counter(); opportunities=collections.Counter()
    failures=gaps=steps=rounds=frontier=0
    latent_g2=latent_discovered=0
    for ep in raw["episodes"]:
        cfg=ep["configuration"]; eid=ep["episode_id"]; by[cfg]+=1
        failures += bool(ep.get("failures"))
        found=set()
        for step in ep.get("steps",[]):
            steps+=1; gaps+=len(step.get("telemetry_gaps",[]))
            found.update(set(step.get("active_evidence_refs",[])) & latent.get(eid,set()))
            for ev in step.get("native_events",[]):
                events[f"{cfg}:{ev.get('type')}"]+=1
            for rr in step.get("recovery_rounds",[]):
                rounds+=1
                opportunities[f"{cfg}:{rr.get('expansion_opportunity_class')}"]+=1
                frontier += bool(rr.get("frontier_changed"))
        if cfg=="G2" and latent.get(eid):
            latent_g2+=1
            latent_discovered += bool(found)

    out={
        "schema":"epistemic-process-v3-cycle5-mechanical-summary-v1",
        "score_bearing":False,
        "score_bearing_authorized":False,
        "outcome_metrics_computed":False,
        "oracle_join_performed":False,
        "configuration_episode_counts":dict(sorted(by.items())),
        "configuration_episode_total":sum(by.values()),
        "runtime_failures":failures,
        "telemetry_gaps":gaps,
        "step_records":steps,
        "recovery_rounds":rounds,
        "native_event_counts":dict(sorted(events.items())),
        "expansion_opportunity_counts":dict(sorted(opportunities.items())),
        "frontier_changes":frontier,
        "latent_g2_episodes":latent_g2,
        "latent_g2_discovered":latent_discovered,
    }
    Path(a.out).write_text(json.dumps(out,indent=2,sort_keys=True)+"\n",encoding="utf-8")
    print(json.dumps(out,sort_keys=True))

if __name__=="__main__":
    main()
