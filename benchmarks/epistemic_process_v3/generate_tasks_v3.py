#!/usr/bin/env python3
"""Deterministically construct the EP-PROCESS-V3 384-episode blind task universe.

Cycle 3 is structural only. This generator does not execute MoolBase and never
consults model/runtime output. Family-local deterministic derivation prevents
global RNG order from changing another family's content.
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
from typing import Any

GENERATOR_SEED = 20261003
SCHEMA = "epistemic-process-v3-task-universe-v1"
STATUS = "UNSCORED_PREREGISTERED_CANDIDATE"
EXPERIMENT = "EP-PROCESS-V3-SCORE-001"

FAMILIES = [
    "exact_symmetric_tie",
    "near_tie",
    "label_mirror_tie",
    "node_id_permutation_tie",
    "target_insertion_permutation",
    "evidence_order_permutation",
    "family_name_permutation",
    "duplicate_correlated_support",
    "valid_persistent_history",
    "explicit_revocation",
    "explicit_supersession",
    "refutation_no_replacement",
    "refutation_insufficient_replacement",
    "refutation_sufficient_replacement",
    "delayed_sufficient_replacement",
    "correlated_majority_independent_minority",
    "dwm_latent_evidence_positive",
    "dwm_exhausted_frontier_negative",
    "dwm_insufficiency_no_opposition",
    "dwm_genuine_opposition",
    "earned_revision",
    "earned_resolution",
    "single_step_control",
    "no_change_control",
]

FAMILY_PURPOSE = {
    "exact_symmetric_tie": "exact symmetry; identity must not select a target",
    "near_tie": "small real semantic advantage outside frozen tolerance",
    "label_mirror_tie": "H1/H2 label-mirror invariance",
    "node_id_permutation_tie": "numeric-node identity invariance",
    "target_insertion_permutation": "target creation-order invariance",
    "evidence_order_permutation": "semantically irrelevant evidence-order invariance",
    "family_name_permutation": "family-name/lexical invariance",
    "duplicate_correlated_support": "independence accounting under duplicate support",
    "valid_persistent_history": "valid historical evidence remains useful",
    "explicit_revocation": "revoked evidence leaves active positive reasoning",
    "explicit_supersession": "superseded evidence becomes audit-only/non-operative",
    "refutation_no_replacement": "defeated incumbent with no credible replacement",
    "refutation_insufficient_replacement": "contested/null-operative frozen replacement policy",
    "refutation_sufficient_replacement": "corroborated replacement can take over",
    "delayed_sufficient_replacement": "replacement earns support after a contested interval",
    "correlated_majority_independent_minority": "independent minority defeats correlated count majority",
    "dwm_latent_evidence_positive": "real hidden admissible evidence exists beyond initial frontier",
    "dwm_exhausted_frontier_negative": "uncertainty/opposition with genuinely exhausted frontier",
    "dwm_insufficiency_no_opposition": "corroboration search without dialectical opposition",
    "dwm_genuine_opposition": "real opposition plus real search expansion opportunity",
    "earned_revision": "old justified belief is defeated then natively replaced",
    "earned_resolution": "full frozen resolution requirements are structurally satisfiable",
    "single_step_control": "simple non-history regression control",
    "no_change_control": "irrelevant/duplicate update must not perturb state",
}

FAMILY_CLAIMS = {
    "exact_symmetric_tie": ["CLAIM-TIE-SYMMETRY"],
    "near_tie": ["CLAIM-TIE-SYMMETRY", "CLAIM-HYPOKOSH-SELECTION"],
    "label_mirror_tie": ["CLAIM-TIE-SYMMETRY"],
    "node_id_permutation_tie": ["CLAIM-TIE-SYMMETRY"],
    "target_insertion_permutation": ["CLAIM-TIE-SYMMETRY"],
    "evidence_order_permutation": ["CLAIM-TIE-SYMMETRY"],
    "family_name_permutation": ["CLAIM-TIE-SYMMETRY"],
    "duplicate_correlated_support": ["CLAIM-HYPOKOSH-SELECTION"],
    "valid_persistent_history": ["CLAIM-PERSISTENCE"],
    "explicit_revocation": ["CLAIM-INVALIDATION"],
    "explicit_supersession": ["CLAIM-INVALIDATION"],
    "refutation_no_replacement": ["CLAIM-HYPOKOSH-SELECTION"],
    "refutation_insufficient_replacement": ["CLAIM-HYPOKOSH-SELECTION"],
    "refutation_sufficient_replacement": ["CLAIM-HYPOKOSH-SELECTION", "CLAIM-REVISION"],
    "delayed_sufficient_replacement": ["CLAIM-REVISION"],
    "correlated_majority_independent_minority": ["CLAIM-HYPOKOSH-SELECTION"],
    "dwm_latent_evidence_positive": ["CLAIM-DWM-PRECISION", "CLAIM-DWM-USEFULNESS", "CLAIM-DWM-SAFETY", "CLAIM-DWM-BROAD-OUTCOME"],
    "dwm_exhausted_frontier_negative": ["CLAIM-DWM-PRECISION", "CLAIM-DWM-USEFULNESS"],
    "dwm_insufficiency_no_opposition": ["CLAIM-DWM-PRECISION"],
    "dwm_genuine_opposition": ["CLAIM-DWM-PRECISION", "CLAIM-DWM-USEFULNESS", "CLAIM-DWM-SAFETY", "CLAIM-DWM-BROAD-OUTCOME"],
    "earned_revision": ["CLAIM-REVISION"],
    "earned_resolution": ["CLAIM-EARNED-RESOLUTION"],
    "single_step_control": ["CLAIM-PERSISTENCE", "CLAIM-HYPOKOSH-SELECTION"],
    "no_change_control": ["CLAIM-REVISION", "CLAIM-DWM-PRECISION"],
}

def stable_token(family: str, variant: int, purpose: str, length: int = 10) -> str:
    material = f"{GENERATOR_SEED}:{family}:{variant}:{purpose}".encode("utf-8")
    return hashlib.sha256(material).hexdigest()[:length].upper()

def mirror_primary(variant: int) -> str:
    return "H1" if variant % 2 == 1 else "H2"

def other(hypothesis: str) -> str:
    return "H2" if hypothesis == "H1" else "H1"

def prefix(family: str, variant: int) -> str:
    return f"V3_{family.upper()}_{variant:02d}"

def family_ids(family: str, variant: int):
    p = prefix(family, variant)
    salt = stable_token(family, variant, "families", 6)
    def raw(suffix: str) -> str:
        return f"{p}_{salt}_{suffix}"
    def canonical(suffix: str) -> str:
        return f"family:{raw(suffix)}"
    return raw, canonical

def event(step: int, event_id: str, family_id: str, kind: str, bears_on: str,
          *, independent: bool = True, depends_on: list[str] | None = None,
          revokes: list[str] | None = None, supersedes: list[str] | None = None,
          semantic_verification: bool = True, material: bool = False,
          delivery_epoch: int | None = None) -> dict[str, Any]:
    row: dict[str, Any] = {
        "step": step, "id": event_id, "raw_family_id": family_id,
        "canonical_family_id": f"family:{family_id}", "kind": kind,
        "bears_on": bears_on, "independent": independent,
        "semantic_verification": semantic_verification, "material": material,
        "delivery_epoch": step if delivery_epoch is None else delivery_epoch,
    }
    if depends_on: row["depends_on"] = list(depends_on)
    if revokes: row["revokes"] = list(revokes)
    if supersedes: row["supersedes"] = list(supersedes)
    return row

def target_layout(family: str, variant: int, primary: str) -> dict[str, Any]:
    lower_h1 = ((variant - 1) % 4) < 2
    order = ["H1", "H2"] if lower_h1 else ["H2", "H1"]
    pad_count = int(stable_token(family, variant, "node-padding", 2), 16) % 3
    return {
        "target_insertion_order": order,
        "lower_numeric_target": order[0],
        "padding_nodes_before_targets": pad_count,
        "semantic_labels": {"H1": "H1", "H2": "H2"},
    }

def topology_none(events: list[dict[str, Any]]) -> dict[str, Any]:
    return {
        "mode": "fully_visible",
        "initial_frontier": [e["id"] for e in events],
        "latent_frontier": [], "edges": [],
        "frontier_exhausted_initially": True,
        "expansion_opportunities": [],
    }

def topology_latent(visible_events: list[dict[str, Any]],
                    latent_events: list[dict[str, Any]], *,
                    opportunity_class: str, anchor_event_id: str) -> dict[str, Any]:
    return {
        "mode": "gated_latent_frontier",
        "initial_frontier": [e["id"] for e in visible_events],
        "latent_frontier": [e["id"] for e in latent_events],
        "edges": [{
            "from": anchor_event_id, "to": e["id"], "admissible": True,
            "initially_reachable": False, "requires_expansion": True,
            "opportunity_class": opportunity_class,
        } for e in latent_events],
        "frontier_exhausted_initially": False,
        "expansion_opportunities": [opportunity_class],
        "unlock_contract": {
            "kind": "reopen", "anchor_event_id": anchor_event_id,
            "reveals": [e["id"] for e in latent_events],
        },
    }

def oracle(*, operative: str | None, committed: str | None, status: str,
           abstain: bool, challenge_steps: list[int] | None = None,
           corroboration_steps: list[int] | None = None,
           revision_required: bool = False, revision_from: str | None = None,
           revision_to: str | None = None, revision_trigger_step: int | None = None,
           replacement_earned_step: int | None = None,
           resolution_eligible: bool = False,
           expected_useful_reopen: bool = False,
           expected_opportunity_class: str | None = None,
           latent_evidence_ids: list[str] | None = None) -> dict[str, Any]:
    return {
        "expected_terminal_operative": operative,
        "expected_terminal_committed": committed,
        "expected_status": status, "expected_abstention": abstain,
        "challenge_warranted_steps": challenge_steps or [],
        "corroboration_search_warranted_steps": corroboration_steps or [],
        "revision_required": revision_required,
        "expected_revision": None if not revision_required else {
            "from": revision_from, "to": revision_to,
            "trigger_step": revision_trigger_step,
            "replacement_earned_step": replacement_earned_step,
            "max_latency_steps": 2,
        },
        "resolution_eligible": resolution_eligible,
        "expected_dwm_useful_reopen": expected_useful_reopen,
        "expected_expansion_opportunity_class": expected_opportunity_class,
        "latent_evidence_ids": latent_evidence_ids or [],
    }

def make_episode(family: str, variant: int) -> dict[str, Any]:
    primary = mirror_primary(variant)
    alternative = other(primary)
    p = prefix(family, variant)
    raw, _ = family_ids(family, variant)
    evid = lambda n: f"{p}_e{n}"
    layout = target_layout(family, variant, primary)
    visible: list[dict[str, Any]] = []
    latent: list[dict[str, Any]] = []

    if family in {"exact_symmetric_tie","label_mirror_tie","node_id_permutation_tie",
                  "target_insertion_permutation","family_name_permutation"}:
        visible = [event(1,evid(1),raw("A"),"support","H1"),
                   event(2,evid(2),raw("B"),"support","H2")]
        if family == "family_name_permutation":
            a=stable_token(family,variant,"rename-a",8); b=stable_token(family,variant,"rename-b",8)
            visible[0]["raw_family_id"]=f"{p}_{a}"; visible[0]["canonical_family_id"]=f"family:{p}_{a}"
            visible[1]["raw_family_id"]=f"{p}_{b}"; visible[1]["canonical_family_id"]=f"family:{p}_{b}"
        o=oracle(operative=None,committed=None,status="open",abstain=True)
    elif family == "near_tie":
        visible=[event(1,evid(1),raw("A"),"support",primary),
                 event(2,evid(2),raw("B"),"support",alternative),
                 event(3,evid(3),raw("C"),"support",primary)]
        o=oracle(operative=primary,committed=primary,status="provisionally_resolved",abstain=False)
    elif family == "evidence_order_permutation":
        first,second=("H1","H2") if variant%2==1 else ("H2","H1")
        fam_by_h={"H1":raw("A"),"H2":raw("B")}; id_by_h={"H1":evid(1),"H2":evid(2)}
        visible=[event(1,id_by_h[first],fam_by_h[first],"support",first,delivery_epoch=1),
                 event(2,id_by_h[second],fam_by_h[second],"support",second,delivery_epoch=1)]
        o=oracle(operative=None,committed=None,status="open",abstain=True)
    elif family == "duplicate_correlated_support":
        visible=[event(1,evid(1),raw("A"),"support",primary),
                 event(2,evid(2),raw("A"),"support",primary,independent=False,depends_on=[evid(1)]),
                 event(3,evid(3),raw("A"),"support",primary,independent=False,depends_on=[evid(1)]),
                 event(4,evid(4),raw("B"),"support",alternative),
                 event(5,evid(5),raw("C"),"support",primary)]
        o=oracle(operative=primary,committed=primary,status="resolved",abstain=False,resolution_eligible=True)
    elif family == "valid_persistent_history":
        visible=[event(1,evid(1),raw("A"),"support",primary),
                 event(2,evid(2),raw("B"),"support",primary),
                 event(3,evid(3),raw("C"),"support",alternative),
                 event(4,evid(4),raw("C"),"support",alternative,independent=False,depends_on=[evid(3)])]
        o=oracle(operative=primary,committed=primary,status="resolved",abstain=False,resolution_eligible=True)
    elif family == "explicit_revocation":
        visible=[event(1,evid(1),raw("A"),"support",alternative),
                 event(2,evid(2),raw("B"),"support",alternative),
                 event(3,evid(3),raw("C"),"support",primary),
                 event(4,evid(4),raw("R"),"revoke",alternative,revokes=[evid(1),evid(2)],material=True),
                 event(5,evid(5),raw("D"),"support",primary)]
        o=oracle(operative=primary,committed=primary,status="resolved",abstain=False,resolution_eligible=True)
    elif family == "explicit_supersession":
        visible=[event(1,evid(1),raw("A"),"support",alternative),
                 event(2,evid(2),raw("A2"),"supersede",primary,supersedes=[evid(1)],material=True),
                 event(3,evid(3),raw("B"),"support",primary),
                 event(4,evid(4),raw("C"),"support",primary)]
        o=oracle(operative=primary,committed=primary,status="resolved",abstain=False,resolution_eligible=True)
    elif family == "refutation_no_replacement":
        incumbent=primary
        visible=[event(1,evid(1),raw("A"),"support",incumbent),
                 event(2,evid(2),raw("B"),"support",incumbent),
                 event(3,evid(3),raw("R"),"refute",incumbent,material=True)]
        o=oracle(operative=None,committed=None,status="contested",abstain=True,challenge_steps=[3])
    elif family == "refutation_insufficient_replacement":
        incumbent,replacement=primary,alternative
        visible=[event(1,evid(1),raw("A"),"support",incumbent),
                 event(2,evid(2),raw("B"),"support",incumbent),
                 event(3,evid(3),raw("R"),"refute",incumbent,material=True),
                 event(4,evid(4),raw("C"),"support",replacement)]
        o=oracle(operative=None,committed=None,status="contested",abstain=True,challenge_steps=[3])
    elif family in {"refutation_sufficient_replacement","earned_revision"}:
        incumbent,replacement=alternative,primary
        visible=[event(1,evid(1),raw("A"),"support",incumbent),
                 event(2,evid(2),raw("B"),"support",incumbent),
                 event(3,evid(3),raw("R"),"refute",incumbent,material=True),
                 event(4,evid(4),raw("C"),"support",replacement),
                 event(5,evid(5),raw("D"),"support",replacement)]
        o=oracle(operative=replacement,committed=replacement,status="resolved",abstain=False,
                 challenge_steps=[3],revision_required=True,revision_from=incumbent,
                 revision_to=replacement,revision_trigger_step=3,replacement_earned_step=5,
                 resolution_eligible=True)
    elif family == "delayed_sufficient_replacement":
        incumbent,replacement=alternative,primary
        visible=[event(1,evid(1),raw("A"),"support",incumbent),
                 event(2,evid(2),raw("B"),"support",incumbent),
                 event(3,evid(3),raw("R"),"refute",incumbent,material=True),
                 event(4,evid(4),raw("C"),"support",replacement),
                 event(5,evid(5),raw("N"),"noop",incumbent,independent=False,depends_on=[evid(1)]),
                 event(6,evid(6),raw("D"),"support",replacement)]
        o=oracle(operative=replacement,committed=replacement,status="resolved",abstain=False,
                 challenge_steps=[3],revision_required=True,revision_from=incumbent,
                 revision_to=replacement,revision_trigger_step=3,replacement_earned_step=6,
                 resolution_eligible=True)
    elif family == "correlated_majority_independent_minority":
        visible=[event(1,evid(1),raw("A"),"support",alternative),
                 event(2,evid(2),raw("A"),"support",alternative,independent=False,depends_on=[evid(1)]),
                 event(3,evid(3),raw("A"),"support",alternative,independent=False,depends_on=[evid(1)]),
                 event(4,evid(4),raw("B"),"support",primary),
                 event(5,evid(5),raw("C"),"support",primary)]
        o=oracle(operative=primary,committed=primary,status="resolved",abstain=False,resolution_eligible=True)
    elif family == "dwm_latent_evidence_positive":
        incumbent,replacement=alternative,primary
        visible=[event(1,evid(1),raw("A"),"support",incumbent),
                 event(2,evid(2),raw("B"),"support",incumbent),
                 event(3,evid(3),raw("R"),"refute",incumbent,material=True)]
        latent=[event(4,evid(4),raw("C"),"support",replacement),event(5,evid(5),raw("D"),"support",replacement)]
        o=oracle(operative=replacement,committed=replacement,status="resolved",abstain=False,
                 challenge_steps=[3],revision_required=True,revision_from=incumbent,
                 revision_to=replacement,revision_trigger_step=3,replacement_earned_step=5,
                 resolution_eligible=True,expected_useful_reopen=True,
                 expected_opportunity_class="NEW_ELIGIBLE_NODE_AVAILABLE",
                 latent_evidence_ids=[e["id"] for e in latent])
    elif family == "dwm_exhausted_frontier_negative":
        incumbent=primary
        visible=[event(1,evid(1),raw("A"),"support",incumbent),
                 event(2,evid(2),raw("B"),"support",incumbent),
                 event(3,evid(3),raw("R"),"refute",incumbent,material=True)]
        o=oracle(operative=None,committed=None,status="contested",abstain=True,challenge_steps=[3])
    elif family == "dwm_insufficiency_no_opposition":
        visible=[event(1,evid(1),raw("A"),"support",primary)]
        latent=[event(2,evid(2),raw("A"),"support",primary,independent=False,depends_on=[evid(1)])]
        o=oracle(operative=primary,committed=None,status="provisional",abstain=True,
                 corroboration_steps=[1],expected_useful_reopen=True,
                 expected_opportunity_class="NEW_ELIGIBLE_NODE_AVAILABLE",
                 latent_evidence_ids=[e["id"] for e in latent])
    elif family == "dwm_genuine_opposition":
        incumbent,replacement=alternative,primary
        visible=[event(1,evid(1),raw("A"),"support",incumbent),
                 event(2,evid(2),raw("B"),"support",incumbent),
                 event(3,evid(3),raw("R"),"refute",incumbent,material=True),
                 event(4,evid(4),raw("C"),"support",replacement)]
        latent=[event(5,evid(5),raw("D"),"support",replacement)]
        o=oracle(operative=replacement,committed=replacement,status="resolved",abstain=False,
                 challenge_steps=[3],revision_required=True,revision_from=incumbent,
                 revision_to=replacement,revision_trigger_step=3,replacement_earned_step=5,
                 resolution_eligible=True,expected_useful_reopen=True,
                 expected_opportunity_class="NEW_ADMISSIBLE_PATH_AVAILABLE",
                 latent_evidence_ids=[e["id"] for e in latent])
    elif family == "earned_resolution":
        visible=[event(1,evid(1),raw("A"),"support",primary,semantic_verification=True),
                 event(2,evid(2),raw("B"),"support",primary,semantic_verification=True),
                 event(3,evid(3),raw("B"),"support",primary,independent=False,depends_on=[evid(2)],semantic_verification=True)]
        o=oracle(operative=primary,committed=primary,status="resolved",abstain=False,resolution_eligible=True)
    elif family == "single_step_control":
        visible=[event(1,evid(1),raw("A"),"support",primary)]
        o=oracle(operative=primary,committed=None,status="provisional",abstain=True)
    elif family == "no_change_control":
        visible=[event(1,evid(1),raw("A"),"support",primary),
                 event(2,evid(2),raw("B"),"support",primary),
                 event(3,evid(3),raw("A"),"support",primary,independent=False,depends_on=[evid(1)])]
        o=oracle(operative=primary,committed=primary,status="resolved",abstain=False,resolution_eligible=True)
    else:
        raise ValueError(f"unknown family: {family}")

    if family == "node_id_permutation_tie":
        layout["target_insertion_order"]=["H1","H2"] if variant%2==1 else ["H2","H1"]
        layout["lower_numeric_target"]=layout["target_insertion_order"][0]
        layout["padding_nodes_before_targets"]=variant%3
    if family == "target_insertion_permutation":
        layout["target_insertion_order"]=["H1","H2"] if variant%2==1 else ["H2","H1"]
        layout["lower_numeric_target"]=layout["target_insertion_order"][0]

    if latent:
        topology=topology_latent(visible,latent,
            opportunity_class=o["expected_expansion_opportunity_class"],
            anchor_event_id=visible[-1]["id"])
    else:
        topology=topology_none(visible)

    return {
        "id": p, "task_family": family, "variant": variant,
        "mirror_primary": primary, "mirror_group": f"{family}:{(variant+1)//2:02d}",
        "scientific_purpose": FAMILY_PURPOSE[family], "claim_ids": FAMILY_CLAIMS[family],
        "runtime": {
            "target_layout": layout, "events": visible,
            "initial_search_limits": {
                "semantic_candidates": 4 if latent else 32,
                "max_paths": 8 if latent else 128,
                "max_visited_states": 32 if latent else 4096,
            },
        },
        "environment": {"latent_events": latent, "search_topology": topology},
        "oracle": o,
    }

def generate(families: list[str] | None = None) -> dict[str, Any]:
    selected=FAMILIES if families is None else list(families)
    episodes=[make_episode(family,variant) for family in selected for variant in range(1,17)]
    return {
        "schema": SCHEMA, "status": STATUS, "experiment_id_candidate": EXPERIMENT,
        "generator_seed": GENERATOR_SEED, "score_bearing_authorized": False,
        "episode_count": len(episodes), "family_count": len(selected),
        "episodes_per_family": 16, "mirror_balance": "8/8",
        "canonical_family_id": "family:<raw-id>",
        "runtime_oracle_separation": {
            "production_payload_path": "episodes[].runtime",
            "harness_environment_path": "episodes[].environment",
            "evaluator_only_path": "episodes[].oracle",
            "environment_is_not_reasoner_input": True,
        },
        "families": selected, "episodes": episodes,
    }

def canonical_bytes(data: dict[str, Any]) -> bytes:
    return (json.dumps(data,separators=(",",":"),sort_keys=True)+"\n").encode("utf-8")

def main() -> None:
    target=Path(__file__).with_name("tasks_v3_candidate.json")
    payload=canonical_bytes(generate())
    target.write_bytes(payload)
    digest=hashlib.sha256(payload).hexdigest()
    print(f"generated={target} episodes=384 families=24 sha256={digest} score_authorized=false")

if __name__ == "__main__":
    main()
