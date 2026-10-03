#!/usr/bin/env python3
"""Fail-closed structural validator for EP-PROCESS-V3 Cycle-3 task universe."""
from __future__ import annotations

import json
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parent
TASKS = ROOT / "tasks_v3_candidate.json"
REQ = ROOT / "task_family_requirements_v3.json"
SEM = ROOT / "semantic_contract_v3.json"

EXPECTED_FAMILIES = [
    "exact_symmetric_tie","near_tie","label_mirror_tie",
    "node_id_permutation_tie","target_insertion_permutation",
    "evidence_order_permutation","family_name_permutation",
    "duplicate_correlated_support","valid_persistent_history",
    "explicit_revocation","explicit_supersession","refutation_no_replacement",
    "refutation_insufficient_replacement","refutation_sufficient_replacement",
    "delayed_sufficient_replacement","correlated_majority_independent_minority",
    "dwm_latent_evidence_positive","dwm_exhausted_frontier_negative",
    "dwm_insufficiency_no_opposition","dwm_genuine_opposition",
    "earned_revision","earned_resolution","single_step_control","no_change_control",
]

FORBIDDEN_RUNTIME_KEYS = {
    "expected_terminal_answer","correctness","expected_operative_hypothesis",
    "expected_committed_answer","challenge_warranted","challenge_warranted_steps",
    "revision_required","expected_usefulness","expected_expansion_result",
    "future_observations","outcome_class","expected_abstention",
    "evaluator_only_independence","earned_resolution_oracle",
    "metric_eligibility","oracle","task_family_outcome_label",
    "expected_terminal_operative","expected_terminal_committed",
    "expected_status","expected_dwm_useful_reopen",
    "expected_expansion_opportunity_class","resolution_eligible",
}

class ValidationError(AssertionError):
    pass

def fail(message: str) -> None:
    raise ValidationError(message)

def require(condition: bool, message: str) -> None:
    if not condition:
        fail(message)

def load(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))

def walk_keys(value: Any, path: str = ""):
    if isinstance(value, dict):
        for key, child in value.items():
            yield path, key, child
            yield from walk_keys(child, f"{path}.{key}" if path else key)
    elif isinstance(value, list):
        for idx, child in enumerate(value):
            yield from walk_keys(child, f"{path}[{idx}]")

def all_events(ep: dict[str, Any]) -> list[dict[str, Any]]:
    return list(ep["runtime"]["events"]) + list(ep["environment"]["latent_events"])

def active_support_families(ep: dict[str, Any], hypothesis: str) -> set[str]:
    active: dict[str, dict[str, Any]] = {}
    for e in sorted(all_events(ep), key=lambda x: x["step"]):
        if e["kind"] == "support":
            active[e["id"]] = e
        elif e["kind"] == "revoke":
            for ref in e.get("revokes", []):
                active.pop(ref, None)
        elif e["kind"] == "supersede":
            for ref in e.get("supersedes", []):
                active.pop(ref, None)
            # A supersede event itself is evidence for bears_on.
            active[e["id"]] = {**e, "kind": "support"}
    return {
        e["canonical_family_id"] for e in active.values()
        if e["bears_on"] == hypothesis and e.get("independent", True)
    }

def validate_topology(ep: dict[str, Any]) -> None:
    env = ep["environment"]
    topology = env["search_topology"]
    runtime_ids = {e["id"] for e in ep["runtime"]["events"]}
    latent_ids = {e["id"] for e in env["latent_events"]}
    require(runtime_ids.isdisjoint(latent_ids), f"{ep['id']}: latent evidence visible in runtime")

    if latent_ids:
        require(topology["mode"] == "gated_latent_frontier", f"{ep['id']}: latent mode")
        require(not topology["frontier_exhausted_initially"], f"{ep['id']}: latent marked exhausted")
        require(set(topology["latent_frontier"]) == latent_ids, f"{ep['id']}: latent frontier mismatch")
        require(set(topology["initial_frontier"]) == runtime_ids, f"{ep['id']}: initial frontier mismatch")
        opportunities = set(topology["expansion_opportunities"])
        require(opportunities, f"{ep['id']}: missing expansion opportunity")
        require(opportunities <= {
            "NEW_ELIGIBLE_NODE_AVAILABLE","NEW_ADMISSIBLE_PATH_AVAILABLE",
            "BUDGET_BOUND_AND_EXPANDABLE","DEPENDENCY_STATE_CHANGED",
            "EXTERNAL_EVIDENCE_ARRIVED"
        }, f"{ep['id']}: invalid positive opportunity")
        edges = topology["edges"]
        require({x["to"] for x in edges} == latent_ids, f"{ep['id']}: latent edges incomplete")
        for edge in edges:
            require(edge["from"] in runtime_ids, f"{ep['id']}: latent edge lacks visible anchor")
            require(edge["admissible"] is True, f"{ep['id']}: latent edge inadmissible")
            require(edge["initially_reachable"] is False, f"{ep['id']}: latent already reachable")
            require(edge["requires_expansion"] is True, f"{ep['id']}: latent does not require expansion")
            require(edge["opportunity_class"] in opportunities, f"{ep['id']}: opportunity mismatch")
    else:
        require(topology["mode"] == "fully_visible", f"{ep['id']}: no-latent mode mismatch")
        require(topology["frontier_exhausted_initially"] is True, f"{ep['id']}: no-latent frontier not exhausted")
        require(topology["latent_frontier"] == [], f"{ep['id']}: unexpected latent frontier")
        require(topology["edges"] == [], f"{ep['id']}: exhausted topology has hidden edges")
        require(topology["expansion_opportunities"] == [], f"{ep['id']}: exhausted topology has opportunity")

