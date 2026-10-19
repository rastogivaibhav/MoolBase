#!/usr/bin/env python3
"""F8 project-authored natural-language trial corpus, not independently blind."""
import argparse, hashlib, json, random, sys
from pathlib import Path
HERE=Path(__file__).resolve().parent
sys.path[:0]=[str(HERE.parent/"v3"),str(HERE.parent/"v6")]
from challenge import cards_from,generate,sha
from scorer import oracle
from export_typed import mechanisms,spec,MANIFEST_FIELDS,write_tsv
SEED=20261021
def prose(task):
    d=task["domain"]; p=task["inputs"]; dest=task["target"]
    if d=="networks":
        out="how many packets remain waiting" if dest=="next_queue" else "how many packets get discarded"
        return ("In a small packet queue, initially "+str(p["queue"])+" packets wait. "
                +str(p["arrivals"])+" packets arrive during the interval; the device serves "
                +str(p["service"])+"; its buffer capacity is "+str(p["buffer"])
                +". "+out.capitalize()+" after this interval? These are integer packet counts.")
    if d=="biology":
        out="what corrective actuator output is applied" if dest=="control" else "what state reading follows the correction"
        return ("A simplified proportional controller senses "+str(p["observed"])+" state units "
                "against a desired "+str(p["setpoint"])+". Its dimensionless sensitivity is "
                +str(p["gain"])+", actuator magnitude ceiling "+str(p["limit"])+
                " state units and outside disturbance "+str(p["disturbance"])+
                " state units. "+out.capitalize()+"? Treat this as a single-step numerical toy.")
    out="replenishment trigger threshold" if dest=="reorder_point" else "number of new units to order"
    lead=(str(p["lead_days"])+" days" if "lead_days" in p else "not supplied")
    return ("For a small inventory policy, average usage is "+str(p["daily_demand"])+" items per day, "
            "supplier lead time is "+lead+", and reserve is "+str(p["safety"])+
            " items. There are "+str(p["on_hand"])+" items physically held, "
            +str(p["on_order"])+" inbound, "+str(p["backorders"])+" outstanding unmet, "
            "and the desired stock target is "+str(p["target"])+
            " items. Find the "+out+" under the stated policy.")

def normalized(task,defs):
    m=defs[task["domain"]]
    output={f[0]:(f[1],f[2]) for f in m["output_fields"]}
    parts=[]
    for name,kind,unit in m["input_fields"]:
        if name in task["inputs"]:
            parts.append(f"{name}={task['inputs'][name]}@{unit}#{kind}")
    return dict(description="Natural-language mechanism selection",
                target=task["target"],target_unit=output[task["target"]][1],
                inputs=";".join(parts))

def generate_pilot(cards,defs):
    f3=generate(cards,SEED)
    samples=[]
    for domain in ("networks","biology","supply_chain"):
        original=[t for t in f3["tasks"] if t["domain"]==domain]
        good=[t for t in original if oracle(t) is not None][:4]
        bad=[t for t in original if oracle(t) is None][:2]
        if len(good)!=4 or len(bad)!=2:raise ValueError("unexpected v3 distribution")
        for q in good+bad:
            samples.append((prose(q),"in_library_valid" if q in good else "assumption_violation",q))
    outside=[
        "A blue laser with wavelength 450 nanometres enters a prism of refractive index 1.52. Calculate its refracted angle.",
        "A satellite orbits a central mass; given gravitational parameter 6.5 and period 20, calculate the orbital radius.",
        "A metal tube resonates when a sound wave travels 343 metres per second. Calculate its fundamental frequency."
    ]
    samples += [(s,"unknown_mechanism",None) for s in outside]
    pairings=[("networks","biology"),("biology","supply_chain"),("networks","supply_chain")]
    for d1,d2 in pairings:
        a=next(t for t in f3["tasks"] if t["domain"]==d1 and oracle(t) is not None)
        b=next(t for t in f3["tasks"] if t["domain"]==d2 and oracle(t) is not None)
        samples.append(("These are TWO independent situations and no single model should answer them jointly. "
                        +prose(a)+" Also "+prose(b)+
                        " Provide one joint mechanism result covering both.", "composite",None))
    if len(samples)!=24:raise ValueError("must have 24 frozen trials")
    random.Random(SEED+109).shuffle(samples)
    queries=[];gold=[]
    for i,(description,category,q) in enumerate(samples):
        tid=f"f8-trial-{i+1:03d}"
        queries.append({"opaque_id":tid,"question":description})
        gold.append({"opaque_id":tid,"category":category,
                     "domain":q["domain"] if q else None,
                     "mechanism_id":defs[q["domain"]]["mechanism_id"] if q else None,
                     "expected_normalized":normalized(q,defs) if q else None,
                     "source_task":q})
    return queries,gold,sha(f3)

def main():
    ap=argparse.ArgumentParser()
    for a in ("cards","mechanisms","queries","gold","manifest","receipt"):
        ap.add_argument("--"+a,required=True)
    a=ap.parse_args()
    cards=cards_from(a.cards);defs=mechanisms(a.mechanisms,cards)
    queries,gold,source=generate_pilot(cards,defs)
    manifest=[]
    for d,m in sorted(defs.items()):
        manifest.append({k:v for k,v in (
         ("mechanism_id",m["mechanism_id"]),("version",m["version"]),("domain",d),
         ("card_id",m["card_id"]),("formula_id",m["formula_id"]),
         ("source_family",m["source_family"]),("source_status",m["source_status"]),
         ("license_status",m["license_status"]),("description",m["description"]),
         ("input_spec",spec(m["input_fields"])),("output_spec",spec(m["output_fields"])),
         ("card_sha256",sha(cards)))})
    write_tsv(a.manifest,MANIFEST_FIELDS,manifest)
    for k,data in (("queries",queries),("gold",gold)):
        Path(getattr(a,k)).write_text(json.dumps(data,indent=2,sort_keys=True)+"\n")
    receipt={"experiment":"MOOL-POLYMATH-F8-001","public_corpus_not_independently_blind":True,
             "seed":SEED,"n":24,"source_f3_sha256":source,
             **{k+"_sha256":hashlib.sha256(Path(getattr(a,k)).read_bytes()).hexdigest()
                for k in ("queries","gold","manifest")}}
    Path(a.receipt).write_text(json.dumps(receipt,indent=2,sort_keys=True)+"\n")
    print("f8_generated=PASS 24 questions; mechanism IDs and numerical truth withheld from model prompts")
if __name__=="__main__":main()
