#!/usr/bin/env python3
"""Generate deterministic, open procedural application questions WITHOUT gold.

These are not independently blind problems and do not establish comprehension.
"""
import argparse
import hashlib
import json
import random
from pathlib import Path

EXPERIMENT = "MOOL-POLYMATH-F3-001"
SEED = 20261018
CARD_BY_DOMAIN = {
    "networks": "card-network-queue-v1",
    "biology": "card-biology-homeostat-v1",
    "supply_chain": "card-supply-replenishment-v1",
}
TARGETS = {
    "networks": ("next_queue", "drops"),
    "biology": ("control", "next_state"),
    "supply_chain": ("reorder_point", "order_qty"),
}
DOMAINS = tuple(CARD_BY_DOMAIN)


def canonical_json(value):
    return json.dumps(value, sort_keys=True, separators=(",", ":"),
                      ensure_ascii=False).encode()


def sha(value):
    return hashlib.sha256(canonical_json(value)).hexdigest()


def cards_from(path):
    data = json.loads(Path(path).read_text(encoding="utf-8"))
    if (data.get("schema_version") != "polymath-domain-cards-v3" or
            data.get("content_status") != "project_authored_instructional"):
        raise ValueError("cards must be explicitly authored pedagogical material")
    if data.get("publisher_rights") != "license_review_required_before_external_republication":
        raise ValueError("unreviewed rights cannot be declared cleared")
    cards = data.get("cards")
    if not isinstance(cards, list) or len(cards) != 3:
        raise ValueError("three cards required")
    for card in cards:
        domain = card.get("domain")
        if (domain not in TARGETS or card.get("id") != CARD_BY_DOMAIN[domain] or
                set(card.get("outputs", [])) != set(TARGETS[domain]) or
                not card.get("assumptions") or not card.get("must_abstain_when") or
                not card.get("description") or not card.get("worked_example") or
                card.get("source_authority", "").startswith("verified")):
            raise ValueError("card incomplete or falsely authoritative")
    if len(set(c["id"] for c in cards)) != 3:
        raise ValueError("duplicate domain card")
    return data


def _valid_case(domain, rng):
    if domain == "networks":
        buffer = rng.randint(3, 45)
        return dict(queue=rng.randint(0, buffer),
                    arrivals=rng.randint(0, 60),
                    service=rng.randint(0, 60), buffer=buffer)
    if domain == "biology":
        half = lambda lo, hi: rng.randint(lo, hi) / 2
        return dict(observed=half(-30, 60), setpoint=half(-30, 60),
                    gain=half(0, 8), limit=half(1, 20),
                    disturbance=half(-16, 16))
    if domain == "supply_chain":
        return dict(daily_demand=rng.randint(0, 25),
                    lead_days=rng.randint(0, 25), safety=rng.randint(0, 55),
                    on_hand=rng.randint(0, 240), on_order=rng.randint(0, 90),
                    backorders=rng.randint(0, 75), target=rng.randint(0, 250))
    raise ValueError("unknown task domain")


def _invalid_case(domain, rng, j):
    params = _valid_case(domain, rng)
    if domain == "networks":
        if j % 2:
            params["queue"] = params["buffer"] + 1
        else:
            params["arrivals"] = -rng.randint(1, 5)
    elif domain == "biology":
        if j % 2:
            params["limit"] = 0
        else:
            params["gain"] = -0.5
    else:
        if j % 2:
            params.pop("lead_days")
        else:
            params["daily_demand"] = -1
    return params


def generate(cards, seed=SEED):
    if type(seed) is not int or seed < 0 or seed > 2**31:
        raise ValueError("seed must be fixed nonnegative integer")
    if cards.get("schema_version") != "polymath-domain-cards-v3":
        raise ValueError("unreviewed card schema")
    tasks = []
    for ix, domain in enumerate(DOMAINS):
        rng = random.Random(seed + 1009 * (ix + 1))
        targets = TARGETS[domain]
        cases = [(_valid_case(domain, rng), targets[j % 2]) for j in range(20)]
        cases.extend((_invalid_case(domain, rng, j), targets[j % 2])
                     for j in range(4))
        rng.shuffle(cases)
        for index, (params, target) in enumerate(cases):
            tasks.append({
                "task_id": f"f3-{ix+1}-{index+1:02d}",
                "domain": domain,
                "card_id": CARD_BY_DOMAIN[domain],
                "task_type": "apply_declared_model",
                "target": target,
                "inputs": params,
                "prompt": "Apply the supplied learning card. If inputs violate assumptions, abstain and explain why. Cite card ID. Return target, not a speculative real-world prediction."
            })
    packet = {
        "experiment": EXPERIMENT,
        "status": "open_procedural_challenge_not_independent_blind",
        "generator_seed": seed,
        "card_content_sha256": sha(cards),
        "question_count": len(tasks),
        "tasks": tasks
    }
    validate_packet(packet, cards)
    return packet


def validate_packet(packet, cards):
    if packet.get("experiment") != EXPERIMENT or packet.get("status") != "open_procedural_challenge_not_independent_blind":
        raise ValueError("experiment identity mismatch")
    if packet.get("card_content_sha256") != sha(cards):
        raise ValueError("card input hash mismatch")
    tasks = packet.get("tasks")
    if not isinstance(tasks, list) or len(tasks) != 72 or packet.get("question_count") != 72:
        raise ValueError("72 challenge cases required")
    ids = set()
    counts = {domain: 0 for domain in DOMAINS}
    for task in tasks:
        if set(task) != {"task_id", "domain", "card_id", "task_type",
                         "target", "inputs", "prompt"}:
            raise ValueError("unapproved key in challenge or answer leakage")
        tid = task["task_id"]
        if tid in ids:
            raise ValueError("repeated task id")
        ids.add(tid)
        domain = task["domain"]
        if domain not in DOMAINS or task["card_id"] != CARD_BY_DOMAIN[domain] or task["target"] not in TARGETS[domain]:
            raise ValueError("invalid domain/card/target identity")
        if task["task_type"] != "apply_declared_model" or not isinstance(task["inputs"], dict) or not task["prompt"]:
            raise ValueError("invalid challenge descriptor")
        counts[domain] += 1
    if any(n != 24 for n in counts.values()):
        raise ValueError("domain challenge count mismatch")
    banned = {"gold", "correct", "expected", "solution", "answer",
              "oracle", "label", "rationale_gold"}

    def recurse(value):
        if isinstance(value, dict):
            for key, child in value.items():
                if key.lower() in banned:
                    raise ValueError("gold answer leaked into challenge")
                recurse(child)
        elif isinstance(value, list):
            for child in value:
                recurse(child)
    recurse(packet)
    return True


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--cards", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--seed", type=int, default=SEED)
    options = p.parse_args()
    cards = cards_from(options.cards)
    packet = generate(cards, options.seed)
    path = Path(options.out)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(packet, sort_keys=True, indent=2) + "\n",
                    encoding="utf-8")
    print(f"f3_challenge_generation=PASS tasks={len(packet['tasks'])} packet_sha256={sha(packet)}")


if __name__ == "__main__":
    main()
