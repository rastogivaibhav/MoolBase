#!/usr/bin/env python3
"""Build a self-contained customer source ZIP, checksum, manifest and SPDX inventory."""
import hashlib,json,subprocess,sys,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=Path(sys.argv[1]).resolve();OUT.parent.mkdir(parents=True,exist_ok=True)
commit=subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'],text=True).strip()
version=next(line.split(':',1)[1].strip().strip('"') for line in (ROOT/'CITATION.cff').read_text().splitlines() if line.startswith('version:'))
entries={}
for folder in ['src','include','clients/python','examples/customer_showcase','third_party/emscripten']:
 for path in (ROOT/folder).rglob('*'):
  if path.is_file() and '__pycache__' not in path.parts and path.suffix not in ['.zip','.png'] and not path.name.endswith(('.manifest.json','.sha256','.spdx.json')):entries[str(path.relative_to(ROOT))]=path.read_bytes()
for path in (ROOT/'tools').glob('graphenedb_server*'):entries[str(path.relative_to(ROOT))]=path.read_bytes()
for name in ['tools/server_runtime.hpp','LICENSE','NOTICE','THIRD_PARTY_NOTICES.md','DCO-1.1.txt','docs/AGENT_INTEGRATION.md','docs/RELEASE_v0.6.0-alpha.2.md','docs/ARCHITECTURE.md','docs/STORAGE_FORMAT.md']:entries[name]=(ROOT/name).read_bytes()
entries['README.md']=(ROOT/'examples/customer_showcase/README.md').read_bytes()
files=['c_api','db','dialectic','epistemic','hyperedge','hypokosh','kosh_adapter','lattice_placement','learning','entity_resolution','epistemic_receipt','epistemic_control','escape','fiber_bundle','generic_relation','hypokosh_runtime','model_world','path_verifier','relation_ontology','self_healing','stability_critic']
cmake='''cmake_minimum_required(VERSION 3.16)
project(MoolBaseCustomerShowcase LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
find_package(Threads REQUIRED)
set(PLATFORM_SOURCE src/platform_posix.cpp)
if(WIN32)
  set(PLATFORM_SOURCE src/platform_windows.cpp)
endif()
add_library(graphenedb\n'''+''.join(f'  src/{name}.cpp\n' for name in files)+'''  ${PLATFORM_SOURCE})
target_include_directories(graphenedb PUBLIC include)
target_link_libraries(graphenedb PUBLIC Threads::Threads)
add_executable(moolbase_customer_showcase examples/customer_showcase/engine.cpp)
target_link_libraries(moolbase_customer_showcase PRIVATE graphenedb)
option(GRAPHENEDB_BUILD_SERVER "Build the POSIX HTTP example server" OFF)
if(GRAPHENEDB_BUILD_SERVER)
  if(WIN32)
    message(FATAL_ERROR "The HTTP server requires POSIX")
  endif()
  add_executable(graphenedb_server tools/graphenedb_server.cpp)
  target_link_libraries(graphenedb_server PRIVATE graphenedb)
endif()
'''
entries['CMakeLists.txt']=cmake.encode()
entries['BUILD_PROVENANCE.json']=(json.dumps({'repository':'https://github.com/rastogivaibhav/graphenedb_v1','source_commit':commit,'version':version,'dataClass':'realistic synthetic fixtures','browserBuild':'Emscripten 3.1.74 when included'},indent=2)+'\n').encode()
with zipfile.ZipFile(OUT,'w',zipfile.ZIP_DEFLATED) as archive:
 for name,data in sorted(entries.items()):archive.writestr(name,data)
sha=lambda data:hashlib.sha256(data).hexdigest()
manifest={'schema_version':2,'version':version,'cmake_package_version':'0.6.0','license':'Apache-2.0','source_commit':commit,'package':OUT.name,'package_size':OUT.stat().st_size,'package_sha256':sha(OUT.read_bytes()),'files':[{'path':name,'sha256':sha(data),'size':len(data)}for name,data in sorted(entries.items())]}
manifest_path=Path(str(OUT)+'.manifest.json');manifest_path.write_text(json.dumps(manifest,indent=2)+'\n')
Path(str(OUT)+'.sha256').write_text(f'{manifest["package_sha256"]}  {OUT.name}\n')
subprocess.run([sys.executable,str(ROOT/'scripts/generate_distribution_sbom.py'),str(manifest_path),str(OUT)+'.spdx.json'],check=True)
print(OUT)
