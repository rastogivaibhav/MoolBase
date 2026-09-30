#!/usr/bin/env python3
"""Mechanical-only Cycle-5 summary. No outcome/correctness metrics."""
from __future__ import annotations
import argparse, collections, json
from pathlib import Path

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--raw",required=True); p.add_argument("--out",required=True)
    args=p.parse_args()
    raw=json.loads(Path(args.raw).read_text())
    configs=collections.Counter(); events=collections.Counter()
    statuses=collections.Counter(); opportunities=collections.Counter()
    failures=gaps=steps=rounds=0
    for ep in raw["episodes"]:
        cfg=ep["configuration"]; configs[cfg]+=1
        failures+=bool(ep.get("failures"))
        for step in ep.get("steps",[]):
            steps+=1; gaps+=len(step.get("telemetry_gaps",[]))
            status=step.get("final_state",{}).get("epistemic_status")
            if status is not None: statuses[(cfg,str(status))]+=1
            for ev in step.get("native_events",[]):
                events[(cfg,str(ev.get("type")))] += 1
            for rr in step.get("recovery_rounds",[]):
                rounds+=1
                opportunities[(cfg,str(rr.get("expansion_opportunity_class")))] += 1
    summary={
      "schema":"epistemic-process-v3-cycle5-mechanical-summary-v1",
      "score_bearing":False,"outcome_metrics_computed":False,
      "oracle_join_performed":False,
      "configuration_episode_counts":dict(sorted(configs.items())),
      "configuration_episode_total":sum(configs.values()),
      "runtime_failures":failures,"telemetry_gaps":gaps,
      "step_records":steps,"recovery_rounds":rounds,
      "native_event_counts":{f"{k[0]}:{k[1]}":v for k,v in sorted(events.items())},
      "final_status_counts":{f"{k[0]}:{k[1]}":v for k,v in sorted(statuses.items())},
      "expansion_opportunity_counts":{f"{k[0]}:{k[1]}":v for k,v in sorted(opportunities.items())},
    }
    if failures or gaps: raise SystemExit(json.dumps(summary,sort_keys=True))
    Path(args.out).write_text(json.dumps(summary,indent=2,sort_keys=True)+"\n")
    print(json.dumps(summary,sort_keys=True))
if __name__=="__main__": main()
