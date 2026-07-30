#include "graphene/db.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

using namespace graphene;
namespace fs = std::filesystem;
using Clock = std::chrono::high_resolution_clock;

namespace {

constexpr uint32_t kNodesPerIncident = 6;

struct QuerySeed {
  std::vector<float> vector;
  uint64_t signature{0};
  uint32_t expected_root{UINT32_MAX};
  std::string service;
};

uint64_t mix64(uint64_t value) {
  value += 0x9e3779b97f4a7c15ULL;
  value = (value ^ (value >> 30u)) * 0xbf58476d1ce4e5b9ULL;
  value = (value ^ (value >> 27u)) * 0x94d049bb133111ebULL;
  return value ^ (value >> 31u);
}

float signed_unit(uint64_t value) {
  constexpr double kScale = 1.0 / static_cast<double>(UINT32_MAX);
  return static_cast<float>(2.0 * static_cast<double>(value & UINT32_MAX) * kScale - 1.0);
}

std::vector<float> make_vec(uint32_t dim, uint32_t family, uint32_t slot) {
  std::vector<float> out(dim);
  for (uint32_t i = 0; i < dim; ++i) {
    const uint64_t coordinate = static_cast<uint64_t>(i) * 0xd6e8feb86659fd93ULL;
    const float incident_component =
        signed_unit(mix64(static_cast<uint64_t>(family) ^ coordinate));
    const float slot_component =
        signed_unit(mix64(static_cast<uint64_t>(family) ^
                          (static_cast<uint64_t>(slot) + 1u) * 0xa0761d6478bd642fULL ^
                          coordinate));
    out[i] = incident_component + 0.03f * slot_component;
  }
  return out;
}

double percentile(std::vector<double> values, double pct) {
  if (values.empty()) return 0.0;
  std::sort(values.begin(), values.end());
  size_t idx = static_cast<size_t>(pct * static_cast<double>(values.size() - 1));
  return values[idx];
}

int get_arg(int argc, char** argv, const std::string& key, int def) {
  for (int i = 1; i + 1 < argc; ++i) {
    if (argv[i] == key) return std::stoi(argv[i + 1]);
  }
  return def;
}

std::string get_arg(int argc, char** argv, const std::string& key, const std::string& def) {
  for (int i = 1; i + 1 < argc; ++i) {
    if (argv[i] == key) return argv[i + 1];
  }
  return def;
}

VectorIndexKind parse_vector_index_kind(const std::string& value) {
  if (value == "auto") return VectorIndexKind::Auto;
  if (value == "flat") return VectorIndexKind::Flat;
  if (value == "kdtree") return VectorIndexKind::KDTree;
  if (value == "faiss") return VectorIndexKind::Faiss;
  throw std::invalid_argument("unknown vector index: " + value);
}

void print_inspect_index_fields(const std::string& inspect) {
  for (const auto& prefix : {"vector_index_requested=", "vector_index="}) {
    auto pos = inspect.find(prefix);
    if (pos != std::string::npos) {
      auto end = inspect.find('\n', pos);
      std::cout << inspect.substr(pos, end == std::string::npos ? std::string::npos : end - pos)
                << "\n";
    }
  }
}

void require(Status st, const char* what) {
  if (!st) {
    std::cerr << "FAIL " << what << ": " << st.message << "\n";
    std::abort();
  }
}

const char* service_name(uint32_t incident) {
  static const char* kServices[] = {
      "gateway", "checkout", "catalog", "auth", "payments", "fulfillment", "search", "profile"};
  return kServices[incident % (sizeof(kServices) / sizeof(kServices[0]))];
}

const char* tenant_name(uint32_t incident) {
  static const char* kTenants[] = {"alpha", "beta", "gamma", "delta"};
  return kTenants[incident % (sizeof(kTenants) / sizeof(kTenants[0]))];
}

LatticeCoord incident_coord(uint32_t incident, uint32_t slot, int32_t base_layer) {
  static const LatticeCoord kOffsets[kNodesPerIncident] = {
      {0, 0, 0}, {1, 0, 0}, {1, -1, 0}, {0, 0, 1}, {-1, 1, 0}, {0, 1, 0},
  };
  const int32_t base_q = static_cast<int32_t>((incident % 1024u) * 6u);
  const int32_t base_r = static_cast<int32_t>((incident / 1024u) * 6u);
  return {
      base_q + kOffsets[slot].q,
      base_r + kOffsets[slot].r,
      base_layer + kOffsets[slot].layer,
  };
}

void add_relation(ExtractionInput* input,
                  const std::string& from,
                  const std::string& to,
                  EdgeRole role,
                  BondType bond,
                  double confidence,
                  double strength,
                  LayerCoupling coupling,
                  DefectType defect,
                  const std::string& flavor) {
  ExtractionRelation rel;
  rel.from_external_id = from;
  rel.to_external_id = to;
  rel.origin = bond == BondType::Defect ? EdgeOrigin::Inferred : EdgeOrigin::Observed;
  rel.role = role;
  rel.confidence = confidence;
  rel.evidence_id = input->source_id + "-" + flavor + "-" + from + "-" + to;
  rel.evidence_uri = "memory://graphenedb/stress/" + rel.evidence_id;
  rel.evidence_text = flavor + " stress relation";
  rel.metadata["graphene_profile"] = "rich-stress-100k";
  rel.metadata["flavor"] = flavor;
  rel.bond_type = bond;
  rel.defect_type = defect;
  rel.layer_coupling = coupling;
  rel.bond_strength = strength;
  input->relations.push_back(rel);
}

}  // namespace

