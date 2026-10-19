#!/usr/bin/env bash
set -euo pipefail
R=reports/polymath_foundation_v8
mkdir -p "$R"
python3 research/polymath_foundation/v8/generate_pilot.py \
  --cards research/polymath_foundation/v3/cards.json \
  --mechanisms research/polymath_foundation/v6/mechanisms.json \
  --queries "$R/model_questions.json" --gold "$R/scorer_only_gold.json" \
  --manifest "$R/mechanisms.tsv" --receipt "$R/corpus_hashes.json"
for arm in B0 B1; do
  python3 research/polymath_foundation/v8/parse_semantics.py \
    --arm "$arm" --queries "$R/model_questions.json" \
    --manifest "$R/mechanisms.tsv" --out "$R/predictions_$arm.json"
done
# Inference server must have answered /health before this script starts.
python3 research/polymath_foundation/v8/parse_semantics.py \
  --arm B2 --queries "$R/model_questions.json" \
  --manifest "$R/mechanisms.tsv" --model-file /tmp/f8-model/qwen.gguf \
  --out "$R/predictions_B2.json" 2>&1 | tee "$R/model_inference.log"
for arm in B0 B1 B2; do
  python3 research/polymath_foundation/v8/prepare_routes.py \
    --mode queries --predictions "$R/predictions_$arm.json" \
    --manifest "$R/mechanisms.tsv" --queries "$R/normalized_$arm.tsv"
  python3 research/polymath_foundation/v8/prepare_routes.py \
    --mode baseline --predictions "$R/predictions_$arm.json" \
    --manifest "$R/mechanisms.tsv" --queries "$R/normalized_$arm.tsv" \
    --routes "$R/routes_$arm.tsv"
done
./build-v8/moolbase_polymath_v8_selector "$R/mechanisms.tsv" \
  "$R/normalized_B2.tsv" "$R/routes_B3.tsv" \
  2>&1 | tee "$R/native_selection.log"
python3 - <<'PY'
import csv
from pathlib import Path
root=Path("reports/polymath_foundation_v8")
def read(name):
    with (root/name).open(newline="") as f:return list(csv.DictReader(f,delimiter="\t"))
a,b=read("routes_B2.tsv"),read("routes_B3.tsv")
assert len(a)==len(b)==24
for x,y in zip(a,b):
    assert all(x[k]==y[k] for k in ("opaque_id","selection_status",
                        "mechanism_id","domain","reason","source_family"))
    if y["selection_status"]=="selected":assert y["native_node_id"]!="-"
print("f8_MoolBase_native_vs_schema_policy=EQUIVALENT")
PY
for arm in B0 B1 B2 B3; do
  predicted="$arm"
  if [ "$arm" = B3 ]; then predicted=B2; fi
  python3 research/polymath_foundation/v8/prepare_routes.py \
    --mode selected --predictions "$R/predictions_$predicted.json" \
    --manifest "$R/mechanisms.tsv" --queries "$R/normalized_$predicted.tsv" \
    --routes "$R/routes_$arm.tsv" --selected "$R/selected_$arm.tsv"
  if [ "$(wc -l < "$R/selected_$arm.tsv")" -gt 1 ]; then
    ./build-v8/moolbase_polymath_v6_executor "$R/mechanisms.tsv" \
      "$R/selected_$arm.tsv" "$R/executed_$arm.tsv"
  else
    python3 - "$R/executed_$arm.tsv" <<'PY'
from pathlib import Path
import sys
sys.path.insert(0,"research/polymath_foundation/v6")
from export_typed import RECEIPT_FIELDS
Path(sys.argv[1]).write_text("\t".join(RECEIPT_FIELDS)+"\n")
PY
  fi
  python3 research/polymath_foundation/v8/score_semantic.py \
    --arm "$arm" --predictions "$R/predictions_$predicted.json" \
    --routes "$R/routes_$arm.tsv" --native-results "$R/executed_$arm.tsv" \
    --gold "$R/scorer_only_gold.json" --out "$R/evaluation_$arm.json"
done
python3 - <<'PY'
import json
from pathlib import Path
root=Path("reports/polymath_foundation_v8")
e={a:json.loads((root/f"evaluation_{a}.json").read_text()) for a in ("B0","B1","B2","B3")}
assert e["B2"]["overall"]==e["B3"]["overall"]
assert e["B3"]["native_moolbase_execution"] is True
assert e["B2"]["real_model_executed"] is True
assert e["B3"]["full_dwm_opposition_cycle"]=="NOT_RUN"
print("f8_real_model_native_CXX_pipeline=PASS",{
  a:e[a]["overall"]["passed"] for a in e})
PY
