#include "graphene/db.hpp"
#include <algorithm>
#include <cassert>
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

static NodeInput node_at(int q, int r) {
  NodeInput n;
  n.content = "dense cell " + std::to_string(q) + "," + std::to_string(r);
  n.vector = {static_cast<float>((q + 4096) % 29) / 29.0f,
              static_cast<float>((r + 4096) % 31) / 31.0f,
              static_cast<float>((q - r + 4096) % 37) / 37.0f,
              0.5f};
  n.signature = 9000 + static_cast<uint64_t>((q + 4096) * 8192 + (r + 4096));
  n.incident = 1;
  n.lattice = LatticeCoord{q, r, 0};
  return n;
}

int main(int argc, char** argv) {
  int radius = 90; // 24,571 cells; use 182 for ~99,373 cells on stronger hosts.
  if (argc > 1) radius = std::max(1, std::stoi(argv[1]));
  size_t probes = 5000;
  if (argc > 2) probes = static_cast<size_t>(std::stoull(argv[2]));
  auto dir = tmpdir("graphenedb_dense_hex_lattice_stress_" + std::to_string(radius));

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

  BatchInput batch;
  batch.nodes.reserve(static_cast<size_t>(1 + 3 * radius * (radius + 1)));
  for (int q = -radius; q <= radius; ++q) {
    for (int r = -radius; r <= radius; ++r) {
      int s = -q - r;
      if (std::max({std::abs(q), std::abs(r), std::abs(s)}) > radius) continue;
      batch.nodes.push_back(node_at(q, r));
    }
  }

  auto t0 = std::chrono::steady_clock::now();
  BatchResult result;
  st = db.put_batch(batch, &result);
  auto t1 = std::chrono::steady_clock::now();
  if (!st) { std::cerr << st.message << "\n"; return 2; }
  assert(result.node_ids.size() == batch.nodes.size());

  std::string inspect;
  st = db.inspect(&inspect);
  assert(st);
  if (inspect.find("dense_lattice_index=true") == std::string::npos) {
    std::cerr << inspect;
    return 3;
  }

  std::mt19937 rng(11);
  std::uniform_int_distribution<size_t> pick(0, result.node_ids.size() - 1);
  size_t total_neighbors = 0;
  auto q0 = std::chrono::steady_clock::now();
  for (size_t i = 0; i < probes; ++i) {
    auto out = db.lattice_neighbors(result.node_ids[pick(rng)], 2);
    total_neighbors += out.size();
  }
  auto q1 = std::chrono::steady_clock::now();

  st = db.validate(&inspect);
  if (!st) { std::cerr << inspect; return 4; }
  const double insert_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
  const double query_ms = std::chrono::duration<double, std::milli>(q1 - q0).count();
  std::cout << "dense_hex_lattice_stress_passed=true\n";
  std::cout << "dense_hex_stress_radius=" << radius << "\n";
  std::cout << "dense_hex_stress_nodes=" << result.node_ids.size() << "\n";
  std::cout << "dense_hex_stress_batch_insert_ms=" << insert_ms << "\n";
  std::cout << "dense_hex_stress_neighbor_probes=" << probes << "\n";
  std::cout << "dense_hex_stress_neighbor_query_ms=" << query_ms << "\n";
  std::cout << "dense_hex_stress_neighbor_avg_us=" << (query_ms * 1000.0 / static_cast<double>(probes)) << "\n";
  std::cout << "dense_hex_stress_total_neighbors=" << total_neighbors << "\n";
  st = db.close();
  assert(st);
  fs::remove_all(dir);
  return 0;
}
