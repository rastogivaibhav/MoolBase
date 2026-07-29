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

constexpr uint32_t kMotifSize = 6;

struct QuerySeed {
  std::vector<float> vector;
  uint64_t signature{0};
  std::string source_id;
  std::string root_external_id;
  uint32_t expected_root{UINT32_MAX};
  size_t expected_doc_nodes{0};
};

struct BuiltDocument {
  ExtractionInput input;
  std::vector<QuerySeed> query_seeds;
  std::string service;
  size_t expected_nodes{0};
};

std::vector<float> vec(uint32_t dim, uint32_t family, uint32_t slot) {
  std::vector<float> out(dim);
  const float base = static_cast<float>((family % 500u) + 1u) * 0.002f;
  const float slot_shift = static_cast<float>(slot) * 0.0003f;
  for (uint32_t i = 0; i < dim; ++i) {
    out[i] = base + slot_shift + static_cast<float>((i + 1u) * ((family % 13u) + 3u)) * 0.00007f;
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

const char* service_name(uint32_t doc) {
  static const char* kServices[] = {
      "gateway", "checkout", "catalog", "auth", "fulfillment", "payments"};
  return kServices[doc % (sizeof(kServices) / sizeof(kServices[0]))];
}

const char* source_kind(uint32_t doc) {
  static const char* kKinds[] = {"ticket", "postmortem", "runbook", "chat-log"};
  return kKinds[doc % (sizeof(kKinds) / sizeof(kKinds[0]))];
}

LatticeCoord motif_coord(uint32_t cluster_index, uint32_t slot, int32_t base_layer) {
  static const LatticeCoord kOffsets[kMotifSize] = {
      {0, 0, 0},   // root
      {1, 0, 0},   // dependency
      {1, -1, 0},  // symptom
      {0, 0, 1},   // cross-layer impact
      {-1, 1, 0},  // context
      {0, 1, 0},   // note / defect node
  };
  const int32_t base_q = static_cast<int32_t>((cluster_index % 512u) * 6u);
  const int32_t base_r = static_cast<int32_t>((cluster_index / 512u) * 6u);
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
                  const std::string& evidence_id,
                  const std::string& evidence_text,
                  const std::string& path_kind) {
  ExtractionRelation rel;
  rel.from_external_id = from;
  rel.to_external_id = to;
  rel.origin = bond == BondType::Defect ? EdgeOrigin::Inferred : EdgeOrigin::Observed;
  rel.role = role;
  rel.confidence = confidence;
  rel.evidence_id = evidence_id;
  rel.evidence_uri = "memory://bench/extraction/" + evidence_id;
  rel.evidence_text = evidence_text;
  rel.metadata["path_kind"] = path_kind;
  rel.metadata["workload_profile"] = "rich-extraction";
  rel.bond_type = bond;
  rel.defect_type = defect;
  rel.layer_coupling = coupling;
  rel.bond_strength = strength;
  input->relations.push_back(rel);
}

BuiltDocument build_document(uint32_t doc, uint32_t nodes_per_doc, uint32_t dim) {
  BuiltDocument built;
  built.input.source_id = "doc-" + std::to_string(doc);
  built.input.source_uri = "memory://graphenedb/bench/" + built.input.source_id;
  built.input.extraction_run_id = "rich-bench-run-" + std::to_string(doc % 11u);
  built.input.layer = static_cast<int32_t>(doc % 3u);
  built.input.incident = doc;
  built.input.signature = signature_for(doc % 8u, (doc * 7u) % 16u);
  built.input.place_missing_lattice = false;
  built.input.idempotent = true;
  built.service = service_name(doc);

  const std::string tenant = "tenant-" + std::to_string(doc % 4u);
  const std::string kind = source_kind(doc);
  const std::string external_prefix = "doc-" + std::to_string(doc) + "-";
  uint32_t remaining = nodes_per_doc;
  uint32_t group = 0;
  std::string previous_boundary_id;

  while (remaining > 0) {
    const uint32_t count = std::min<uint32_t>(remaining, kMotifSize);
    const uint32_t cluster_index = doc * 1024u + group;
    const int32_t base_layer = built.input.layer + static_cast<int32_t>((group % 2u) * 2u);
    std::vector<std::string> ids;
    ids.reserve(count);

    for (uint32_t slot = 0; slot < count; ++slot) {
      ExtractionNode node;
      node.external_id = external_prefix + "group-" + std::to_string(group) + "-node-" + std::to_string(slot);
      node.content = built.service + " " + kind + " fragment " + std::to_string(doc) + ":" +
                     std::to_string(group) + ":" + std::to_string(slot);
      node.vector = vec(dim, doc * 131u + group * 17u, slot);
      node.signature = built.input.signature;
      node.incident = doc;
      node.metadata["service"] = built.service;
      node.metadata["tenant"] = tenant;
      node.metadata["source_kind"] = kind;
      node.metadata["cluster"] = std::to_string(group);
      node.metadata["slot"] = std::to_string(slot);
      node.metadata["workload_profile"] = "rich-extraction";
      node.metadata["carbon_zone"] = slot == 3 ? "stacked" : (slot == 5 ? "defect" : "sheet");
      node.lattice = motif_coord(cluster_index, slot, base_layer);
      if (slot == 0) node.role = ExtractionRole::Root;
      if (slot == 2) node.role = ExtractionRole::Symptom;
      if (slot == 3) node.role = ExtractionRole::Impact;
      if (slot == 4 && ((doc + group) % 7u == 0u)) node.defect_type = DefectType::Boundary;
      if (slot == 5 && ((doc + group) % 5u == 0u)) node.defect_type = DefectType::Strain;
      if (slot == 3 && ((doc + group) % 6u == 0u)) node.defect_type = DefectType::Doped;
      ids.push_back(node.external_id);
      built.input.nodes.push_back(std::move(node));
    }

    if (count > 1) {
      add_relation(&built.input, ids[0], ids[1], EdgeRole::Causal, BondType::Sigma, 0.95, 0.95,
                   LayerCoupling::SameLayer, DefectType::None,
                   built.input.source_id + "-sigma-" + std::to_string(group),
                   "same-layer sigma path", "same-layer");
    }
    if (count > 2) {
      add_relation(&built.input, ids[1], ids[2], EdgeRole::Causal, BondType::Pi, 0.92, 0.91,
                   LayerCoupling::SameLayer, DefectType::None,
                   built.input.source_id + "-pi-" + std::to_string(group),
                   "same-layer pi path", "same-layer");
      built.query_seeds.push_back(
          {vec(dim, doc * 131u + group * 17u, 2), built.input.signature, built.input.source_id,
           ids[0], UINT32_MAX, count});
    }
    if (count > 3) {
      const DefectType impact_defect = ((doc + group) % 6u == 0u) ? DefectType::Doped : DefectType::None;
      add_relation(&built.input, ids[0], ids[3], EdgeRole::Supports, BondType::VanDerWaals, 0.84,
                   0.80, group % 2u == 0u ? LayerCoupling::BernalStacked : LayerCoupling::Twisted,
                   impact_defect,
                   built.input.source_id + "-vdw-" + std::to_string(group),
                   "cross-layer propagation path", "cross-layer");
    }
    if (count > 4) {
      add_relation(&built.input, ids[0], ids[4], EdgeRole::Supports, BondType::Sigma, 0.88, 0.86,
                   LayerCoupling::SameLayer, DefectType::None,
                   built.input.source_id + "-context-" + std::to_string(group),
                   "context lattice path", "same-layer");
    }
    if (count > 5) {
      const DefectType note_defect = ((doc + group) % 5u == 0u) ? DefectType::Strain : DefectType::None;
      add_relation(&built.input, ids[4], ids[5], EdgeRole::Supports, BondType::Pi, 0.81, 0.78,
                   LayerCoupling::SameLayer, note_defect,
                   built.input.source_id + "-note-" + std::to_string(group),
                   "supporting note path", "same-layer");
      add_relation(&built.input, ids[5], ids[2], EdgeRole::Supports, BondType::Defect, 0.58, 0.55,
                   LayerCoupling::SameLayer, note_defect,
                   built.input.source_id + "-defect-" + std::to_string(group),
                   "defect shortcut path", "defect");
    }
    if (!previous_boundary_id.empty() && count > 4) {
      add_relation(&built.input, previous_boundary_id, ids[4], EdgeRole::Supports, BondType::Synthetic,
                   0.42, 0.40, LayerCoupling::Synthetic, DefectType::Boundary,
                   built.input.source_id + "-boundary-" + std::to_string(group),
                   "cross-group synthetic bridge", "boundary");
    }
    previous_boundary_id = ids.back();
    built.expected_nodes += count;
    remaining -= count;
    ++group;
  }

  return built;
}

}  // namespace

int main(int argc, char** argv) {
  uint32_t docs = argc > 1 ? static_cast<uint32_t>(std::stoul(argv[1])) : 100;
  uint32_t nodes_per_doc = argc > 2 ? static_cast<uint32_t>(std::stoul(argv[2])) : 50;
  uint32_t queries = argc > 3 ? static_cast<uint32_t>(std::stoul(argv[3])) : 100;
  uint32_t dim = argc > 4 ? static_cast<uint32_t>(std::stoul(argv[4])) : 64;
  std::string vector_index = get_arg(argc, argv, "--vector-index", std::string("auto"));

  fs::path dir = fs::temp_directory_path() / "graphenedb_extraction_ingest_bench";
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

  std::vector<double> doc_ingest_ms;
  std::vector<QuerySeed> query_seeds;
  uint64_t inserted_nodes = 0;
  uint64_t inserted_edges = 0;
  for (uint32_t d = 0; d < docs; ++d) {
    auto built = build_document(d, nodes_per_doc, dim);
    auto t1 = std::chrono::steady_clock::now();
    ExtractionResult result;
    st = db.put_extraction(built.input, &result);
    if (!st) {
      std::cerr << st.message << "\n";
      return 2;
    }
    auto t2 = std::chrono::steady_clock::now();
    doc_ingest_ms.push_back(std::chrono::duration<double, std::milli>(t2 - t1).count());
    inserted_nodes += result.inserted_node_ids.size();
    inserted_edges += result.inserted_edge_ids.size();
    for (auto& seed : built.query_seeds) {
      auto it = result.external_to_node_id.find(seed.root_external_id);
      if (it == result.external_to_node_id.end()) {
        std::cerr << "missing expected root id for extraction query seed\n";
        return 2;
      }
      seed.expected_root = it->second;
      seed.expected_doc_nodes = built.expected_nodes;
      query_seeds.push_back(seed);
    }
  }
  if (query_seeds.empty()) {
    std::cerr << "no extraction query seeds were generated\n";
    return 2;
  }

  std::vector<double> idempotent_ms;
  for (uint32_t d = 0; d < std::min<uint32_t>(docs, 20); ++d) {
    auto built = build_document(d, nodes_per_doc, dim);
    auto t1 = std::chrono::steady_clock::now();
    ExtractionResult result;
    st = db.put_extraction(built.input, &result);
    if (!st) {
      std::cerr << st.message << "\n";
      return 2;
    }
    auto t2 = std::chrono::steady_clock::now();
    if (!result.inserted_node_ids.empty() || !result.inserted_edge_ids.empty()) {
      std::cerr << "idempotent re-import inserted records\n";
      return 2;
    }
    idempotent_ms.push_back(std::chrono::duration<double, std::milli>(t2 - t1).count());
  }

  std::vector<double> vector_ms;
  std::vector<double> causal_ms;
  std::vector<double> metadata_ms;
  double lattice_neighbors_sum = 0.0;
  double lattice_score_sum = 0.0;
  uint32_t causal_root_hits = 0;
  for (uint32_t q = 0; q < queries; ++q) {
    const auto& seed = query_seeds[q % query_seeds.size()];
    auto t1 = std::chrono::steady_clock::now();
    auto vr = db.vector_search(seed.vector, 10);
    auto t2 = std::chrono::steady_clock::now();
    auto bundle = db.causal_search(seed.vector, seed.signature, QueryMode::Empirical);
    auto t3 = std::chrono::steady_clock::now();
    auto ids = db.metadata_search("graphene_source_id", seed.source_id);
    auto t4 = std::chrono::steady_clock::now();
    (void)vr;
    if (ids.size() != seed.expected_doc_nodes) {
      std::cerr << "metadata search returned unexpected document size\n";
      return 2;
    }
    if (!bundle.abstain && bundle.target_node == seed.expected_root) ++causal_root_hits;
    vector_ms.push_back(std::chrono::duration<double, std::milli>(t2 - t1).count());
    causal_ms.push_back(std::chrono::duration<double, std::milli>(t3 - t2).count());
    metadata_ms.push_back(std::chrono::duration<double, std::milli>(t4 - t3).count());
    lattice_neighbors_sum += static_cast<double>(bundle.lattice_neighbors.size());
    lattice_score_sum += bundle.lattice_score;
  }

  auto t_close = std::chrono::steady_clock::now();
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
  auto t_reopen = std::chrono::steady_clock::now();
  std::string report;
  st = reopened.validate(&report);
  if (!st) {
    std::cerr << report << "\n";
    return 2;
  }
  reopened.close();

  double total_nodes = static_cast<double>(docs) * static_cast<double>(nodes_per_doc);
  double ingest_total_ms = 0.0;
  for (double ms : doc_ingest_ms) ingest_total_ms += ms;

  std::cout << "extraction_ingest_bench=true\n";
  std::cout << "docs=" << docs << " nodes_per_doc=" << nodes_per_doc << " nodes=" << inserted_nodes
            << " edges=" << inserted_edges << " dim=" << dim << " queries=" << queries << "\n";
  print_inspect_index_fields(inspect);
  std::cout << "extract_ingest_total_ms=" << ingest_total_ms
            << " extract_nodes_per_sec=" << (total_nodes / (ingest_total_ms / 1000.0)) << "\n";
  std::cout << "extract_doc_p50_ms=" << percentile(doc_ingest_ms, 50)
            << " extract_doc_p95_ms=" << percentile(doc_ingest_ms, 95) << "\n";
  std::cout << "idempotent_doc_p50_ms=" << percentile(idempotent_ms, 50)
            << " idempotent_doc_p95_ms=" << percentile(idempotent_ms, 95) << "\n";
  std::cout << "vector_p50_ms=" << percentile(vector_ms, 50)
            << " vector_p95_ms=" << percentile(vector_ms, 95) << "\n";
  std::cout << "causal_lattice_p50_ms=" << percentile(causal_ms, 50)
            << " causal_lattice_p95_ms=" << percentile(causal_ms, 95) << "\n";
  std::cout << "metadata_doc_p50_ms=" << percentile(metadata_ms, 50)
            << " metadata_doc_p95_ms=" << percentile(metadata_ms, 95) << "\n";
  std::cout << "causal_root_hit_rate="
            << (static_cast<double>(causal_root_hits) / static_cast<double>(std::max<uint32_t>(1, queries)))
            << " avg_lattice_neighbors="
            << (lattice_neighbors_sum / static_cast<double>(std::max<uint32_t>(1, queries)))
            << " avg_lattice_score="
            << (lattice_score_sum / static_cast<double>(std::max<uint32_t>(1, queries))) << "\n";
  std::cout << "reopen_ms=" << std::chrono::duration<double, std::milli>(t_reopen - t_close).count()
            << "\n";
  return 0;
}
