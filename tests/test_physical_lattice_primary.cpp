#include "graphene/db.hpp"
#include "graphene/lattice_placement.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
using namespace graphene;
namespace fs = std::filesystem;
static fs::path tmpdir(const char* name) {
  auto p = fs::temp_directory_path() / (std::string(name) + "_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::remove_all(p); fs::create_directories(p); return p;
}
int main() {
  auto dir = tmpdir("graphenedb_physical_lattice_primary_test");
  DBOptions opt; opt.dimension = 4; opt.require_lattice = true; opt.physical_lattice_storage = true; opt.physical_lattice_primary = true; opt.physical_lattice_radius = 8; opt.vector_index_kind = VectorIndexKind::KDTree;
  GrapheneDB db; auto st = db.open(dir, opt); assert(st);
  std::vector<uint32_t> ids;
  for (uint32_t i=0;i<37;++i) { NodeInput n; n.content="memory_"+std::to_string(i); n.vector={1.0f,float(i%3),0.0f,0.1f}; n.lattice=hex_spiral_coord(i,0); uint32_t id=0; st=db.put_node(n,&id); assert(st); ids.push_back(id); }
  auto ns = db.lattice_neighbors(0,2); assert(!ns.empty());
  std::string inspect; st=db.inspect(&inspect); assert(st); assert(inspect.find("physical_lattice_primary=true") != std::string::npos);
  db.close();
  assert(fs::exists(dir/"graphene.lattice.bin")); assert(fs::file_size(dir/"graphene.lattice.bin") > 128);
  assert(fs::exists(dir/"graphene.nodeidx")); assert(fs::file_size(dir/"graphene.nodeidx") > 64);
  GrapheneDB reopened; st=reopened.open(dir,opt); assert(st); assert(reopened.node_count()==37); auto ns2=reopened.lattice_neighbors(0,2); assert(ns2.size()==ns.size()); reopened.close();
  std::cout << "physical_lattice_primary_tests_passed=true\n";
}
