#include "graphene/db.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace graphene;
namespace fs = std::filesystem;
using Clock = std::chrono::high_resolution_clock;

namespace {

constexpr uint32_t kMotifSize = 6;
constexpr uint32_t kPreferredDocNodes = 24;

struct QuerySeed {
  std::vector<float> vector;
  uint64_t signature{0};
  uint32_t expected_root{UINT32_MAX};
  std::string service;
};

struct DocumentSeed {
  std::vector<float> vector;
  uint64_t signature{0};
  std::string root_external_id;
};

struct BuiltDocument {
  ExtractionInput input;
  std::vector<DocumentSeed> seeds;
  uint32_t cross_layer_edges{0};
  uint32_t defect_nodes{0};
  uint32_t defect_edges{0};
  uint32_t synthetic_edges{0};
};

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

std::vector<float> vec(uint32_t dim, uint32_t family, uint32_t slot) {
  std::vector<float> out(dim);
  const float base = static_cast<float>((family % 900u) + 1u) * 0.0011f;
  const float slot_shift = static_cast<float>(slot) * 0.00019f;
  for (uint32_t i = 0; i < dim; ++i) {
    out[i] = base + slot_shift + static_cast<float>((i + 3u) * ((family % 19u) + 5u)) * 0.00004f;
  }
  return out;
}

void require(Status st, const char* what) {
  if (!st) {
    std::cerr << "FAIL " << what << ": " << st.message << "\n";
    std::abort();
  }
}

double percentile(std::vector<double> values, double pct) {
  if (values.empty()) return 0.0;
  std::sort(values.begin(), values.end());
  size_t idx = static_cast<size_t>(pct * static_cast<double>(values.size() - 1));
  return values[idx];
}

const char* service_name(uint32_t doc) {
  static const char* kServices[] = {
      "gateway", "checkout", "catalog", "auth", "payments", "fulfillment", "search", "profile"};
  return kServices[doc % (sizeof(kServices) / sizeof(kServices[0]))];
}

const char* tenant_name(uint32_t doc) {
  static const char* kTenants[] = {"alpha", "beta", "gamma", "delta"};
  return kTenants[doc % (sizeof(kTenants) / sizeof(kTenants[0]))];
}

LatticeCoord doc_coord(uint32_t cluster_index, uint32_t slot, int32_t base_layer) {
  static const LatticeCoord kOffsets[kMotifSize] = {
      {0, 0, 0}, {1, 0, 0}, {1, -1, 0}, {0, 0, 1}, {-1, 1, 0}, {0, 1, 0},
  };
  const int32_t base_q = static_cast<int32_t>((cluster_index % 1024u) * 6u);
  const int32_t base_r = static_cast<int32_t>((cluster_index / 1024u) * 6u);
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
  rel.evidence_uri = "memory://graphenedb/1m/" + rel.evidence_id;
  rel.evidence_text = flavor + " 1m relation";
  rel.metadata["graphene_profile"] = "rich-stress-1m";
  rel.metadata["flavor"] = flavor;
  rel.bond_type = bond;
  rel.defect_type = defect;
  rel.layer_coupling = coupling;
  rel.bond_strength = strength;
  input->relations.push_back(rel);
}

BuiltDocument build_document(uint32_t doc, uint32_t nodes_in_doc, uint32_t dim) {
  BuiltDocument built;
  built.input.source_id = "doc-" + std::to_string(doc);
  built.input.source_uri = "memory://graphenedb/1m/doc/" + std::to_string(doc);
  built.input.extraction_run_id = "rich-stress-1m";
  built.input.layer = static_cast<int32_t>(doc % 3u);
  built.input.incident = doc;
  built.input.signature = signature_for(doc % 16u, (doc * 5u) % 16u);
  built.input.place_missing_lattice = false;
  built.input.idempotent = true;

  const std::string service = service_name(doc);
  const std::string tenant = tenant_name(doc);
  uint32_t remaining = nodes_in_doc;
  uint32_t motif = 0;
  std::string previous_boundary_id;
  while (remaining > 0) {
    const uint32_t count = std::min<uint32_t>(remaining, kMotifSize);
    const uint32_t cluster_index = doc * 64u + motif;
    const int32_t base_layer = built.input.layer + static_cast<int32_t>((motif % 2u) * 2u);
    std::vector<std::string> ids;
    ids.reserve(count);
    for (uint32_t slot = 0; slot < count; ++slot) {
      ExtractionNode node;
      node.external_id = "motif-" + std::to_string(motif) + "-node-" + std::to_string(slot);
      node.content = service + " doc " + std::to_string(doc) + " fragment " + std::to_string(motif) +
                     ":" + std::to_string(slot);
      node.vector = vec(dim, doc * 257u + motif * 23u, slot);
      node.signature = built.input.signature;
      node.incident = doc;
      node.metadata["service"] = service;
      node.metadata["tenant"] = tenant;
      node.metadata["graphene_profile"] = "rich-stress-1m";
      node.metadata["motif"] = std::to_string(motif);
      node.metadata["role_slot"] = std::to_string(slot);
      node.metadata["carbon_zone"] = slot == 3 ? "stacked" : (slot == 5 ? "defect" : "sheet");
      node.lattice = doc_coord(cluster_index, slot, base_layer);
      if (slot == 0) node.role = ExtractionRole::Root;
      if (slot == 2) node.role = ExtractionRole::Symptom;
      if (slot == 3) node.role = ExtractionRole::Impact;
      if (slot == 5 && ((doc + motif) % 11u == 0u)) {
        node.defect_type = DefectType::Strain;
        ++built.defect_nodes;
      } else if (slot == 4 && ((doc + motif) % 7u == 0u)) {
        node.defect_type = DefectType::Boundary;
        ++built.defect_nodes;
      }
      ids.push_back(node.external_id);
      built.input.nodes.push_back(std::move(node));
    }

    if (count > 1) {
      add_relation(&built.input, ids[0], ids[1], EdgeRole::Causal, BondType::Sigma, 0.96, 0.95,
                   LayerCoupling::SameLayer, DefectType::None, "sigma");
    }
    if (count > 2) {
      add_relation(&built.input, ids[1], ids[2], EdgeRole::Causal, BondType::Pi, 0.91, 0.90,
                   LayerCoupling::SameLayer, DefectType::None, "pi");
      add_relation(&built.input, ids[0], ids[2], EdgeRole::Supports, BondType::Sigma, 0.89, 0.87,
                   LayerCoupling::SameLayer, DefectType::None, "root-symptom");
      built.seeds.push_back({vec(dim, doc * 257u + motif * 23u, 2), built.input.signature, ids[0]});
    }
    if (count > 3) {
      add_relation(&built.input, ids[0], ids[3], EdgeRole::Supports, BondType::VanDerWaals, 0.84,
                   0.80, (motif % 2u == 0u) ? LayerCoupling::BernalStacked : LayerCoupling::Twisted,
                   DefectType::None, "vdw");
      ++built.cross_layer_edges;
    }
    if (count > 4) {
      add_relation(&built.input, ids[0], ids[4], EdgeRole::Supports, BondType::Sigma, 0.87, 0.85,
                   LayerCoupling::SameLayer, DefectType::None, "context");
    }
    if (count > 5) {
      const DefectType defect = ((doc + motif) % 11u == 0u) ? DefectType::Strain : DefectType::None;
      add_relation(&built.input, ids[4], ids[5], EdgeRole::Supports, BondType::Pi, 0.80, 0.78,
                   LayerCoupling::SameLayer, defect, "note");
      add_relation(&built.input, ids[5], ids[2], EdgeRole::Supports, BondType::Defect, 0.56, 0.54,
                   LayerCoupling::SameLayer, defect, "defect");
      ++built.defect_edges;
    }
    if (!previous_boundary_id.empty() && count > 4) {
      add_relation(&built.input, previous_boundary_id, ids[4], EdgeRole::Supports, BondType::Synthetic,
                   0.42, 0.40, LayerCoupling::Synthetic, DefectType::Boundary, "synthetic-boundary");
      ++built.synthetic_edges;
    }
    previous_boundary_id = ids.back();
    remaining -= count;
    ++motif;
  }
  return built;
}

}  // namespace

