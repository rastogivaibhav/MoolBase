#!/usr/bin/env python3
"""V3 Cycle-5 blind, unscored mechanical campaign.

No oracle data is loaded. No outcome/correctness metric is computed.
"""
from __future__ import annotations

import argparse
import json
import subprocess
import tempfile
from pathlib import Path
from typing import Any, Mapping

import sys
ROOT=Path(__file__).resolve().parent
V2=ROOT.parent/"epistemic_process_v2"
sys.path.insert(0,str(V2))
from common_head_v2 import common_head  # noqa: E402

CONFIGS=("C0","G0E","C1","G1","G2")

def load(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))

def dump(path,value):
    target=Path(path); target.parent.mkdir(parents=True,exist_ok=True)
    target.write_text(json.dumps(value,indent=2,sort_keys=True)+"\n",encoding="utf-8")

def csv(values):
    return ",".join(str(x) for x in (values or []))

def row(event):
    vals=[
        str(event["step"]),
        str(event["id"]),
        str(event["raw_family_id"]),
        str(event["kind"]),
        str(event["bears_on"]),
        csv(event.get("depends_on")),
        csv(event.get("revokes")),
        csv(event.get("supersedes")),
        "1" if event.get("independent",True) else "0",
        "1" if event.get("semantic_verification",True) else "0",
        "1" if event.get("material",False) else "0",
    ]
    if any("\t" in v or "\n" in v for v in vals):
        raise ValueError("TSV delimiter in runtime task")
    return "\t".join(vals)

def common_metadata(task):
    out={}
    for event in task["runtime"]["events"]:
        kind=str(event["kind"])
        if kind in {"support","supersede","noop"}:
            head_kind="support"
        elif kind=="refute":
            head_kind="refute"
        else:
            head_kind="lifecycle"
        out[str(event["id"])]={
            "family":str(event["raw_family_id"]),
            "kind":head_kind,
            "bears_on":str(event["bears_on"]),
            "depends_on":[str(v) for v in event.get("depends_on",[])],
            "revokes":[str(v) for v in event.get("revokes",[])],
        }
    return out

def head_state(decision):
    return {
        "has_answer":decision["operative_hypothesis"] is not None,
        "operative_hypothesis":decision["operative_hypothesis"],
        "committed_answer":None,
        "epistemic_status":decision["status"],
        "bundle_hash":None,
        "visited_states":None,
        "truncated":None,
        "evidence_family_ids":sorted(
            set(decision["support_families"]["H1"]) |
            set(decision["support_families"]["H2"]) |
            set(decision["opposition_families"]["H1"]) |
            set(decision["opposition_families"]["H2"])
        ),
    }

def c0_episode(task):
    metadata=common_metadata(task)
    steps=[]
    for event in task["runtime"]["events"]:
        refs=[str(event["id"])] if str(event["kind"])!="revoke" else []
        decision=common_head(refs,metadata)
        steps.append({
            "step":int(event["step"]),
            "ingested_evidence_ids":[str(event["id"])],
            "final_state":head_state(decision),
            "active_evidence_refs":refs,
            "native_events":[],
            "recovery_rounds":[],
            "telemetry_gaps":[],
            "execution":{"runtime_failure":False},
            "common_head":decision,
        })
    return {"episode_id":task["id"],"configuration":"C0","steps":steps,"failures":[]}

def normalise_runtime(episode):
    nodes={int(v):k for k,v in episode["hypothesis_nodes"].items()}
    for step in episode["steps"]:
        state=step["final_state"]
        raw=int(state.pop("operative_hypothesis_node",0) or 0)
        committed=int(state.pop("committed_answer_node",0) or 0)
        state["operative_hypothesis"]=nodes.get(raw)
        state["committed_answer"]=nodes.get(committed)
        for ev in step.get("native_events",[]):
            ev["previous_hypothesis"]=nodes.get(int(ev.pop("previous_hypothesis_node",0) or 0))
            ev["hypothesis"]=nodes.get(int(ev.pop("hypothesis_node",0) or 0))
            ev["competing_hypotheses"]=[
                nodes.get(int(v),f"node:{v}") for v in ev.get("competing_hypotheses",[])
            ]
        for rr in step.get("recovery_rounds",[]):
            rr["operative_hypothesis_before"]=nodes.get(int(rr.pop("operative_hypothesis_node_before",0) or 0))
            rr["operative_hypothesis_after"]=nodes.get(int(rr.pop("operative_hypothesis_node_after",0) or 0))
            rr["committed_answer_before"]=nodes.get(int(rr.pop("committed_answer_node_before",0) or 0))
            rr["committed_answer_after"]=nodes.get(int(rr.pop("committed_answer_node_after",0) or 0))
    episode.pop("hypothesis_nodes",None)
    return episode