def validate_event_contract(ep: dict[str, Any]) -> None:
    events = all_events(ep)
    ids = [e["id"] for e in events]
    require(len(ids) == len(set(ids)), f"{ep['id']}: duplicate evidence id")
    seen: set[str] = set()
    for e in sorted(events, key=lambda x: x["step"]):
        require(e["step"] >= 1, f"{ep['id']}: invalid step")
        require(e["bears_on"] in {"H1","H2"}, f"{ep['id']}: invalid hypothesis")
        require(e["canonical_family_id"] == f"family:{e['raw_family_id']}", f"{ep['id']}: family namespace mismatch")
        for dep in e.get("depends_on", []):
            require(dep in seen, f"{ep['id']}: dependency not prior/existing: {dep}")
        for ref in e.get("revokes", []):
            require(ref in seen, f"{ep['id']}: revocation target not prior/existing: {ref}")
        for ref in e.get("supersedes", []):
            require(ref in seen, f"{ep['id']}: supersession target not prior/existing: {ref}")
        seen.add(e["id"])

    visible_steps=[e["step"] for e in ep["runtime"]["events"]]
    require(visible_steps == sorted(visible_steps), f"{ep['id']}: visible events out of step order")

def validate_family_semantics(ep: dict[str, Any]) -> None:
    fam = ep["task_family"]
    runtime = ep["runtime"]["events"]
    latent = ep["environment"]["latent_events"]
    oracle = ep["oracle"]
    primary = ep["mirror_primary"]
    alt = "H2" if primary == "H1" else "H1"
    all_e = all_events(ep)

    if fam in {"exact_symmetric_tie","label_mirror_tie","node_id_permutation_tie",
               "target_insertion_permutation","evidence_order_permutation",
               "family_name_permutation"}:
        supports=Counter(e["bears_on"] for e in all_e if e["kind"]=="support" and e["independent"])
        opp=Counter(e["bears_on"] for e in all_e if e["kind"]=="refute")
        require(supports["H1"] == supports["H2"] == 1, f"{ep['id']}: exact tie support imbalance")
        require(opp["H1"] == opp["H2"] == 0, f"{ep['id']}: exact tie opposition imbalance")
        require(oracle["expected_terminal_operative"] is None, f"{ep['id']}: exact tie operative oracle")
        require(oracle["expected_terminal_committed"] is None, f"{ep['id']}: exact tie commitment oracle")
        require(oracle["expected_abstention"] is True, f"{ep['id']}: exact tie abstention oracle")

    if fam == "near_tie":
        supports=Counter(e["bears_on"] for e in all_e if e["kind"]=="support" and e["independent"])
        require(supports[primary] == 2 and supports[alt] == 1, f"{ep['id']}: near tie not outside tolerance")
        require(oracle["expected_terminal_operative"] == primary, f"{ep['id']}: near tie oracle")

    if fam == "duplicate_correlated_support":
        correlated=[e for e in runtime if e["kind"]=="support" and not e["independent"]]
        require(len(correlated) == 2, f"{ep['id']}: duplicate family lacks correlated duplicates")
        require(len({e["canonical_family_id"] for e in correlated}) == 1, f"{ep['id']}: duplicates marked separate families")

    if fam == "explicit_revocation":
        rev=[e for e in runtime if e["kind"]=="revoke"]
        require(len(rev)==1 and len(rev[0].get("revokes",[]))==2, f"{ep['id']}: revocation structure")
        require(len(active_support_families(ep, primary)) >= 2, f"{ep['id']}: revocation terminal support")

    if fam == "explicit_supersession":
        sup=[e for e in runtime if e["kind"]=="supersede"]
        require(len(sup)==1 and len(sup[0].get("supersedes",[]))==1, f"{ep['id']}: supersession structure")
        require(len(active_support_families(ep, primary)) >= 2, f"{ep['id']}: supersession lacks resolution support")

    if fam == "refutation_no_replacement":
        require(any(e["kind"]=="refute" and e["bears_on"]==primary for e in runtime), f"{ep['id']}: missing refutation")
        require(oracle["expected_status"]=="contested" and oracle["expected_terminal_operative"] is None, f"{ep['id']}: frozen contested rule")

    if fam == "refutation_insufficient_replacement":
        require(any(e["kind"]=="refute" and e["bears_on"]==primary for e in runtime), f"{ep['id']}: missing incumbent refutation")
        require(len(active_support_families(ep, alt)) == 1, f"{ep['id']}: replacement not insufficient")
        require(oracle["expected_status"]=="contested" and oracle["expected_terminal_operative"] is None, f"{ep['id']}: insufficient replacement oracle")

    if fam in {"refutation_sufficient_replacement","delayed_sufficient_replacement","earned_revision"}:
        rev=oracle["expected_revision"]
        require(oracle["revision_required"] is True and rev is not None, f"{ep['id']}: missing revision oracle")
        prior_incumbent_support=[
            e for e in all_e
            if e["kind"]=="support" and e["bears_on"]==rev["from"]
            and e["independent"] and e["step"] < rev["trigger_step"]
        ]
        require(
            len({e["canonical_family_id"] for e in prior_incumbent_support}) >= 2,
            f"{ep['id']}: revision lacks prior justified incumbent"
        )
        refute_steps=[e["step"] for e in runtime if e["kind"]=="refute" and e["bears_on"]==rev["from"]]
        require(refute_steps and min(refute_steps)==rev["trigger_step"], f"{ep['id']}: revision trigger")
        replacement_support=[e for e in all_e if e["kind"]=="support" and e["bears_on"]==rev["to"] and e["independent"]]
        require(len({e["canonical_family_id"] for e in replacement_support}) >= 2, f"{ep['id']}: insufficient revision replacement")
        require(rev["replacement_earned_step"] > rev["trigger_step"], f"{ep['id']}: replacement precedes defeat")

    if fam == "correlated_majority_independent_minority":
        alt_support=[e for e in runtime if e["kind"]=="support" and e["bears_on"]==alt]
        primary_support=[e for e in runtime if e["kind"]=="support" and e["bears_on"]==primary]
        require(len(alt_support) > len(primary_support), f"{ep['id']}: no correlated raw majority")
        require(len({e["canonical_family_id"] for e in primary_support if e["independent"]}) == 2, f"{ep['id']}: independent minority malformed")

    if fam == "dwm_latent_evidence_positive":
        require(len(latent)==2, f"{ep['id']}: positive control latent count")
        require(oracle["expected_dwm_useful_reopen"] is True, f"{ep['id']}: positive usefulness oracle")
        require(oracle["revision_required"] is True, f"{ep['id']}: positive revision structure")

    if fam == "dwm_exhausted_frontier_negative":
        require(not latent, f"{ep['id']}: exhausted case contains latent evidence")
        require(ep["environment"]["search_topology"]["frontier_exhausted_initially"] is True, f"{ep['id']}: exhausted flag")
        require(oracle["expected_dwm_useful_reopen"] is False, f"{ep['id']}: exhausted usefulness oracle")

    if fam == "dwm_insufficiency_no_opposition":
        require(not [e for e in runtime if e["kind"]=="refute"], f"{ep['id']}: insufficiency has opposition")
        require(oracle["challenge_warranted_steps"] == [], f"{ep['id']}: insufficiency marked challenge")
        require(oracle["corroboration_search_warranted_steps"], f"{ep['id']}: missing corroboration oracle")
        require(latent, f"{ep['id']}: insufficiency positive search lacks latent evidence")

    if fam == "dwm_genuine_opposition":
        require(any(e["kind"]=="refute" and e["material"] for e in runtime), f"{ep['id']}: genuine opposition missing")
        require(latent, f"{ep['id']}: genuine opposition lacks latent expansion")

    if fam == "earned_resolution":
        require(oracle["resolution_eligible"] is True, f"{ep['id']}: resolution eligibility")
        require(len(active_support_families(ep, primary)) >= 2, f"{ep['id']}: resolution lacks two independent families")
        require(not any(e["kind"]=="refute" and e["bears_on"]==primary for e in all_e), f"{ep['id']}: resolution contradicted")
        require(all(e["semantic_verification"] for e in all_e if e["bears_on"]==primary), f"{ep['id']}: resolution verification")

    if fam == "single_step_control":
        require(len(runtime)==1 and not latent, f"{ep['id']}: single-step not single")

    if fam == "no_change_control":
        require(len(runtime)==3 and runtime[-1].get("independent") is False, f"{ep['id']}: no-change structure")
        require(runtime[-1].get("depends_on"), f"{ep['id']}: no-change duplicate lacks lineage")
        require(oracle["revision_required"] is False, f"{ep['id']}: no-change revision oracle")