int main(int argc, char** argv) {
  int nodes = get_arg(argc, argv, "--nodes", 1000000);
  int queries = get_arg(argc, argv, "--queries", 5);
  uint32_t dim = static_cast<uint32_t>(get_arg(argc, argv, "--dim", 2));
  std::string vector_index = get_arg(argc, argv, "--vector-index", std::string("auto"));
  bool reopen = get_arg(argc, argv, "--reopen", 1) != 0;
  fs::path dir = fs::temp_directory_path() /
                 ("graphenedb_rc_1m_storage_" + std::to_string(nodes) + "_" + std::to_string(dim));
  fs::remove_all(dir);

  DBOptions opt;
  opt.dimension = dim;
  opt.fsync_on_commit = false;
  opt.wal_rotate_bytes = 0;
  opt.require_lattice = true;
  opt.vector_index_kind = parse_vector_index_kind(vector_index);
  opt.tuning.enable_lattice_retrieval = true;
  opt.tuning.min_candidate_floor = 64;

  GrapheneDB db;
  require(db.open(dir, opt), "open 1m storage");
  std::string inspect;
  require(db.inspect(&inspect), "inspect 1m storage");
  auto t0 = Clock::now();

  std::vector<QuerySeed> seeds;
  uint64_t inserted_nodes = 0;
  uint64_t inserted_edges = 0;
  uint32_t cross_layer_edges = 0;
  uint32_t defect_nodes = 0;
  uint32_t defect_edges = 0;
  uint32_t synthetic_edges = 0;
  uint32_t doc = 0;
  int remaining = nodes;
  while (remaining > 0) {
    uint32_t nodes_in_doc = static_cast<uint32_t>(std::min<int>(remaining, static_cast<int>(kPreferredDocNodes)));
    auto built = build_document(doc, nodes_in_doc, dim);
    ExtractionResult result;
    require(db.put_extraction(built.input, &result), "put 1m extraction");
    inserted_nodes += result.inserted_node_ids.size();
    inserted_edges += result.inserted_edge_ids.size();
    cross_layer_edges += built.cross_layer_edges;
    defect_nodes += built.defect_nodes;
    defect_edges += built.defect_edges;
    synthetic_edges += built.synthetic_edges;
    for (const auto& seed : built.seeds) {
      auto it = result.external_to_node_id.find(seed.root_external_id);
      if (it == result.external_to_node_id.end()) {
        std::cerr << "missing root id for 1m query seed\n";
        std::abort();
      }
      seeds.push_back({seed.vector, seed.signature, it->second, service_name(doc)});
    }
    remaining -= static_cast<int>(nodes_in_doc);
    ++doc;
  }
  auto t1 = Clock::now();
  assert(db.node_count() == static_cast<size_t>(nodes));
  assert(inserted_nodes == static_cast<uint64_t>(nodes));
  if (queries > 0 && seeds.empty()) {
    std::cerr << "no 1m query seeds were generated\n";
    std::abort();
  }

  std::vector<double> vector_ms;
  std::vector<double> causal_ms;
  std::vector<double> metadata_ms;
  int hits = 0;
  double lattice_neighbors_sum = 0.0;
  double lattice_score_sum = 0.0;
  for (int q = 0; q < queries; ++q) {
    const auto& seed = seeds[(static_cast<size_t>(q) * 7919u) % seeds.size()];
    auto a = Clock::now();
    auto vector = db.vector_search(seed.vector, 10);
    auto b = Clock::now();
    auto bundle = db.causal_search(seed.vector, seed.signature, QueryMode::Empirical);
    auto c = Clock::now();
    auto ids = db.metadata_search("service", seed.service);
    auto d = Clock::now();
    assert(!vector.empty());
    assert(!ids.empty());
    vector_ms.push_back(std::chrono::duration<double, std::milli>(b - a).count());
    causal_ms.push_back(std::chrono::duration<double, std::milli>(c - b).count());
    metadata_ms.push_back(std::chrono::duration<double, std::milli>(d - c).count());
    lattice_neighbors_sum += static_cast<double>(bundle.lattice_neighbors.size());
    lattice_score_sum += bundle.lattice_score;
    if (!bundle.abstain && bundle.target_node == seed.expected_root) ++hits;
  }
  std::sort(vector_ms.begin(), vector_ms.end());
  std::sort(causal_ms.begin(), causal_ms.end());
  std::sort(metadata_ms.begin(), metadata_ms.end());

  std::string report;
  require(db.validate(&report), "validate 1m storage");
  require(db.close(), "close 1m storage");
  auto t2 = Clock::now();
  double reopen_ms = 0.0;
  if (reopen) {
    auto r0 = Clock::now();
    GrapheneDB reopened;
    require(reopened.open(dir, opt), "reopen 1m storage");
    auto r1 = Clock::now();
    assert(reopened.node_count() == static_cast<size_t>(nodes));
    require(reopened.validate(&report), "validate reopened 1m");
    std::string reopened_inspect;
    require(reopened.inspect(&reopened_inspect), "inspect reopened 1m");
    print_inspect_index_fields(reopened_inspect);
    reopen_ms = std::chrono::duration<double, std::milli>(r1 - r0).count();
    require(reopened.close(), "close reopened 1m");
  }

  const double hit_rate = queries > 0 ? static_cast<double>(hits) / static_cast<double>(queries) : 1.0;
  std::cout << "rc_1m_storage_gate=true\n";
  std::cout << "nodes=" << nodes << " docs=" << doc << " inserted_edges=" << inserted_edges << " dim=" << dim
            << " queries=" << queries << "\n";
  print_inspect_index_fields(inspect);
  std::cout << "ingest_ms=" << std::chrono::duration<double, std::milli>(t1 - t0).count()
            << " total_close_ms=" << std::chrono::duration<double, std::milli>(t2 - t0).count() << "\n";
  std::cout << "vector_p50_ms=" << percentile(vector_ms, 0.50) << " vector_p95_ms="
            << percentile(vector_ms, 0.95) << "\n";
  std::cout << "causal_lattice_p50_ms=" << percentile(causal_ms, 0.50) << " causal_lattice_p95_ms="
            << percentile(causal_ms, 0.95) << "\n";
  std::cout << "metadata_service_p50_ms=" << percentile(metadata_ms, 0.50)
            << " metadata_service_p95_ms=" << percentile(metadata_ms, 0.95) << "\n";
  std::cout << "causal_root_hit_rate=" << hit_rate
            << " avg_lattice_neighbors=" << (queries > 0 ? lattice_neighbors_sum / queries : 0.0)
            << " avg_lattice_score=" << (queries > 0 ? lattice_score_sum / queries : 0.0) << "\n";
  std::cout << "cross_layer_edges=" << cross_layer_edges << " defect_nodes=" << defect_nodes
            << " defect_edges=" << defect_edges << " synthetic_edges=" << synthetic_edges << "\n";
  std::cout << "reopen_ms=" << reopen_ms << "\n";
  assert(hit_rate >= 0.80);
  return 0;
}
