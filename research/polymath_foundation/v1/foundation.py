#!/usr/bin/env python3
"""Polymath Foundation v1: strict metadata gate and read-only research triage.

Research scaffolding, NOT autonomous hypothesis discovery or a source verifier.
All synthetic fixtures are explicitly unverified and no tool is executed.
Python standard library only; writes no MoolBase Verified records.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path


class ContractError(ValueError):
    pass


def require(condition, label):
    if not condition:
        raise ContractError(label)


def present_string(obj, field, context):
    require(isinstance(obj, dict) and
            isinstance(obj.get(field), str) and bool(obj[field].strip()),
            f"{context}.{field}: nonempty string required")
    return obj[field]


def array(obj, field, context, min_size=0):
    value = obj.get(field)
    require(isinstance(value, list) and len(value) >= min_size,
            f"{context}.{field}: list of at least {min_size} required")
    return value


def fraction(obj, field, context):
    value = obj.get(field)
    require(type(value) in (int, float) and math.isfinite(value) and 0 <= value <= 1,
            f"{context}.{field}: expected finite number in [0,1]")
    return float(value)


def unique_by(items, key, context):
    index = {}
    for i, item in enumerate(items):
        require(isinstance(item, dict), f"{context}[{i}] must be a record")
        identifier = present_string(item, key, f"{context}[{i}]")
        require(identifier not in index, f"{context}: duplicate {key}={identifier}")
        index[identifier] = item
    return index


def validate_package(pack):
    """Fail closed on malformed source identities, concept structure and mapping."""
    require(isinstance(pack, dict), "package object required")
    require(pack.get("schema_version") == "polymath-foundation-1",
            "wrong schema version")
    require(pack.get("dataset_kind") == "synthetic_curated_fixture",
            "v1 requires explicitly marked synthetic demo data")
    sources = unique_by(array(pack, "sources", "package", 3), "id", "sources")
    domains = unique_by(array(pack, "domains", "package", 3), "id", "domains")
    concepts = unique_by(array(pack, "concepts", "package", 6), "id", "concepts")
    mappings = unique_by(array(pack, "mappings", "package", 2), "id", "mappings")
    gaps = unique_by(array(pack, "knowledge_gaps", "package", 1),
                     "id", "knowledge_gaps")
    tests = unique_by(array(pack, "competence_tests", "package", 3),
                      "id", "competence_tests")

    for sid, s in sources.items():
        for key in ("family_id", "kind", "uri", "license", "integrity", "methodology"):
            present_string(s, key, f"source {sid}")
        require(s["kind"] == "synthetic_fixture" and
                s["integrity"] == "not_authenticated",
                f"source {sid}: cannot claim independently authenticated demo evidence")
        require(s["uri"].startswith("fixture://"), f"source {sid}: fixture URI required")

    for did, d in domains.items():
        present_string(d, "ontology_version", f"domain {did}")
        test_ids = array(d, "competence_test_ids", f"domain {did}", 1)
        require(len(test_ids) == len(set(test_ids)),
                f"domain {did}: repeated competence test")

    for cid, c in concepts.items():
        context = f"concept {cid}"
        require(present_string(c, "domain", context) in domains,
                f"{context}: unknown domain")
        require(cid.startswith(c["domain"] + ":"),
                f"{context}: domain-qualified identifier required")
        present_string(c, "label", context)
        present_string(c, "definition", context)
        for key, minimum in (("roles", 1), ("prerequisites", 0),
                             ("assumptions", 1), ("boundaries", 1), ("source_ids", 1)):
            items = array(c, key, context, minimum)
            require(all(isinstance(v, str) and v.strip() for v in items),
                    f"{context}.{key}: empty/string-invalid entry")
            require(len(items) == len(set(items)), f"{context}.{key}: duplicate")
        for source_id in c["source_ids"]:
            require(source_id in sources, f"{context}: missing source {source_id}")

    for cid, c in concepts.items():
        for pre in c["prerequisites"]:
            require(pre in concepts and pre != cid, f"concept {cid}: invalid prerequisite {pre}")
            require(concepts[pre]["domain"] == c["domain"],
                    f"concept {cid}: prerequisite outside baseline domain")
    # No prerequisite cycles; otherwise the domain cannot be learned progressively.
    visiting, visited = set(), set()

    def visit(cid):
        require(cid not in visiting, f"concept prerequisite cycle at {cid}")
        if cid in visited:
            return
        visiting.add(cid)
        for pre in concepts[cid]["prerequisites"]:
            visit(pre)
        visiting.remove(cid)
        visited.add(cid)

    for cid in concepts:
        visit(cid)

    for tid, t in tests.items():
        require(present_string(t, "domain", f"test {tid}") in domains,
                f"test {tid}: unknown domain")
        cid = present_string(t, "concept_id", f"test {tid}")
        require(cid in concepts and concepts[cid]["domain"] == t["domain"],
                f"test {tid}: mismatched concept")
        present_string(t, "task", f"test {tid}")
        require(t.get("answer_access") == "withheld_not_in_fixture",
                f"test {tid}: no held-out answer may enter this fixture")
    for did, d in domains.items():
        for tid in d["competence_test_ids"]:
            require(tid in tests and tests[tid]["domain"] == did,
                    f"domain {did}: invalid competence test {tid}")

    for mid, m in mappings.items():
        context = f"mapping {mid}"
        a = present_string(m, "from", context)
        b = present_string(m, "to", context)
        require(a in concepts and b in concepts and a != b,
                f"{context}: invalid endpoints")
        require(concepts[a]["domain"] != concepts[b]["domain"],
                f"{context}: cross-domain relation required")
        require(m.get("kind") in ("structural_candidate", "lexical_only"),
                f"{context}: unknown mapping type")
        roles = array(m, "shared_roles", context)
        require(len(roles) == len(set(roles)), f"{context}: duplicate roles")
        shared = set(concepts[a]["roles"]) & set(concepts[b]["roles"])
        require(set(roles).issubset(shared),
                f"{context}: claimed roles are not shared by both concepts")
        boundaries = array(m, "boundary_conditions", context, 1)
        require(all(isinstance(x, str) and x.strip() for x in boundaries),
                f"{context}: nonempty boundaries required")
        present_string(m, "question", context)
        for k in ("information_gain", "transfer_value", "feasibility", "cost", "risk"):
            fraction(m, k, context)
        if m["kind"] == "structural_candidate":
            require(len(roles) >= 2, f"{context}: insufficient structural roles")
            present_string(m, "prediction", context)
            present_string(m, "falsifier", context)
        else:
            require(not roles, f"{context}: lexical match must not claim mechanisms")
            require(not m.get("prediction") and not m.get("falsifier"),
                    f"{context}: lexical-only match cannot assert testable transfer")

    for gid, gap in gaps.items():
        context = f"gap {gid}"
        require(present_string(gap, "concept_id", context) in concepts,
                f"{context}: unknown concept")
        present_string(gap, "question", context)
        present_string(gap, "method", context)
        for k in ("information_gain", "transfer_value", "feasibility", "cost", "risk"):
            fraction(gap, k, context)

    return sources, domains, concepts, mappings, gaps, tests


def priority(record):
    """Transparent predeclared heuristic, NOT an empirically calibrated reward."""
    score = (.35 * record["information_gain"] +
             .25 * record["transfer_value"] +
             .25 * record["feasibility"] -
             .15 * record["cost"] -
             .15 * record["risk"])
    return round(max(0.0, min(1.0, score)), 6)


def triage(pack):
    sources, domains, concepts, mappings, gaps, tests = validate_package(pack)
    agenda = []
    blocked = []
    for mid, m in mappings.items():
        a, b = concepts[m["from"]], concepts[m["to"]]
        families = sorted({
            sources[sid]["family_id"]
            for source_id in (a["source_ids"] + b["source_ids"])
            for sid in [source_id]
        })
        if m["kind"] == "lexical_only":
            blocked.append({"id": mid, "reason": "LEXICAL_MATCH_NOT_STRUCTURAL",
                            "status": "rejected"})
            continue
        if len(families) < 2:
            blocked.append({"id": mid, "reason": "SOURCE_FAMILIES_NOT_INDEPENDENT",
                            "status": "deferred"})
            continue
        agenda.append({
            "id": mid,
            "type": "cross_domain_analogy",
            "status": "candidate_unverified",
            "priority": priority(m),
            "question": m["question"],
            "hypothesis_prediction": m["prediction"],
            "falsifier": m["falsifier"],
            "shared_mechanisms": m["shared_roles"],
            "boundary_conditions": m["boundary_conditions"],
            "distinct_source_families": len(families),
            "competence_precondition": "not_assessed",
            "external_experiment_permission": False
        })

    for gid, gap in gaps.items():
        agenda.append({
            "id": gid,
            "type": "knowledge_gap",
            "status": "needs_research",
            "priority": priority(gap),
            "question": gap["question"],
            "proposed_method": gap["method"],
            "competence_precondition": "not_assessed",
            "external_experiment_permission": False
        })
    agenda.sort(key=lambda v: (-v["priority"], v["id"]))
    blocked.sort(key=lambda v: v["id"])
    return {
        "schema_version": pack["schema_version"],
        "input_classification": pack["dataset_kind"],
        "epistemic_status": "no_new_verified_facts",
        "domain_competence": {d: "not_assessed" for d in sorted(domains)},
        "counts": {"sources": len(sources), "concepts": len(concepts),
                   "domains": len(domains), "candidate_questions": len(agenda),
                   "blocked_mappings": len(blocked)},
        "agenda": agenda,
        "blocked": blocked,
        "warnings": [
            "Illustrative hand-authored mappings; no model discovered new concepts.",
            "Source family IDs are assertions, not authenticated source independence.",
            "No research actions or external experiments executed.",
            "Priority weights are heuristic and not externally calibrated."
        ]
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    path = Path(args.input)
    try:
        pack = json.loads(path.read_text(encoding="utf-8"))
        result = triage(pack)
    except (OSError, json.JSONDecodeError, ContractError) as err:
        parser.exit(status=2, message=f"foundation_validation_failed: {err}\n")
    result["source_sha256"] = hashlib.sha256(path.read_bytes()).hexdigest()
    Path(args.output).parent.mkdir(parents=True, exist_ok=True)
    Path(args.output).write_text(json.dumps(result, indent=2, sort_keys=True) + "\n",
                                 encoding="utf-8")
    print(f"foundation_validation=PASS triaged={len(result['agenda'])} "
          f"blocked={len(result['blocked'])} verified_facts=0")


if __name__ == "__main__":
    main()