def production_episode(task,env,configuration,runner,seed,root):
    safe="".join(ch if ch.isalnum() or ch in "-_" else "_" for ch in task["id"])
    visible=root/f"{configuration}-{safe}-visible.tsv"
    latent=root/f"{configuration}-{safe}-latent.tsv"
    visible.write_text("\n".join(row(e) for e in task["runtime"]["events"])+"\n",encoding="utf-8")
    latent_events=env.get("latent_events") or []
    latent.write_text(
        ("\n".join(row(e) for e in latent_events)+"\n") if latent_events else "",
        encoding="utf-8",
    )
    layout=task["runtime"]["target_layout"]
    limits=task["runtime"]["initial_search_limits"]
    proc=subprocess.run([
        str(runner),
        configuration,
        str(task["id"]),
        str(visible),
        str(latent),
        str(root/f"db-{configuration}-{safe}"),
        str(seed),
        ",".join(layout["target_insertion_order"]),
        str(layout["padding_nodes_before_targets"]),
        str(limits["semantic_candidates"]),
        str(limits["max_paths"]),
        str(limits["max_visited_states"]),
    ],capture_output=True,text=True)
    if proc.returncode:
        return {
            "episode_id":task["id"],"configuration":configuration,"steps":[],
            "failures":[f"runtime exit={proc.returncode}",proc.stderr[-3000:]],
        }
    try:
        ep=json.loads(proc.stdout)
    except json.JSONDecodeError as exc:
        return {
            "episode_id":task["id"],"configuration":configuration,"steps":[],
            "failures":[f"malformed JSON: {exc}",proc.stdout[-3000:]],
        }
    ep["configuration"]=configuration
    return normalise_runtime(ep)

def c1_episode(task,g0e):
    metadata=common_metadata(task)
    steps=[]
    for source_step in g0e["steps"]:
        decision=common_head(source_step.get("active_evidence_refs",[]),metadata)
        state=head_state(decision)
        state["bundle_hash"]=source_step["final_state"].get("bundle_hash")
        state["visited_states"]=source_step["final_state"].get("visited_states")
        state["truncated"]=source_step["final_state"].get("truncated")
        steps.append({
            "step":source_step["step"],
            "ingested_evidence_ids":source_step["ingested_evidence_ids"],
            "final_state":state,
            "active_evidence_refs":list(source_step.get("active_evidence_refs",[])),
            "native_events":[],
            "recovery_rounds":[],
            "telemetry_gaps":list(source_step.get("telemetry_gaps",[])),
            "execution":{"runtime_failure":False},
            "common_head":decision,
        })
    return {"episode_id":task["id"],"configuration":"C1","steps":steps,"failures":[]}

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--runtime-tasks",required=True)
    parser.add_argument("--environment",required=True)
    parser.add_argument("--runtime-runner",required=True)
    parser.add_argument("--out",required=True)
    parser.add_argument("--seed",type=int,default=20261005)
    parser.add_argument("--episode-id",action="append")
    args=parser.parse_args()

    tasks=load(args.runtime_tasks)
    env=load(args.environment)
    assert tasks["score_bearing_authorized"] is False
    assert env["score_bearing_authorized"] is False
    assert env["oracle_join_permitted"] is False
    by_env={e["id"]:e for e in env["episodes"]}
    selected=set(args.episode_id or [])
    rows=[e for e in tasks["episodes"] if not selected or e["id"] in selected]
    if selected and len(rows)!=len(selected):
        raise SystemExit("requested replay episode id missing")

    episodes=[]
    runner=Path(args.runtime_runner).resolve()
    with tempfile.TemporaryDirectory(prefix="moolbase-v3-cycle5-") as td:
        root=Path(td)
        for task in rows:
            c0=c0_episode(task)
            episodes.append(c0)
            g0e=production_episode(task,by_env[task["id"]],"G0E",runner,args.seed,root)
            episodes.append(g0e)
            episodes.append(c1_episode(task,g0e) if not g0e["failures"] else {
                "episode_id":task["id"],"configuration":"C1","steps":[],
                "failures":["G0E prerequisite failed"],
            })
            episodes.append(production_episode(task,by_env[task["id"]],"G1",runner,args.seed,root))
            episodes.append(production_episode(task,by_env[task["id"]],"G2",runner,args.seed,root))

    result={
        "schema":"epistemic-process-v3-cycle5-blind-unscored-v1",
        "mode":"blind-unscored-mechanical-validation",
        "score_bearing":False,
        "score_bearing_authorized":False,
        "oracle_join_performed":False,
        "outcome_metrics_computed":False,
        "seed":args.seed,
        "configurations":list(CONFIGS),
        "episode_count":len(rows),
        "configuration_episode_total":len(episodes),
        "episodes":episodes,
    }
    dump(args.out,result)
    failures=sum(bool(e.get("failures")) for e in episodes)
    print(json.dumps({
        "episodes":len(rows),
        "configuration_executions":len(episodes),
        "runtime_failures":failures,
        "score_bearing":False,
        "oracle_join_performed":False,
    },sort_keys=True))
    if failures:
        raise SystemExit("Cycle-5 runtime failures observed")

if __name__=="__main__":
    main()
