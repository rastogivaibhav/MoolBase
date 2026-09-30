#!/usr/bin/env python3
from __future__ import annotations

import copy,json
from pathlib import Path
from validate_tasks_v3_candidate import ValidationError, validate_document

ROOT=Path(__file__).resolve().parent
BASE=json.loads((ROOT/"tasks_v3_candidate.json").read_text())

def first(family):
    return next(e for e in BASE["episodes"] if e["task_family"]==family)

def idx(family,variant=1):
    return next(i for i,e in enumerate(BASE["episodes"]) if e["task_family"]==family and e["variant"]==variant)

def expect_rejected(name, mutate):
    doc=copy.deepcopy(BASE)
    mutate(doc)
    try:
        validate_document(doc)
    except (ValidationError,AssertionError,KeyError,TypeError,ValueError):
        return
    raise AssertionError(f"mutation accepted: {name}")

cases=[]

cases.append(("duplicate_episode_id",lambda d: d["episodes"].__setitem__(1,{**d["episodes"][1],"id":d["episodes"][0]["id"]})))
cases.append(("missing_family",lambda d: d["episodes"].__setitem__(slice(None),[e for e in d["episodes"] if e["task_family"]!="near_tie"])))
def imbalance(d):
    d["episodes"][idx("exact_symmetric_tie",16)]["task_family"]="near_tie"
cases.append(("family_15_17_imbalance",imbalance))
cases.append(("broken_mirror_balance",lambda d: d["episodes"][idx("exact_symmetric_tie",2)].__setitem__("mirror_primary","H1")))
def node_changes_evidence(d):
    d["episodes"][idx("node_id_permutation_tie",2)]["runtime"]["events"][1]["bears_on"]="H1"
cases.append(("node_permutation_changes_evidence",node_changes_evidence))
def rename_changes_independence(d):
    d["episodes"][idx("family_name_permutation",1)]["runtime"]["events"][1]["independent"]=False
cases.append(("family_rename_changes_independence",rename_changes_independence))
cases.append(("oracle_leaked_runtime",lambda d: d["episodes"][0]["runtime"].__setitem__("expected_terminal_answer","H1")))
def future_visible(d):
    ep=d["episodes"][idx("dwm_latent_evidence_positive",1)]
    ep["runtime"]["events"].append(copy.deepcopy(ep["environment"]["latent_events"][0]))
cases.append(("future_observation_visible",future_visible))
def bad_revoke(d):
    ep=d["episodes"][idx("explicit_revocation",1)]
    next(e for e in ep["runtime"]["events"] if e["kind"]=="revoke")["revokes"]=["MISSING"]
cases.append(("revocation_missing_target",bad_revoke))
def early_revoke(d):
    ep=d["episodes"][idx("explicit_revocation",1)]
    next(e for e in ep["runtime"]["events"] if e["kind"]=="revoke")["step"]=0
cases.append(("revocation_before_target",early_revoke))
def bad_super(d):
    ep=d["episodes"][idx("explicit_supersession",1)]
    next(e for e in ep["runtime"]["events"] if e["kind"]=="supersede")["supersedes"]=["MISSING"]
cases.append(("supersession_without_predecessor",bad_super))
def latent_visible(d):
    ep=d["episodes"][idx("dwm_latent_evidence_positive",1)]
    ep["environment"]["search_topology"]["initial_frontier"].append(ep["environment"]["latent_events"][0]["id"])
cases.append(("latent_already_visible",latent_visible))
def exhausted_has_hidden(d):
    ep=d["episodes"][idx("dwm_exhausted_frontier_negative",1)]
    fake=copy.deepcopy(ep["runtime"]["events"][0]); fake["id"]="ILLEGAL_LATENT"; fake["step"]=99
    ep["environment"]["latent_events"].append(fake)
cases.append(("exhausted_contains_hidden",exhausted_has_hidden))
def no_initial_belief(d):
    ep=d["episodes"][idx("earned_revision",1)]
    ep["runtime"]["events"][1]["independent"]=False
    ep["runtime"]["events"][1]["raw_family_id"]=ep["runtime"]["events"][0]["raw_family_id"]
    ep["runtime"]["events"][1]["canonical_family_id"]=ep["runtime"]["events"][0]["canonical_family_id"]
cases.append(("revision_no_prior_operative_support",no_initial_belief))
def replacement_too_early(d):
    ep=d["episodes"][idx("earned_revision",1)]
    repl=ep["oracle"]["expected_revision"]["to"]
    next(e for e in ep["runtime"]["events"] if e["kind"]=="support" and e["bears_on"]==repl)["step"]=2
cases.append(("replacement_before_contradiction",replacement_too_early))
def bad_resolution(d):
    ep=d["episodes"][idx("earned_resolution",1)]
    ep["runtime"]["events"][1]["independent"]=False
    ep["runtime"]["events"][1]["raw_family_id"]=ep["runtime"]["events"][0]["raw_family_id"]
    ep["runtime"]["events"][1]["canonical_family_id"]=ep["runtime"]["events"][0]["canonical_family_id"]
cases.append(("resolution_missing_two_families",bad_resolution))
def unequal_tie(d):
    d["episodes"][idx("exact_symmetric_tie",1)]["runtime"]["events"][1]["kind"]="noop"
cases.append(("exact_tie_unequal_coordinates",unequal_tie))
def near_inside_tie(d):
    ep=d["episodes"][idx("near_tie",1)]
    primary=ep["mirror_primary"]
    supports=[e for e in ep["runtime"]["events"] if e["kind"]=="support" and e["bears_on"]==primary]
    supports[-1]["independent"]=False
    supports[-1]["raw_family_id"]=supports[0]["raw_family_id"]
    supports[-1]["canonical_family_id"]=supports[0]["canonical_family_id"]
cases.append(("near_tie_inside_tolerance_structure",near_inside_tie))
def duplicate_marked_independent(d):
    ep=d["episodes"][idx("duplicate_correlated_support",1)]
    for e in ep["runtime"]["events"]:
        if not e["independent"]: e["independent"]=True
cases.append(("correlated_duplicates_marked_independent",duplicate_marked_independent))
def metadata_mismatch(d):
    d["episodes"][idx("no_change_control",1)]["task_family"]="single_step_control"
cases.append(("family_metadata_topology_mismatch",metadata_mismatch))

assert len(cases)==20
for name,mutation in cases:
    expect_rejected(name,mutation)
print("cycle3_adversarial_mutations=passed rejected=20")
