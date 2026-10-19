#!/usr/bin/env python3
"""Foundation v8 authored alias-case generator. NOT independently blind."""
import argparse
import csv
import hashlib
import json
import random
import sys
from pathlib import Path

HERE=Path(__file__).resolve().parent
sys.path[:0]=[str(HERE.parent/"v3"),str(HERE.parent/"v6"),str(HERE.parent/"v7")]
from challenge import cards_from,generate,sha
from export_typed import mechanisms,write_tsv
from build_trials import QUERY_FIELDS, NARRATIVE

SEED=20261021
ALIAS_FIELDS=("alias","canonical","domain","field_kind","canonical_type",
              "canonical_unit","alias_unit","mul_num","mul_den",
              "source_status","source_family","relation_kind")

def load_aliases(path,bydomain):
    doc=json.loads(Path(path).read_text(encoding="utf-8"))
    if (doc.get("schema_version")!="moolbase.polymath.lexical_alias.v1" or
        doc.get("source_classification")!="project_authored_unverified_not_empirical_synonym_proof"
        or doc.get("truth_claim") is not False):
        raise ValueError("untrusted alias schema")
    result={}
    for a in doc["entries"]:
        alias=a["alias"]
        if alias in result or not alias.isidentifier() or alias.startswith("__"):
            raise ValueError("duplicate/unsafe alias identity")
        domain=a["domain"]
        if domain not in bydomain:
            raise ValueError("alias unknown domain")
        m=bydomain[domain]
        field=("input_fields" if a["field_kind"]=="input" else "output_fields")
        original={v[0]:(v[1],v[2]) for v in m[field]}
        if a["canonical"] not in original or original[a["canonical"]] != (a["canonical_type"],a["canonical_unit"]):
            raise ValueError("alias type/units mismatch reviewed procedure")
        if a["source_status"]!="unverified" or a["source_family"]!=m["source_family"] or a["relation_kind"]!="lexical_alias_not_scientific_equivalence":
            raise ValueError("source alias promoted into unverified scientific truth")
        if type(a["mul_num"])!=int or type(a["mul_den"])!=int or a["mul_num"]<1 or a["mul_den"]<1:
            raise ValueError("non-positive rational conversion")
        # Exactly these six unit conversions were preregistered.
        conversions={
            ("biology",k):("centistate",1,100) for k in
            ("observed","setpoint","limit","disturbance")}
        conversions.update({("supply_chain","daily_demand"):("items_per_week",1,7),
                            ("supply_chain","lead_days"):("hours",1,24)})
        actual=(a["alias_unit"],a["mul_num"],a["mul_den"])
        allowed={(a["canonical_unit"],1,1)}
        if a["field_kind"]=="input" and (domain,a["canonical"]) in conversions:
            allowed.add(conversions[(domain,a["canonical"])])
        if actual not in allowed:
            raise ValueError("unreviewed unit conversion")
        result[alias]=a
    expected=sum(2*(len(m["input_fields"])+len(m["output_fields"])) for m in bydomain.values())
    if len(result)!=expected:
        raise ValueError("missing alias definitions")
    return result

def choose(candidates,key):
    choices=sorted(candidates,key=lambda a:a["alias"])
    h=int(hashlib.sha256((str(SEED)+":"+key).encode()).hexdigest()[:8],16)
    return choices[h%len(choices)]

def encode(q,bydomain,aliases):
    domain=q["domain"]
    m=bydomain[domain]
    records=list(aliases.values())
    fragments=[]
    for k,typ,unit in m["input_fields"]:
        if k not in q["inputs"]:
            continue
        alias=choose([a for a in records if a["domain"]==domain and
                      a["canonical"]==k and a["field_kind"]=="input"],
                     q["task_id"]+":"+k)
        canonical=q["inputs"][k]
        num=canonical*alias["mul_den"]/alias["mul_num"]
        # Input units can be author-curated; original v6 canonical type
        # continues to be checked after normalization.
        value=str(int(num)) if float(num).is_integer() else str(num)
        fragments.append(f"{alias['alias']}={value}@{alias['alias_unit']}#{typ}")
    target=choose([a for a in records if a["domain"]==domain and
                   a["canonical"]==q["target"] and a["field_kind"]=="output"],
                  q["task_id"]+":target")
    return {
        "description":NARRATIVE[domain].replace("finite-buffer queue","processing backlog"),
        "target":target["alias"],
        "target_unit":target["alias_unit"],
        "inputs":";".join(fragments)
    }