int main(int argc, char** argv) {
  int incidents = get_arg(argc, argv, "--incidents", 5000);
  int queries = get_arg(argc, argv, "--queries", 200);
  uint32_t dim = static_cast<uint32_t>(get_arg(argc, argv, "--dim", 64));
  std::string vector_index = get_arg(argc, argv, "--vector-index", std::string("auto"));
  bool reopen_check = get_arg(argc, argv, "--reopen", 1) != 0;
  fs::path dir = fs::temp_directory_path() /
                 ("graphenedb_rc_stress_" + std::to_string(incidents) + "_" + std::to_string(dim));
  fs::remove_all(dir);

  DBOptions opt;
  opt.dimension = dim;
  opt.fsync_on_commit = false;
  opt.require_lattice = true;
  opt.vector_index_kind = parse_vector_index_kind(vector_index);
  opt.tuning.enable_lattice_retrieval = true;
  opt.tuning.min_candidate_floor = 64;

  GrapheneDB db;
  require(db.open(dir, opt), "open stress");
  std::string inspect;
  require(db.inspect(&inspect), "inspect stress");
  std::vector<QuerySeed> seeds;
  seeds.reserve(static_cast<size_t>(incidents));
  std::map<uint64_t, size_t> plane_node_counts;
  uint32_t defect_nodes = 0;
  uint32_t defect_edges = 0;
  uint32_t cross_layer_edges = 0;
  uint32_t synthetic_edges = 0;
  uint32_t previous_boundary_id = 0;
  bool have_previous_boundary = false;

  auto t0 = Clock::now();
  for (int i = 0; i < incidents; ++i) {
    ExtractionInput input;
    input.source_id = "incident-" + std::to_string(i);
    input.source_uri = "memory://graphenedb/stress/incident/" + std::to_string(i);
    input.extraction_run_id = "rich-stress-100k";
    input.layer = i % 3;
    input.incident = static_cast<uint32_t>(i);
    input.signature = signature_for(static_cast<uint32_t>(i % 16), static_cast<uint32_t>((i * 7) % 16));
    input.place_missing_lattice = false;
    input.idempotent = true;

    const std::string service = service_name(static_cast<uint32_t>(i));
    const std::string tenant = tenant_name(static_cast<uint32_t>(i));
    std::vector<std::string> ids;
    ids.reserve(kNodesPerIncident);
    for (uint32_t slot = 0; slot < kNodesPerIncident; ++slot) {
      ExtractionNode node;
      node.external_id = "node-" + std::to_string(slot);
      node.content = service + " incident " + std::to_string(i) + " fragment " + std::to_string(slot);
      node.vector = make_vec(dim, static_cast<uint32_t>(i) * 131u, slot);
      node.signature = input.signature;
      node.incident = static_cast<uint32_t>(i);
      node.metadata["service"] = service;
      node.metadata["tenant"] = tenant;
      node.metadata["graphene_profile"] = "rich-stress-100k";
      node.metadata["role_slot"] = std::to_string(slot);
      node.metadata["carbon_zone"] = slot == 3 ? "stacked" : (slot == 5 ? "defect" : "sheet");
      node.lattice = incident_coord(static_cast<uint32_t>(i), slot, input.layer);
      if (slot == 0) node.role = ExtractionRole::Root;
      if (slot == 2) node.role = ExtractionRole::Symptom;
      if (slot == 3) node.role = ExtractionRole::Impact;
      if (slot == 5 && (i % 9 == 0)) {
        node.defect_type = DefectType::Strain;
        ++defect_nodes;
      } else if (slot == 4 && (i % 7 == 0)) {
        node.defect_type = DefectType::Boundary;
        ++defect_nodes;
      }
      ids.push_back(node.external_id);
      input.nodes.push_back(std::move(node));
    }

    add_relation(&input, ids[0], ids[1], EdgeRole::Causal, BondType::Sigma, 0.96, 0.95,
                 LayerCoupling::SameLayer, DefectType::None, "sigma");
    add_relation(&input, ids[1], ids[2], EdgeRole::Causal, BondType::Pi, 0.92, 0.91,
                 LayerCoupling::SameLayer, DefectType::None, "pi");
    add_relation(&input, ids[0], ids[2], EdgeRole::Supports, BondType::Sigma, 0.90, 0.88,
                 LayerCoupling::SameLayer, DefectType::None, "root-symptom");
    add_relation(&input, ids[0], ids[3], EdgeRole::Supports, BondType::VanDerWaals, 0.84, 0.80,
                 (i % 2 == 0) ? LayerCoupling::BernalStacked : LayerCoupling::Twisted,
                 DefectType::None, "vdw");
    add_relation(&input, ids[0], ids[4], EdgeRole::Supports, BondType::Sigma, 0.87, 0.85,
                 LayerCoupling::SameLayer, DefectType::None, "context");
    const DefectType note_defect = (i % 9 == 0) ? DefectType::Strain : DefectType::None;
    add_relation(&input, ids[4], ids[5], EdgeRole::Supports, BondType::Pi, 0.80, 0.78,
                 LayerCoupling::SameLayer, note_defect, "note");
    add_relation(&input, ids[5], ids[2], EdgeRole::Supports, BondType::Defect, 0.58, 0.55,
                 LayerCoupling::SameLayer, note_defect, "defect");
    ++cross_layer_edges;
    ++defect_edges;

    ExtractionResult result;
    require(db.put_extraction(input, &result), "put extraction");
    auto root_it = result.external_to_node_id.find(ids[0]);
    if (root_it == result.external_to_node_id.end()) {
      std::cerr << "missing root id for stress incident\n";
      std::abort();
    }
    seeds.push_back({make_vec(dim, static_cast<uint32_t>(i) * 131u, 2), input.signature, root_it->second,
                     service});
    plane_node_counts[input.signature] += result.inserted_node_ids.size();

    if (have_previous_boundary) {
      auto boundary_it = result.external_to_node_id.find(ids[4]);
      if (boundary_it == result.external_to_node_id.end()) {
        std::cerr << "missing boundary id for synthetic stress bridge\n";
        std::abort();
      }
      EdgeInput bridge{previous_boundary_id, boundary_it->second, EdgeOrigin::Observed,
                       EdgeRole::Supports, 0.44};
      bridge.bond_type = BondType::Synthetic;
      bridge.defect_type = DefectType::Boundary;
      bridge.layer_coupling = LayerCoupling::Synthetic;
      bridge.bond_strength = 0.42;
      bridge.metadata["graphene_profile"] = "rich-stress-100k";
      bridge.metadata["flavor"] = "synthetic-boundary";
      require(db.put_edge(bridge), "put synthetic bridge");
      ++synthetic_edges;
    }
    auto boundary_it = result.external_to_node_id.find(ids[4]);
    if (boundary_it != result.external_to_node_id.end()) {
      previous_boundary_id = boundary_it->second;
      have_previous_boundary = true;
    }
  }
  auto t1 = Clock::now();
  if (incidents >= 100000) {
    std::cerr << "progress=ingest_done incidents=" << incidents << " nodes=" << db.node_count()
              << " edges=" << db.edge_count() << "\n";
  }

  std::vector<double> flat_ms;
  std::vector<double> causal_ms;
  std::vector<double> metadata_ms;
  int hits = 0;
  double candidate_pct_sum = 0.0;
  double lattice_neighbors_sum = 0.0;
  double lattice_score_sum = 0.0;
  for (int q = 0; q < queries; ++q) {
    const auto& seed = seeds[(static_cast<size_t>(q) * 7919u) % seeds.size()];
    uint64_t query_sig = seed.signature;
    size_t candidate_nodes = 0;
    int required = std::max(1, __builtin_popcountll(query_sig) - 1);
    for (const auto& kv : plane_node_counts) {
      if (__builtin_popcountll(kv.first & query_sig) >= required) candidate_nodes += kv.second;
    }
    candidate_pct_sum += static_cast<double>(candidate_nodes) /
                         static_cast<double>(std::max<size_t>(1, db.node_count()));

    auto a = Clock::now();
    auto flat = db.vector_search(seed.vector, 10);
    auto b = Clock::now();
    auto bundle = db.causal_search(seed.vector, query_sig, QueryMode::Empirical);
    auto c = Clock::now();
    auto ids = db.metadata_search("service", seed.service);
    auto d = Clock::now();
    assert(!ids.empty());
    (void)flat;
    flat_ms.push_back(std::chrono::duration<double, std::milli>(b - a).count());
    causal_ms.push_back(std::chrono::duration<double, std::milli>(c - b).count());
    metadata_ms.push_back(std::chrono::duration<double, std::milli>(d - c).count());
    lattice_neighbors_sum += static_cast<double>(bundle.lattice_neighbors.size());
    lattice_score_sum += bundle.lattice_score;
    if (!bundle.abstain && bundle.target_node == seed.expected_root) ++hits;
  }

  if (incidents >= 100000) {
    std::cerr << "progress=queries_done queries=" << queries << "\n";
  }
  std::string validation;
  require(db.validate(&validation), "validate stress");
  if (incidents >= 100000) {
    std::cerr << "progress=validate_done\n";
  }
  auto ingest_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
  double hit_rate = static_cast<double>(hits) / std::max(1, queries);
  std::cout << "rc_stress_gate=true\n";
  std::cout << "incidents=" << incidents << " nodes=" << db.node_count() << " edges=" << db.edge_count()
            << " dim=" << dim << "\n";
  print_inspect_index_fields(inspect);
  std::cout << "ingest_ms=" << ingest_ms
            << " ingest_nodes_per_sec=" << (db.node_count() * 1000.0 / std::max(1.0, ingest_ms))
            << "\n";
  std::cout << "vector_p50_ms=" << percentile(flat_ms, 0.50) << " vector_p95_ms="
            << percentile(flat_ms, 0.95) << " vector_p99_ms=" << percentile(flat_ms, 0.99) << "\n";
  std::cout << "causal_p50_ms=" << percentile(causal_ms, 0.50) << " causal_p95_ms="
            << percentile(causal_ms, 0.95) << " causal_p99_ms=" << percentile(causal_ms, 0.99)
            << "\n";
  std::cout << "metadata_service_p50_ms=" << percentile(metadata_ms, 0.50)
            << " metadata_service_p95_ms=" << percentile(metadata_ms, 0.95) << "\n";
  std::cout << "causal_hit_rate=" << hit_rate
            << " avg_candidate_pct=" << (candidate_pct_sum / std::max(1, queries)) * 100.0
            << " avg_lattice_neighbors=" << (lattice_neighbors_sum / std::max(1, queries))
            << " avg_lattice_score=" << (lattice_score_sum / std::max(1, queries)) << "\n";
  std::cout << "cross_layer_edges=" << cross_layer_edges << " defect_nodes=" << defect_nodes
            << " defect_edges=" << defect_edges << " synthetic_edges=" << synthetic_edges << "\n";
  assert(hit_rate >= 0.99);
  assert((candidate_pct_sum / std::max(1, queries)) < 0.30);
  require(db.close(), "close stress");
  if (incidents >= 100000) {
    std::cerr << "progress=close_done\n";
  }

  if (reopen_check) {
    auto r0 = Clock::now();
    GrapheneDB reopened;
    require(reopened.open(dir, opt), "reopen stress");
    auto r1 = Clock::now();
    assert(reopened.node_count() == static_cast<size_t>(incidents) * kNodesPerIncident);
    assert(reopened.edge_count() ==
           static_cast<size_t>(incidents) * 7u + static_cast<size_t>(std::max(0, incidents - 1)));
    std::string reopened_inspect;
    require(reopened.inspect(&reopened_inspect), "inspect reopened stress");
    print_inspect_index_fields(reopened_inspect);
    std::cout << "reopen_ms=" << std::chrono::duration<double, std::milli>(r1 - r0).count() << "\n";
    require(reopened.close(), "close reopened stress");
  }
  return 0;
}
