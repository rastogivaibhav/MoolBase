#!/usr/bin/env python3
"""Run the frozen V3 universe through production variants without oracle join."""
from __future__ import annotations
import argparse, hashlib, json, subprocess, tempfile
from pathlib import Path
from typing import Any, Mapping

CONFIGS=("G0E","G1","G2")
FORBIDDEN_OUTPUT_KEYS={
    "oracle","expected_terminal_operative","expected_terminal_committed",
    "expected_status","expected_abstention","correctness","claim_status",
    "score","accuracy","task_family","mirror_primary","claim_ids",
}

def canonical_bytes(value: Any) -> bytes:
    return (json.dumps(value,separators=(",",":"),sort_keys=True)+"\n").encode()

def sanitized_row(event: Mapping[str,Any]) -> str:
    vals=[
        str(event["step"]),str(event["id"]),str(event["raw_family_id"]),
        str(event["kind"]),str(event["bears_on"]),
        ",".join(str(x) for x in event.get("depends_on",[])),
        ",".join(str(x) for x in event.get("revokes",[])),
        ",".join(str(x) for x in event.get("supersedes",[])),
        "1" if event.get("semantic_verification",False) else "0",
        "1" if event.get("material",False) else "0",
        "1" if event.get("independent",True) else "0",
    ]
    if any("\t" in v or "\n" in v for v in vals):
        raise ValueError("TSV delimiter in task")
    return "\t".join(vals)

def node_name(raw: Any, mapping: Mapping[int,str]) -> str|None:
    try: node=int(raw or 0)
    except (TypeError,ValueError): return None
    return mapping.get(node)

def normalise_ranking(value: Any,mapping: Mapping[int,str]):
    if not isinstance(value,list): return value
    out=[]
    for item in value:
        row=dict(item)
        row["target_id"]=node_name(row.get("target_id"),mapping)
        out.append(row)
    return out

def normalise_state(state: Mapping[str,Any],mapping: Mapping[int,str]):
    row=dict(state)
    operative=row.pop("operative_hypothesis_node",0)
    committed=row.pop("committed_answer_node",0)
    has=bool(row.get("has_answer"))
    row["operative_hypothesis"]=node_name(operative,mapping) if has else None
    row["committed_answer"]=node_name(committed,mapping) if committed else None
    row["target_ranking"]=normalise_ranking(row.get("target_ranking"),mapping)
    return row

def normalise_episode(ep: dict[str,Any]) -> None:
    mapping={int(v):str(k) for k,v in (ep.get("hypothesis_nodes") or {}).items()}
    for step in ep.get("steps",[]):
        for k in ("previous_step_state","initial_state","final_state"):
            if isinstance(step.get(k),Mapping):
                step[k]=normalise_state(step[k],mapping)
        for rr in step.get("recovery_rounds",[]):
            rr["target_ranking_before"]=normalise_ranking(rr.get("target_ranking_before"),mapping)
            rr["target_ranking_after"]=normalise_ranking(rr.get("target_ranking_after"),mapping)
            for suffix in ("before","after"):
                has=bool(rr.get(f"has_answer_{suffix}"))
                op=rr.pop(f"operative_hypothesis_node_{suffix}",0)
                cm=rr.pop(f"committed_answer_node_{suffix}",0)
                rr[f"operative_hypothesis_{suffix}"]=node_name(op,mapping) if has else None
                rr[f"committed_answer_{suffix}"]=node_name(cm,mapping) if cm else None
        for event in step.get("native_events",[]):
            if "previous_hypothesis_node" in event:
                event["previous_hypothesis"]=node_name(event.pop("previous_hypothesis_node"),mapping)
            if "hypothesis_node" in event:
                event["hypothesis"]=node_name(event.pop("hypothesis_node"),mapping)
            if "competing_hypotheses" in event:
                event["competing_hypotheses"]=[
                    node_name(x,mapping) for x in event["competing_hypotheses"]
                    if node_name(x,mapping) is not None
                ]

