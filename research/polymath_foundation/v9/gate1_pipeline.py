#!/usr/bin/env python3
"""Run v9 Gate1 end-to-end, preserving the unchanged v8 scorer and native tools."""
import csv,json,os,subprocess,sys
from pathlib import Path
HERE=Path(__file__).resolve().parent
ROOT=Path("reports/polymath_foundation_v9/gate1")
ROOT.mkdir(parents=True,exist_ok=True)
V8="research/polymath_foundation/v8"
CARDS="research/polymath_foundation/v3/cards.json"
MECHANISMS="research/polymath_foundation/v6/mechanisms.json"
MODEL="/tmp/f9-model/qwen.gguf"
def run(*cmd):
    print("Gate1 running: "+ " ".join(str(x) for x in cmd),flush=True)
    subprocess.run([str(x) for x in cmd],check=True,timeout=3000)
def f(name):return str(ROOT/name)
run(sys.executable,f"{V8}/generate_pilot.py",
    "--cards",CARDS,"--mechanisms",MECHANISMS,
    "--queries",f("model_questions.json"),
    "--gold",f("evaluator_only_gold.json"),
    "--manifest",f("mechanisms.tsv"),
    "--receipt",f("corpus_hashes.json"))
run(sys.executable,str(HERE/"gate1_strong_model.py"),
    "--queries",f("model_questions.json"),"--manifest",f("mechanisms.tsv"),
    "--model-file",MODEL,"--output",f("predictions_B2.json"))
run(sys.executable,f"{V8}/prepare_routes.py",
    "--mode","queries","--predictions",f("predictions_B2.json"),
    "--manifest",f("mechanisms.tsv"),"--queries",f("normalized_B2.tsv"))
run(sys.executable,f"{V8}/prepare_routes.py",
    "--mode","baseline","--predictions",f("predictions_B2.json"),
    "--manifest",f("mechanisms.tsv"),"--queries",f("normalized_B2.tsv"),
    "--routes",f("routes_B2.tsv"))
run("./build-v9/moolbase_polymath_v8_selector",
    f("mechanisms.tsv"),f("normalized_B2.tsv"),f("routes_B3.tsv"))
def rows(file):
    with open(file,newline="") as r:return list(csv.DictReader(r,delimiter="\t"))
b2=rows(f("routes_B2.tsv"));b3=rows(f("routes_B3.tsv"))
if len(b2)!=24 or len(b3)!=24:raise ValueError("incomplete native routing")
for a,b in zip(b2,b3):
    for key in ("opaque_id","selection_status","mechanism_id","domain","reason","source_family"):
        if a[key]!=b[key]:raise ValueError("native vs schema selector divergence")
    if b["selection_status"]=="selected" and b["native_node_id"]=="-":
        raise ValueError("native selector did not return real node ID")
for arm in ("B2","B3"):
    run(sys.executable,f"{V8}/prepare_routes.py",
        "--mode","selected","--predictions",f("predictions_B2.json"),
        "--manifest",f("mechanisms.tsv"),"--queries",f("normalized_B2.tsv"),
        "--routes",f("routes_"+arm+".tsv"),
        "--selected",f("selected_"+arm+".tsv"))
    if len(rows(f("selected_"+arm+".tsv"))):
        run("./build-v9/moolbase_polymath_v6_executor",f("mechanisms.tsv"),
            f("selected_"+arm+".tsv"),f("executed_"+arm+".tsv"))
    else:
        sys.path.insert(0,"research/polymath_foundation/v6")
        from export_typed import RECEIPT_FIELDS
        Path(f("executed_"+arm+".tsv")).write_text("\t".join(RECEIPT_FIELDS)+"\n")
    run(sys.executable,f"{V8}/score_semantic.py",
        "--arm",arm,"--predictions",f("predictions_B2.json"),
        "--routes",f("routes_"+arm+".tsv"),
        "--native-results",f("executed_"+arm+".tsv"),
        "--gold",f("evaluator_only_gold.json"),
        "--out",f("evaluation_"+arm+".json"))
s2=json.loads(Path(f("evaluation_B2.json")).read_text())
s3=json.loads(Path(f("evaluation_B3.json")).read_text())
p=json.loads(Path(f("predictions_B2.json")).read_text())
if s2["overall"]!=s3["overall"] or s2["categories"]!=s3["categories"]:
    raise ValueError("native/non-native scoring did not agree")
if not p["actual_model_executed"] or len(p["records"])!=24:
    raise ValueError("real 7B model did not execute all frozen tasks")
summary={"experiment":"MOOL-POLYMATH-F9-G1-001",
  "actual_7b_model_executed":True,"model":p["model_name"],
  "model_sha256":p["model_sha256"],"github_sha":p["github_sha"],
  "overall":s2["overall"],"categories":s2["categories"],
  "native_equals_schema":True,
  "historical_v8_1_5b":"6/24; model and prompt both differ",
  "independent_blind":False,
  "model_weights_training":"NOT_RUN",
  "polymath_transfer":"NOT_DEMONSTRATED",
  "prompt_tokens":sum((r["usage"] or {}).get("prompt_tokens",0) for r in p["records"]),
  "completion_tokens":sum((r["usage"] or {}).get("completion_tokens",0) for r in p["records"]),
  "parser_failures":sum(bool(r["normalized_response"]["parser_error"]) for r in p["records"])}
Path(f("gate1_summary.json")).write_text(json.dumps(summary,sort_keys=True,indent=2)+"\n")
print("f9_gate1_actual_7B_and_native=PASS",summary["overall"],
      "parser_failures="+str(summary["parser_failures"]),flush=True)
