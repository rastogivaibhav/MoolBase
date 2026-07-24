#include "graphene/db.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace graphene;
namespace fs = std::filesystem;

static fs::path tmpdir(const std::string& name) {
  auto p = fs::temp_directory_path() / name;
  fs::remove_all(p);
  fs::create_directories(p);
  return p;
}

static bool payload_from_frame(const std::string& line, std::string* payload) {
  auto p1 = line.find('|');
  auto p2 = line.find('|', p1 == std::string::npos ? 0 : p1 + 1);
  if (p1 == std::string::npos || p2 == std::string::npos) return false;
  const auto len = static_cast<size_t>(std::stoull(line.substr(0, p1)));
  *payload = line.substr(p2 + 1);
  return payload->size() == len;
}

int main() {
  auto dir = tmpdir("graphenedb_physical_lattice_storage_test");
  DBOptions opt;
  opt.dimension = 2;
  opt.require_lattice = true;
  opt.physical_lattice_storage = true;

  GrapheneDB db;
  auto st = db.open(dir, opt);
  assert(st);

  uint32_t center = 0, east = 0, north_east = 0;
  st = db.put_node(NodeInput{"center", {1.0f, 0.0f}, 1, 1, true, false, false, {}, LatticeCoord{0, 0, 0}}, &center);
  assert(st && center == 0);
  st = db.put_node(NodeInput{"east", {0.9f, 0.1f}, 1, 1, false, true, false, {}, LatticeCoord{1, 0, 0}}, &east);
  assert(st && east == 1);
  st = db.put_node(NodeInput{"north_east", {0.8f, 0.2f}, 1, 1, false, false, true, {}, LatticeCoord{1, -1, 0}}, &north_east);
  assert(st && north_east == 2);

  uint32_t edge = 0;
  st = db.put_edge(EdgeInput{center, east, EdgeOrigin::Observed, EdgeRole::Supports, 0.95, {}, BondType::Sigma, DefectType::None, LayerCoupling::SameLayer, 1.0}, &edge);
  assert(st);
  st = db.compact();
  assert(st);

  std::string report;
  st = db.validate(&report);
  if (!st) {
    std::cerr << report;
    return 1;
  }

  const auto lattice_path = dir / "graphene.lattice";
  assert(fs::exists(lattice_path));
  std::ifstream in(lattice_path);
  assert(in);
  std::string line, payload;
  size_t cell_count = 0;
  bool saw_center_cell_with_east_neighbor = false;
  while (std::getline(in, line)) {
    if (!payload_from_frame(line, &payload)) return 2;
    if (payload.rfind("CELL\t", 0) != 0) continue;
    ++cell_count;
    // CELL layer q r node_id n0 n1 n2 n3 n4 n5 edge_ids
    if (payload == "CELL\t0\t0\t0\t0\t1\t2\t-1\t-1\t-1\t-1\t0") {
      saw_center_cell_with_east_neighbor = true;
    }
  }
  assert(cell_count == 3);
  assert(saw_center_cell_with_east_neighbor);

  std::string inspect;
  st = db.inspect(&inspect);
  assert(st);
  assert(inspect.find("physical_lattice_storage=true") != std::string::npos);
  assert(inspect.find("physical_lattice_bytes=0") == std::string::npos);
  in.close();
  db.close();
  fs::remove_all(dir);
  std::cout << "physical_lattice_storage_tests_passed=true\n";
  return 0;
}
