#include "graphene/db.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace graphene;
namespace fs = std::filesystem;

namespace {

constexpr uint32_t kIncidentMotifSize = 6;

struct QuerySeed {
  std::vector<float> vector;
  uint64_t signature{0};
  uint32_t expected_root{0};
  std::string service;
};

std::vector<float> vec(uint32_t dim, uint32_t family, uint32_t slot) {
  std::vector<float> out(dim);
  const float base = static_cast<float>((family % 700u) + 1u) * 0.0015f;
  const float slot_shift = static_cast<float>(slot) * 0.00025f;
  for (uint32_t i = 0; i < dim; ++i) {
    out[i] = base + slot_shift + static_cast<float>((i + 3u) * ((family % 11u) + 5u)) * 0.00006f;
  }
  return out;
}

double percentile(std::vector<double> values, double p) {
  if (values.empty()) return 0.0;
  std::sort(values.begin(), values.end());
  size_t idx = static_cast<size_t>((p / 100.0) * static_cast<double>(values.size() - 1));
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

const char* service_name(uint32_t incident) {
  static const char* kServices[] = {
      "gateway", "checkout", "catalog", "auth", "payments", "fulfillment", "search", "profile"};
  return kServices[incident % (sizeof(kServices) / sizeof(kServices[0]))];
}

const char* tenant_name(uint32_t incident) {
  static const char* kTenants[] = {"alpha", "beta", "gamma", "delta"};
  return kTenants[incident % (sizeof(kTenants) / sizeof(kTenants[0]))];
}

LatticeCoord motif_coord(uint32_t incident, uint32_t slot, int32_t base_layer) {
  static const LatticeCoord kOffsets[kIncidentMotifSize] = {
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
  rel.evidence_uri = "memory://graphenedb/bench/" + rel.evidence_id;
  rel.evidence_text = flavor + " relation";
  rel.metadata["graphene_profile"] = "rich-storage";
  rel.metadata["flavor"] = flavor;
  rel.bond_type = bond;
  rel.defect_type = defect;
  rel.layer_coupling = coupling;
  rel.bond_strength = strength;
  input->relations.push_back(rel);
}

}  // namespace

int main(int argc, char** argv) {
  uint32_t nodes = argc > 1 ? static_cast<uint32_t>(std::stoul(argv[1])) : 20000;
  uint32_t queries = argc > 2 ? static_cast<uint32_t>(std::stoul(argv[2])) : 100;
  uint32_t dim = argc > 3 ? static_cast<uint32_t>(std::stoul(argv[3])) : 64;
  std::string vector_index = get_arg(argc, argv, "--vector-index", std::string("auto"));

  fs::path dir = fs::temp_directory_path() / "graphenedb_rc5_storage_retrieval_bench";
  fs::remove_all(dir);
  DBOptions opt;
  opt.dimension = dim;
  opt.fsync_on_commit = false;
  opt.require_lattice = true;
  opt.vector_index_kind = parse_vector_index_kind(vector_index);
  opt.tuning.enable_lattice_retrieval = true;
  opt.tuning.min_candidate_floor = 64;

  GrapheneDB db;
  auto st = db.open(dir, opt);
  if (!st) {
    std::cerr << st.message << "\n";
    return 2;
  }
  std::string inspect;
  st = db.inspect(&inspect);
  if (!st) {
    std::cerr << st.message << "\n";
    return 2;
  }

  uint32_t remaining = nodes;
  uint32_t incident = 0;
  uint32_t previous_boundary_id = 0;
  bool have_previous_boundary = false;
  std::vector<QuerySeed> query_seeds;
  query_seeds.reserve((nodes / 3u) + 1u);
  uint32_t cross_layer_edges = 0;
  uint32_t defect_nodes = 0;
  uint32_t defect_edges = 0;
  uint32_t synthetic_edges = 0;

  auto ingest_start = std::chrono::steady_clock::now();
  while (remaining > 0) {
    const uint32_t count = std::min<uint32_t>(remaining, kIncidentMotifSize);
    ExtractionInput input;
    input.source_id = "incident-" + std::to_string(incident);
    input.source_uri = "memory://graphenedb/incident/" + std::to_string(incident);
    input.extraction_run_id = "rich-storage-bench";
    input.layer = static_cast<int32_t>(incident % 3u);
    input.incident = incident;
    input.signature = signature_for(incident % 8u, (incident * 5u) % 16u);
    input.place_missing_lattice = false;
    input.idempotent = true;

    const std::string service = service_name(incident);
    const std::string tenant = tenant_name(incident);
    std::vector<std::string> ids;
    ids.reserve(count);
    for (uint32_t slot = 0; slot < count; ++slot) {
      ExtractionNode node;
      node.external_id = "node-" + std::to_string(slot);
      node.content = service + " incident " + std::to_string(incident) + " fragment " +
                     std::to_string(slot);
      node.vector = vec(dim, incident * 97u, slot);
      node.signature = input.signature;
      node.incident = incident;
      node.metadata["service"] = service;
      node.metadata["tenant"] = tenant;
      node.metadata["graphene_profile"] = "rich-storage";
      node.metadata["role_slot"] = std::to_string(slot);
      node.metadata["carbon_zone"] = slot == 3 ? "stacked" : (slot == 5 ? "defect" : "sheet");
      node.lattice = motif_coord(incident, slot, input.layer);
      if (slot == 0) node.role = ExtractionRole::Root;
      if (slot == 2) node.role = ExtractionRole::Symptom;
      if (slot == 3) node.role = ExtractionRole::Impact;
      if (slot == 5 && (incident % 9u == 0u)) {
        node.defect_type = DefectType::Strain;
        ++defect_nodes;
      } else if (slot == 4 && (incident % 7u == 0u)) {
        node.defect_type = DefectType::Boundary;
        ++defect_nodes;
      }
      ids.push_back(node.external_id);
      input.nodes.push_back(std::move(node));
    }

    if (count > 1) {
      add_relation(&input, ids[0], ids[1], EdgeRole::Causal, BondType::Sigma, 0.96, 0.95,
                   LayerCoupling::SameLayer, DefectType::None, "sigma");
    }
    if (count > 2) {
      add_relation(&input, ids[1], ids[2], EdgeRole::Causal, BondType::Pi, 0.91, 0.90,
                   LayerCoupling::SameLayer, DefectType::None, "pi");
      add_relation(&input, ids[0], ids[2], EdgeRole::Supports, BondType::Sigma, 0.89, 0.87,
                   LayerCoupling::SameLayer, DefectType::None, "root-symptom");
    }
    if (count > 3) {
      ++cross_layer_edges;
      add_relation(&input, ids[0], ids[3], EdgeRole::Supports, BondType::VanDerWaals, 0.83, 0.80,
                   incident % 2u == 0u ? LayerCoupling::BernalStacked : LayerCoupling::Twisted,
                   DefectType::None, "vdw");
    }
    if (count > 4) {
      add_relation(&input, ids[0], ids[4], EdgeRole::Supports, BondType::Sigma, 0.87, 0.85,
                   LayerCoupling::SameLayer, DefectType::None, "context");
    }
    if (count > 5) {
      const DefectType defect = (incident % 9u == 0u) ? DefectType::Strain : DefectType::None;
      add_relation(&input, ids[4], ids[5], EdgeRole::Supports, BondType::Pi, 0.80, 0.78,
                   LayerCoupling::SameLayer, defect, "note");
      add_relation(&input, ids[5], ids[2], EdgeRole::Supports, BondType::Defect, 0.56, 0.54,
                   LayerCoupling::SameLayer, defect, "defect");
      ++defect_edges;
    }

    ExtractionResult result;
    st = db.put_extraction(input, &result);
    if (!st) {
      std::cerr << st.message << "\n";
      return 2;
    }

    if (count > 2) {
      auto root_it = result.external_to_node_id.find(ids[0]);
      if (root_it == result.external_to_node_id.end()) {
        std::cerr << "missing root node id for incident workload\n";
        return 2;
      }
      query_seeds.push_back({vec(dim, incident * 97u, 2), input.signature, root_it->second, service});
    }

    if (have_previous_boundary && count > 4) {
      auto boundary_it = result.external_to_node_id.find(ids[4]);
      if (boundary_it == result.external_to_node_id.end()) {
        std::cerr << "missing boundary node id for synthetic bridge\n";
        return 2;
      }
      EdgeInput bridge{previous_boundary_id, boundary_it->second, EdgeOrigin::Observed, EdgeRole::Supports,
                       0.44};
      bridge.bond_type = BondType::Synthetic;
      bridge.defect_type = DefectType::Boundary;
      bridge.layer_coupling = LayerCoupling::Synthetic;
      bridge.bond_strength = 0.42;
      bridge.metadata["graphene_profile"] = "rich-storage";
      bridge.metadata["flavor"] = "synthetic-boundary";
      st = db.put_edge(bridge);
      if (!st) {
        std::cerr << st.message << "\n";
        return 2;
      }
      ++synthetic_edges;
    }

    auto tail_it = result.external_to_node_id.find(ids.back());
    if (tail_it != result.external_to_node_id.end()) {
      previous_boundary_id = tail_it->second;
      have_previous_boundary = true;
    }

    remaining -= count;
    ++incident;
  }
  if (query_seeds.empty()) {
    std::cerr << "no storage query seeds were generated\n";
    return 2;
  }
  auto ingest_end = std::chrono::steady_clock::now();
  double ingest_ms = std::chrono::duration<double, std::milli>(ingest_end - ingest_start).count();

  std::vector<double> vector_ms;
  std::vector<double> causal_ms;
  std::vector<double> metadata_ms;
  uint32_t causal_root_hits = 0;
  double lattice_neighbors_sum = 0.0;
  double lattice_score_sum = 0.0;
  for (uint32_t q = 0; q < queries; ++q) {
    const auto& seed = query_seeds[q % query_seeds.size()];
    auto t1 = std::chrono::steady_clock::now();
    auto vr = db.vector_search(seed.vector, 10);
    auto t2 = std::chrono::steady_clock::now();
    auto bundle = db.causal_search(seed.vector, seed.signature, QueryMode::Empirical);
    auto t3 = std::chrono::steady_clock::now();
    auto ids = db.metadata_search("service", seed.service);
    auto t4 = std::chrono::steady_clock::now();
    (void)vr;
    if (ids.empty()) {
      std::cerr << "service metadata search returned no rows\n";
      return 2;
    }
    if (!bundle.abstain && bundle.target_node == seed.expected_root) ++causal_root_hits;
    vector_ms.push_back(std::chrono::duration<double, std::milli>(t2 - t1).count());
    causal_ms.push_back(std::chrono::duration<double, std::milli>(t3 - t2).count());
    metadata_ms.push_back(std::chrono::duration<double, std::milli>(t4 - t3).count());
    lattice_neighbors_sum += static_cast<double>(bundle.lattice_neighbors.size());
    lattice_score_sum += bundle.lattice_score;
  }

  auto before_reopen = std::chrono::steady_clock::now();
  st = db.close();
  if (!st) {
    std::cerr << st.message << "\n";
    return 2;
  }
  GrapheneDB reopened;
  st = reopened.open(dir, opt);
  if (!st) {
    std::cerr << st.message << "\n";
    return 2;
  }
  auto after_reopen = std::chrono::steady_clock::now();
  double reopen_ms = std::chrono::duration<double, std::milli>(after_reopen - before_reopen).count();
  std::string report;
  st = reopened.validate(&report);
  if (!st) {
    std::cerr << report << "\n";
    return 2;
  }
  reopened.close();

  std::cout << "rc5_storage_retrieval_bench=true\n";
  std::cout << "incidents=" << incident << " nodes=" << db.node_count() << " edges=" << db.edge_count()
            << " dim=" << dim << " queries=" << queries << "\n";
  print_inspect_index_fields(inspect);
  std::cout << "ingest_ms=" << ingest_ms
            << " ingest_nodes_per_sec=" << (static_cast<double>(db.node_count()) / (ingest_ms / 1000.0))
            << "\n";
  std::cout << "vector_p50_ms=" << percentile(vector_ms, 50)
            << " vector_p95_ms=" << percentile(vector_ms, 95) << "\n";
  std::cout << "causal_lattice_p50_ms=" << percentile(causal_ms, 50)
            << " causal_lattice_p95_ms=" << percentile(causal_ms, 95) << "\n";
  std::cout << "metadata_service_p50_ms=" << percentile(metadata_ms, 50)
            << " metadata_service_p95_ms=" << percentile(metadata_ms, 95) << "\n";
  std::cout << "causal_root_hit_rate="
            << (static_cast<double>(causal_root_hits) / static_cast<double>(std::max<uint32_t>(1, queries)))
            << " avg_lattice_neighbors="
            << (lattice_neighbors_sum / static_cast<double>(std::max<uint32_t>(1, queries)))
            << " avg_lattice_score="
            << (lattice_score_sum / static_cast<double>(std::max<uint32_t>(1, queries))) << "\n";
  std::cout << "cross_layer_edges=" << cross_layer_edges << " defect_nodes=" << defect_nodes
            << " defect_edges=" << defect_edges << " synthetic_edges=" << synthetic_edges << "\n";
  std::cout << "reopen_ms=" << reopen_ms << "\n";
  return 0;
}
