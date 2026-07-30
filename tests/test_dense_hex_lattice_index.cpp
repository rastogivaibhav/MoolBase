#include "graphene/db.hpp"
#include <cassert>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <random>
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

static NodeInput node_at(int q, int r, uint32_t incident = 1) {
  NodeInput n;
  n.content = "cell(" + std::to_string(q) + "," + std::to_string(r) + ")";
  n.vector = {static_cast<float>((q + 128) % 17) / 17.0f, static_cast<float>((r + 128) % 19) / 19.0f, 0.25f, 0.75f};
  n.signature = 100 + static_cast<uint64_t>((q + 256) * 1024 + (r + 256));
  n.incident = incident;
  n.lattice = LatticeCoord{q, r, 0};
  return n;
}

int main(int argc, char** argv) {
  int radius = 20;
  if (argc > 1) radius = std::max(1, std::stoi(argv[1]));
  auto dir = tmpdir("graphenedb_dense_hex_lattice_index_test_" + std::to_string(radius));
  DBOptions opt;
  opt.dimension = 4;
  opt.require_lattice = true;
  opt.physical_lattice_storage = true;
  opt.fsync_on_commit = false;
  opt.tuning.enable_lattice_retrieval = true;
  opt.tuning.lattice_max_hops = 2;

  GrapheneDB db;
  auto st = db.open(dir, opt);
  assert(st);

  std::vector<uint32_t> ids;
  ids.reserve(static_cast<size_t>(1 + 3 * radius * (radius + 1)));
  uint32_t center_id = UINT32_MAX;
  auto t0 = std::chrono::steady_clock::now();
  for (int q = -radius; q <= radius; ++q) {
    for (int r = -radius; r <= radius; ++r) {
      int s = -q - r;
      if (std::max({std::abs(q), std::abs(r), std::abs(s)}) > radius) continue;
      uint32_t id = 0;
      st = db.put_node(node_at(q, r), &id);
      if (!st) { std::cerr << st.message << "\n"; return 2; }
      if (q == 0 && r == 0) center_id = id;
      ids.push_back(id);
    }
  }
  auto t1 = std::chrono::steady_clock::now();

  std::string inspect;
  st = db.inspect(&inspect);
  assert(st);
  assert(inspect.find("dense_lattice_index=true") != std::string::npos);
  const auto expected = static_cast<size_t>(1 + 3 * radius * (radius + 1));
  assert(db.node_count() == expected);

  // The new HexWave/dense path discovers physical hex neighbours even when no
  // explicit edge record exists for every adjacency. Center has six neighbours.
  auto center_neighbors = db.lattice_neighbors(center_id, 1);
  if (center_neighbors.size() != 6) {
    std::cerr << "expected 6 physical neighbours for center, got " << center_neighbors.size() << "\n";
    return 3;
  }

  std::mt19937 rng(7);
  std::uniform_int_distribution<size_t> pick(0, ids.size() - 1);
  size_t probes = std::min<size_t>(1000, ids.size());
  size_t total_neighbors = 0;
  auto q0 = std::chrono::steady_clock::now();
  for (size_t i = 0; i < probes; ++i) {
    auto out = db.lattice_neighbors(ids[pick(rng)], 2);
    total_neighbors += out.size();
  }
  auto q1 = std::chrono::steady_clock::now();
  const double build_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
  const double query_ms = std::chrono::duration<double, std::milli>(q1 - q0).count();

  st = db.validate(&inspect);
  if (!st) { std::cerr << inspect; return 4; }
  st = db.close();
  assert(st);
  fs::remove_all(dir);
  std::cout << "dense_hex_lattice_index_tests_passed=true\n";
  std::cout << "dense_hex_radius=" << radius << "\n";
  std::cout << "dense_hex_nodes=" << expected << "\n";
  std::cout << "dense_hex_insert_ms=" << build_ms << "\n";
  std::cout << "dense_hex_neighbor_probes=" << probes << "\n";
  std::cout << "dense_hex_neighbor_query_ms=" << query_ms << "\n";
  std::cout << "dense_hex_neighbor_avg_us=" << (query_ms * 1000.0 / static_cast<double>(probes)) << "\n";
  std::cout << "dense_hex_total_neighbors=" << total_neighbors << "\n";
  return 0;
}