def construct(cards,bydomain,aliases):
    prior=generate(cards,SEED)
    tasks=prior["tasks"]
    pool=[]
    def push(item,category,original=None):
        pool.append({"input":item,"gold":{"category":category,
                                          "original":original}})
    for q in tasks:
        push(encode(q,bydomain,aliases),"library",q)
    for d in ("networks","biology","supply_chain"):
        original=[q for q in tasks if q["domain"]==d and all(
            k in q["inputs"] for k,_,_ in bydomain[d]["input_fields"])]
        for q in original[:4]:
            case=encode(q,bydomain,aliases)
            first=case["inputs"].split(";")
            key,val=first[0].split("@",1)
            first[0]=key+"@unregistered_unit#"+val.split("#",1)[1]
            case["inputs"]=";".join(first)
            push(case,"unsupported_unit")
        # Duplicate concept through a second different alias
        for q in original[4:8]:
            case=encode(q,bydomain,aliases)
            first=case["inputs"].split(";")[0]
            alias_name=first.split("=",1)[0]
            entry=aliases[alias_name]
            other=next(a for a in aliases.values()
                       if a["domain"]==d and a["canonical"]==entry["canonical"]
                       and a["field_kind"]=="input" and a["alias"]!=alias_name)
            original_val=q["inputs"][entry["canonical"]]
            converted=original_val*other["mul_den"]/other["mul_num"]
            val=str(int(converted)) if float(converted).is_integer() else str(converted)
            case["inputs"]+=";"+f"{other['alias']}={val}@{other['alias_unit']}#{other['canonical_type']}"
            push(case,"conflicting_alias")
    # Unknown source realms, three distinct out-of-ontology vocabularies.
    ood=[
        ("quantum optics","refraction_index","wavelength_nm=520@nanometers#float;refractive_metric=1.4@dimensionless#float"),
        ("orbital astronomy","orbital_radius","mass_parameter=80@kg#float;gravitational_field=2@meters#float"),
        ("acoustics","resonance_frequency","tube_span=4@meters#float;sound_velocity=343@meters_per_second#float")
    ]
    for i in range(12):
        kind,target,inp=ood[i%3]
        push({"description":"Estimate "+kind+" with its domain equations.",
              "target":target,"target_unit":"unknown","inputs":inp},
             "out_of_domain")
    pairs=[("networks","biology"),("biology","supply_chain"),
           ("networks","supply_chain")]
    for da,db in pairs:
        aa=[q for q in tasks if q["domain"]==da and all(k in q["inputs"]
             for k,_,_ in bydomain[da]["input_fields"])]
        bb=[q for q in tasks if q["domain"]==db and all(k in q["inputs"]
             for k,_,_ in bydomain[db]["input_fields"])]
        for j in range(4):
            left=encode(aa[j],bydomain,aliases)
            right=encode(bb[j],bydomain,aliases)
            left["inputs"]+=";"+right["inputs"]
            left["description"]="Intermixed measurements from two distinct mechanisms; no permitted partial answer."
            push(left,"composite")
    if len(pool)!=120:
        raise AssertionError(f"unexpected size {len(pool)}")
    random.Random(SEED+89).shuffle(pool)
    queries,gold=[],[]
    for i,entry in enumerate(pool):
        opaque=f"sem-{i+1:04d}"
        q=dict(entry["input"])
        q["opaque_id"]=opaque
        queries.append(q)
        gold.append({"opaque_id":opaque,**entry["gold"]})
    if set(x["category"] for x in gold)!={"library","unsupported_unit","conflicting_alias","out_of_domain","composite"}:
        raise ValueError("missing negative group")
    for kind,expected in (("library",72),("unsupported_unit",12),
                          ("conflicting_alias",12),("out_of_domain",12),("composite",12)):
        assert sum(x["category"]==kind for x in gold)==expected
    return queries,gold

def main():
    p=argparse.ArgumentParser()
    for x in ("cards","mechanisms","aliases","alias_manifest","queries","gold","hash_receipt"):
        p.add_argument("--"+x,required=True)
    args=p.parse_args()
    cards=cards_from(args.cards)
    bydomain=mechanisms(args.mechanisms,cards)
    aliases=load_aliases(args.aliases,bydomain)
    queries,gold=construct(cards,bydomain,aliases)
    write_tsv(args.alias_manifest,ALIAS_FIELDS,[
        {k:str(x[k]) for k in ALIAS_FIELDS} for x in sorted(aliases.values(),
                                                          key=lambda a:a["alias"])])
    write_tsv(args.queries,("opaque_id","description","target","target_unit","inputs"),queries)
    Path(args.gold).write_text(json.dumps({"experiment":"MOOL-POLYMATH-F8-001",
        "status":"INTERNAL_GENERATOR_NOT_EXPERT_BLIND","trials":gold},indent=2,sort_keys=True)+"\n")
    hash_info={key+"_sha256":hashlib.sha256(Path(getattr(args,key)).read_bytes()).hexdigest()
               for key in ("aliases","alias_manifest","queries","gold")}
    Path(args.hash_receipt).write_text(json.dumps({"experiment":"MOOL-POLYMATH-F8-001",
        "trial_count":120,"internal_not_blind":True,**hash_info},indent=2,sort_keys=True)+"\n")
    print("f8_generated=PASS total=120 valid_calculations=60 expected_refusals=60 independent_blind=NO")

if __name__=="__main__":
    main()
