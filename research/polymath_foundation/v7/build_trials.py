#!/usr/bin/env python3
"""Generate F7 mechanism-identity-free 108-task selector corpus (not blind).

Gold is emitted into a SEPARATE file for scoring; never supplied to selector.
"""
import argparse
import csv
import hashlib
import json
import random
import sys
from pathlib import Path

HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE.parent/"v3"))
sys.path.insert(0,str(HERE.parent/"v6"))
from challenge import cards_from,generate,sha
from export_typed import mechanisms,spec,MANIFEST_FIELDS,write_tsv

SEED=20261020
QUERY_FIELDS=("opaque_id","description","target","target_unit","inputs")
ROUTE_FIELDS=("opaque_id","selection_status","mechanism_id","domain",
              "reason","source_family","native_node_id")

NARRATIVE={
    "networks":"For an interval, determine a finite-buffer queue result. Inventory stock-flow analogies may be misleading.",
    "biology":"Apply a bounded control response to an observed state and disturbance. Network congestion analogies may be misleading.",
    "supply_chain":"Assess a replenishment threshold or purchase quantity, accounting for supplier lead time. Network congestion analogies may be misleading."
}
OOD=[
    ("optics","Compute optical refractive gain for a wavelength.","refraction",{"wavelength":(500,"nm","float"),"refractive_index":(1.3,"dimensionless","float")}),
    ("astronomy","Predict the astronomical orbital radius from gravitational parameters.","radius",{"orbital_mass":(100,"kg","float"),"gravitational_parameter":(7,"m3_s2","float")}),
    ("acoustics","Evaluate sound-wave frequency shift in a resonant tube.","frequency",{"sound_speed":(340,"m_s","float"),"tube_length":(2,"meters","float")})
]

def records(cards,bydomain):
    manifest=[]
    for d,m in sorted(bydomain.items()):
        manifest.append({
            "mechanism_id":m["mechanism_id"],"version":m["version"],
            "domain":d,"card_id":m["card_id"],"formula_id":m["formula_id"],
            "source_family":m["source_family"],
            "source_status":m["source_status"],"license_status":m["license_status"],
            "description":m["description"],
            "input_spec":spec(m["input_fields"]),
            "output_spec":spec(m["output_fields"]),
            "card_sha256":sha(cards)
        })
    return manifest

def encoded(params,fields):
    arr=[]
    for name,typ,unit in fields:
        if name in params:
            value=params[name]
            if type(value) not in (int,float):
                raise ValueError("wrong generated value")
            arr.append(f"{name}={value}@{unit}#{typ}")
    return ";".join(arr)

def build(cards,bydomain):
    original=generate(cards,SEED)
    tasks=original["tasks"]
    if len(tasks)!=72:
        raise ValueError("fixed v3 generator changed")
    pool=[]
    def add(category,description,target,unit,inputs,source_task,truth_domain):
        pool.append({"category":category,"description":description,
                     "target":target,"target_unit":unit,"inputs":inputs,
                     "source_task":source_task,
                     "truth_domain":truth_domain})
    for q in tasks:
        m=bydomain[q["domain"]]
        out={f[0]:f for f in m["output_fields"]}
        add("library",NARRATIVE[q["domain"]],q["target"],
            out[q["target"]][2],encoded(q["inputs"],m["input_fields"]),
            q,q["domain"])

    # Four valid cases per domain, then poison exactly one field unit each.
    for d in ("networks","biology","supply_chain"):
        candidates=[t for t in tasks if t["domain"]==d and
                    all(f[0] in t["inputs"] for f in bydomain[d]["input_fields"])]
        for task in candidates[:4]:
            m=bydomain[d]
            entries=encoded(task["inputs"],m["input_fields"]).split(";")
            entries[0]=entries[0].replace("@"+m["input_fields"][0][2]+"#","@credits#")
            target={f[0]:f[2] for f in m["output_fields"]}[task["target"]]
            add("poison_unit",NARRATIVE[d],task["target"],target,
                ";".join(entries),None,None)

    for i in range(12):
        d,prompt,target,params=OOD[i%3]
        x=";".join(f"{name}={v}@{u}#{ty}" for name,(v,u,ty) in params.items())
        add("out_of_domain",prompt,target,"unknown_unit",x,None,None)

    pairs=(("networks","biology"),("biology","supply_chain"),
           ("networks","supply_chain"))
    for first,second in pairs:
        aa=[t for t in tasks if t["domain"]==first and
            all(k in t["inputs"] for k,_,_ in bydomain[first]["input_fields"])]
        bb=[t for t in tasks if t["domain"]==second and
            all(k in t["inputs"] for k,_,_ in bydomain[second]["input_fields"])]
        for j in range(4):
            target=aa[j]["target"]
            unit={f[0]:f[2] for f in bydomain[first]["output_fields"]}[target]
            a=encoded(aa[j]["inputs"],bydomain[first]["input_fields"])
            b=encoded(bb[j]["inputs"],bydomain[second]["input_fields"])
            add("cross_domain_composite",
                "A deliberately blended case describes control, queues and planning; reject if one procedure cannot consume ALL supplied measurements.",
                target,unit,a+";"+b,None,None)

    if len(pool)!=108:
        raise ValueError(f"expected 108 questions got {len(pool)}")
    rng=random.Random(SEED+8181)
    rng.shuffle(pool)
    queries=[]
    gold=[]
    for i,item in enumerate(pool):
        opaque=f"trial-{i+1:04d}"
        queries.append({k:item[k] for k in QUERY_FIELDS if k in item} |
                       {"opaque_id":opaque})
        gold.append({"opaque_id":opaque,
                     "category":item["category"],
                     "source_task":item["source_task"],
                     "expected_domain":item["truth_domain"]})
    assert sum(x["category"]=="library" for x in gold)==72
    for category in ("poison_unit","out_of_domain","cross_domain_composite"):
        assert sum(x["category"]==category for x in gold)==12
    assert set(q["opaque_id"] for q in queries)==set(g["opaque_id"] for g in gold)
    return original,queries,gold

def main():
    p=argparse.ArgumentParser()
    for key in ("cards","mechanisms","manifest","queries","gold","receipt"):
        p.add_argument("--"+key,required=True)
    a=p.parse_args()
    cards=cards_from(a.cards)
    bydomain=mechanisms(a.mechanisms,cards)
    orig,queries,gold=build(cards,bydomain)
    write_tsv(a.manifest,MANIFEST_FIELDS,records(cards,bydomain))
    write_tsv(a.queries,QUERY_FIELDS,queries)
    Path(a.gold).write_text(json.dumps({
        "experiment":"MOOL-POLYMATH-F7-001",
        "gold_status":"evaluation_only_not_for_selection",
        "generator_seed":SEED,
        "source_v3_challenge_hash":sha(orig),
        "trials":gold
    },indent=2,sort_keys=True)+"\n")
    hashes={}
    for key in ("manifest","queries","gold"):
        hashes[key+"_sha256"]=hashlib.sha256(Path(getattr(a,key)).read_bytes()).hexdigest()
    receipt={"experiment":"MOOL-POLYMATH-F7-001","tasks":len(queries),
             "reference_classification":"nonindependent_open_generator",
             "gold_separated":True,**hashes}
    Path(a.receipt).write_text(json.dumps(receipt,indent=2,sort_keys=True)+"\n")
    print(f"f7_generation=PASS total={len(queries)} labeled_inputs_exposed=0 "
          "independent_blind=NO")

if __name__=="__main__":
    main()