def scan_no_oracle(value: Any,path="root"):
    if isinstance(value,dict):
        for k,v in value.items():
            if k in FORBIDDEN_OUTPUT_KEYS or k.startswith("expected_"):
                raise AssertionError(f"forbidden result key {path}.{k}")
            scan_no_oracle(v,f"{path}.{k}")
    elif isinstance(value,list):
        for i,v in enumerate(value): scan_no_oracle(v,f"{path}[{i}]")

def run_episode(ep: Mapping[str,Any],config: str,runner: Path,root: Path,seed:int):
    safe="".join(ch if ch.isalnum() or ch in "-_" else "_" for ch in ep["id"])
    visible=root/f"{config}-{safe}-visible.tsv"
    latent=root/f"{config}-{safe}-latent.tsv"
    contract=root/f"{config}-{safe}-contract.tsv"
    visible.write_text("\n".join(sanitized_row(e) for e in ep["runtime"]["events"])+"\n")
    latent_rows=[sanitized_row(e) for e in ep["harness_environment"]["latent_events"]]
    latent.write_text(("\n".join(latent_rows)+"\n") if latent_rows else "")
    layout=ep["runtime"]["target_layout"]
    limits=ep["runtime"]["initial_search_limits"]
    contract.write_text("\t".join([
        ",".join(layout["target_insertion_order"]),
        str(layout["padding_nodes_before_targets"]),
        str(limits["semantic_candidates"]),
        str(limits["max_paths"]),
        str(limits["max_visited_states"]),
    ])+"\n")
    db=root/f"db-{config}-{safe}"
    proc=subprocess.run([
        str(runner),config,str(ep["id"]),str(visible),str(latent),
        str(contract),str(db),str(seed)
    ],capture_output=True,text=True)
    if proc.returncode:
        return {"episode_id":ep["id"],"configuration":config,"steps":[],
                "failures":[f"runtime exit={proc.returncode}",proc.stderr[-4000:]]}
    try: out=json.loads(proc.stdout)
    except json.JSONDecodeError as exc:
        return {"episode_id":ep["id"],"configuration":config,"steps":[],
                "failures":[f"malformed JSON {exc}",proc.stdout[-4000:]]}
    out["configuration"]=config
    out["blind_variant"]=ep["variant"]
    normalise_episode(out)
    return out

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--tasks",required=True)
    p.add_argument("--runtime-runner",required=True)
    p.add_argument("--out",required=True)
    p.add_argument("--seed",type=int,default=20261004)
    args=p.parse_args()
    blind=json.loads(Path(args.tasks).read_text())
    assert blind["score_bearing_authorized"] is False
    runner=Path(args.runtime_runner).resolve()
    episodes=[]
    with tempfile.TemporaryDirectory(prefix="moolbase-v3-cycle5-") as td:
        root=Path(td)
        for config in CONFIGS:
            for ep in blind["episodes"]:
                episodes.append(run_episode(ep,config,runner,root,args.seed))
    result={
        "schema":"epistemic-process-v3-cycle5-blind-unscored-v1",
        "mode":"blind-unscored-mechanical",
        "score_bearing":False,
        "score_bearing_authorized":False,
        "oracle_join_performed":False,
        "seed":args.seed,
        "blind_input_sha256":hashlib.sha256(Path(args.tasks).read_bytes()).hexdigest(),
        "configurations":list(CONFIGS),
        "episodes":episodes,
    }
    scan_no_oracle(result)
    Path(args.out).write_bytes(canonical_bytes(result))
    failures=sum(bool(x.get("failures")) for x in episodes)
    print(json.dumps({
        "executions":len(episodes),"runtime_failures":failures,
        "score_bearing":False,"oracle_join_performed":False
    },sort_keys=True))
    if failures: raise SystemExit("runtime failures observed")

if __name__=="__main__":
    main()