def validate_document(doc: dict[str, Any], *, strict_full_universe: bool = True) -> dict[str, Any]:
    require(doc.get("schema")=="epistemic-process-v3-task-universe-v1", "schema")
    require(doc.get("status")=="UNSCORED_PREREGISTERED_CANDIDATE", "status")
    require(doc.get("experiment_id_candidate")=="EP-PROCESS-V3-SCORE-001", "experiment id")
    require(doc.get("generator_seed")==20261003, "generator seed")
    require(doc.get("score_bearing_authorized") is False, "score authorization")
    require(doc.get("canonical_family_id")=="family:<raw-id>", "canonical family id")

    episodes=doc.get("episodes",[])
    if strict_full_universe:
        require(len(episodes)==384 and doc.get("episode_count")==384, "episode count")
        require(doc.get("family_count")==24, "family count")
        require(doc.get("episodes_per_family")==16, "episodes per family")
        require(doc.get("families")==EXPECTED_FAMILIES, "family order/set")
    ids=[e.get("id") for e in episodes]
    require(len(ids)==len(set(ids)), "duplicate episode id")

    counts=Counter(e["task_family"] for e in episodes)
    if strict_full_universe:
        require(set(counts)==set(EXPECTED_FAMILIES), "missing/extra family")
        require(all(counts[f]==16 for f in EXPECTED_FAMILIES), "per-family count")

    for family, rows in defaultdict(list, {f:[] for f in EXPECTED_FAMILIES}).items():
        pass
    grouped=defaultdict(list)
    for ep in episodes:
        grouped[ep["task_family"]].append(ep)
        require(ep["mirror_primary"] in {"H1","H2"}, f"{ep['id']}: mirror primary")
        for path,key,_ in walk_keys(ep["runtime"]):
            require(key not in FORBIDDEN_RUNTIME_KEYS, f"{ep['id']}: forbidden runtime key {path}.{key}")
        validate_event_contract(ep)
        validate_topology(ep)
        validate_family_semantics(ep)

    if strict_full_universe:
        for family in EXPECTED_FAMILIES:
            primaries=Counter(e["mirror_primary"] for e in grouped[family])
            require(primaries=={"H1":8,"H2":8}, f"{family}: mirror balance {primaries}")
            variants=sorted(e["variant"] for e in grouped[family])
            require(variants==list(range(1,17)), f"{family}: variants")
            groups=Counter(e["mirror_group"] for e in grouped[family])
            require(len(groups)==8 and all(v==2 for v in groups.values()), f"{family}: mirror groups")

    summary={
        "episodes":len(episodes),"families":len(counts),
        "events":sum(len(all_events(e)) for e in episodes),
        "latent_positive_controls":sum(bool(e["environment"]["latent_events"]) for e in episodes),
        "exhausted_frontier_controls":sum(e["task_family"]=="dwm_exhausted_frontier_negative" for e in episodes),
        "revision_required":sum(bool(e["oracle"]["revision_required"]) for e in episodes),
        "resolution_eligible":sum(bool(e["oracle"]["resolution_eligible"]) for e in episodes),
        "abstention_required":sum(bool(e["oracle"]["expected_abstention"]) for e in episodes),
    }
    return summary

def main() -> None:
    doc=load(TASKS)
    summary=validate_document(doc)
    print("cycle3_task_validation=passed "+json.dumps(summary,sort_keys=True))

if __name__=="__main__":
    main()
