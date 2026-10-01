#!/usr/bin/env bash
set -euo pipefail
output_dir="${1:?usage: build_wasm.sh ABSOLUTE_OUTPUT_DIR}"
mkdir -p "$output_dir"
em++ -std=c++20 -O2 -fexceptions -Iinclude \
 src/c_api.cpp src/db.cpp src/dialectic.cpp src/epistemic.cpp src/hyperedge.cpp \
 src/hypokosh.cpp src/kosh_adapter.cpp src/lattice_placement.cpp src/learning.cpp \
 src/platform_posix.cpp src/entity_resolution.cpp src/epistemic_receipt.cpp \
 src/epistemic_control.cpp src/escape.cpp src/fiber_bundle.cpp src/generic_relation.cpp \
 src/hypokosh_runtime.cpp src/model_world.cpp src/path_verifier.cpp \
 src/relation_ontology.cpp src/self_healing.cpp src/stability_critic.cpp \
 examples/customer_showcase/engine.cpp --no-entry \
 -sDISABLE_EXCEPTION_CATCHING=0 -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=268435456 \
 -sSTACK_SIZE=1048576 -sMODULARIZE=1 -sEXPORT_NAME=createMoolBase \
 '-sEXPORTED_FUNCTIONS=["_demo_reset","_demo_add","_demo_reopen"]' \
 '-sEXPORTED_RUNTIME_METHODS=["ccall","FS"]' -sENVIRONMENT=web,worker,node \
 -o "$output_dir/moolbase.js"
