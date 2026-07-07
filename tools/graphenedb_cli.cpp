#include "graphene/db.hpp"
#include "graphene/lattice_placement.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <map>
#include <vector>

using namespace graphene;
namespace fs = std::filesystem;

static std::vector<float> parse_vec(const std::string& s) {
  std::vector<float> out;
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, ',')) out.push_back(std::stof(item));
  return out;
}

static std::vector<std::string> split_tab(const std::string& s) {
  std::vector<std::string> out;
  std::string item;
  std::stringstream ss(s);
  while (std::getline(ss, item, '\t')) out.push_back(item);
  return out;
}

static void strip_utf8_bom(std::string& s) {
  if (s.size() >= 3 &&
      static_cast<unsigned char>(s[0]) == 0xef &&
      static_cast<unsigned char>(s[1]) == 0xbb &&
      static_cast<unsigned char>(s[2]) == 0xbf) {
    s.erase(0, 3);
  }
}

static LatticeCoord parse_lattice(const std::string& s) {
  std::stringstream ss(s);
  std::string q, r, layer;
  if (!std::getline(ss, q, ',') || !std::getline(ss, r, ',') || !std::getline(ss, layer, ',')) throw std::invalid_argument("bad lattice coordinate");
  return {std::stoi(q), std::stoi(r), std::stoi(layer)};
}

static DefectType parse_defect(const std::string& s) {
  if (s == "vacancy") return DefectType::Vacancy;
  if (s == "substitution") return DefectType::Substitution;
  if (s == "stone-wales") return DefectType::StoneWales;
  if (s == "strain") return DefectType::Strain;
  if (s == "doped") return DefectType::Doped;
  if (s == "boundary") return DefectType::Boundary;
  return DefectType::None;
}

static BondType parse_bond(const std::string& s) {
  if (s == "sigma") return BondType::Sigma;
  if (s == "pi") return BondType::Pi;
  if (s == "vanderwaals") return BondType::VanDerWaals;
  if (s == "defect") return BondType::Defect;
  if (s == "synthetic") return BondType::Synthetic;
  return BondType::None;
}

static LayerCoupling parse_coupling(const std::string& s) {
  if (s == "same-layer") return LayerCoupling::SameLayer;
  if (s == "vanderwaals") return LayerCoupling::VanDerWaals;
  if (s == "bernal") return LayerCoupling::BernalStacked;
  if (s == "twisted") return LayerCoupling::Twisted;
  if (s == "synthetic") return LayerCoupling::Synthetic;
  return LayerCoupling::None;
}

static ExtractionRole parse_extraction_role(const std::string& s) {
  if (s == "root") return ExtractionRole::Root;
  if (s == "symptom") return ExtractionRole::Symptom;
  if (s == "impact") return ExtractionRole::Impact;
  return ExtractionRole::Node;
}

static VectorIndexKind parse_vector_index_kind(const std::string& s) {
  if (s == "auto") return VectorIndexKind::Auto;
  if (s == "flat") return VectorIndexKind::Flat;
  if (s == "kdtree") return VectorIndexKind::KDTree;
  if (s == "faiss") return VectorIndexKind::Faiss;
  throw std::invalid_argument("unknown vector index: " + s);
}

static int common_option_start(const std::string& cmd) {
  if (cmd == "init" || cmd == "inspect" || cmd == "validate" || cmd == "compact") return 4;
  if (cmd == "put-node") return 8;
  if (cmd == "put-edge") return 7;
  if (cmd == "neighbors" || cmd == "search" || cmd == "extract-tsv") return 6;
  if (cmd == "import-tsv" || cmd == "backup") return 5;
  return 4;
}

static bool is_common_value_option(const std::string& flag) {
  return flag == "--wal-rotate-bytes" || flag == "--vector-index";
}

static bool is_common_switch(const std::string& flag) {
  return flag == "--json" || flag == "--no-recover-stale-lock";
}

static void apply_common_options(DBOptions& opt, const std::string& cmd, int argc, char** argv) {
  for (int i = common_option_start(cmd); i < argc; ++i) {
    std::string flag = argv[i];
    if (flag == "--wal-rotate-bytes") {
      if (i + 1 >= argc) throw std::invalid_argument("--wal-rotate-bytes requires a value");
      opt.wal_rotate_bytes = std::stoull(argv[i + 1]);
      ++i;
    } else if (flag == "--vector-index") {
      if (i + 1 >= argc) throw std::invalid_argument("--vector-index requires a value");
      opt.vector_index_kind = parse_vector_index_kind(argv[i + 1]);
      ++i;
    } else if (flag == "--no-recover-stale-lock") {
      opt.recover_stale_lock = false;
    }
  }
}

static bool common_json_enabled(const std::string& cmd, int argc, char** argv) {
  for (int i = common_option_start(cmd); i < argc; ++i) {
    std::string flag = argv[i];
    if (flag == "--json") return true;
    if (is_common_value_option(flag)) ++i;
  }
  return false;
}

static bool skip_common_option(const std::string& flag, int& i) {
  if (is_common_switch(flag)) return true;
  if (is_common_value_option(flag)) {
    ++i;
    return true;
  }
  return false;
}

static std::string json_escape(const std::string& s) {
  std::ostringstream os;
  for (char ch : s) {
    switch (ch) {
      case '\\': os << "\\\\"; break;
      case '"': os << "\\\""; break;
      case '\n': os << "\\n"; break;
      case '\r': os << "\\r"; break;
      case '\t': os << "\\t"; break;
      default:
        if (static_cast<unsigned char>(ch) < 0x20) {
          os << "\\u";
          const char* hex = "0123456789abcdef";
          os << "00" << hex[(ch >> 4) & 0xf] << hex[ch & 0xf];
        } else {
          os << ch;
        }
    }
  }
  return os.str();
}

static std::map<std::string, std::string> parse_key_values(const std::string& text) {
  std::map<std::string, std::string> out;
  std::stringstream ss(text);
  std::string line;
  while (std::getline(ss, line)) {
    auto pos = line.find('=');
    if (pos == std::string::npos) continue;
    out[line.substr(0, pos)] = line.substr(pos + 1);
  }
  return out;
}

static bool looks_integer(const std::string& value) {
  if (value.empty()) return false;
  size_t start = value[0] == '-' ? 1 : 0;
  if (start == value.size()) return false;
  for (size_t i = start; i < value.size(); ++i) {
    if (value[i] < '0' || value[i] > '9') return false;
  }
  return true;
}

static void print_json_object(const std::map<std::string, std::string>& values) {
  std::cout << "{";
  bool first = true;
  for (const auto& kv : values) {
    if (!first) std::cout << ",";
    first = false;
    std::cout << "\"" << json_escape(kv.first) << "\":";
    if (kv.second == "true" || kv.second == "false" || looks_integer(kv.second)) {
      std::cout << kv.second;
    } else {
      std::cout << "\"" << json_escape(kv.second) << "\"";
    }
  }
  std::cout << "}\n";
}

static void usage() {
  std::cout << "graphenedb_cli commands:\n"
            << "  init <path> <dim>\n"
            << "  inspect <path> <dim>\n"
            << "  put-node <path> <dim> <content> <comma-vector> <signature> [root|symptom|impact] [--lattice q,r,layer] [--defect type]\n"
            << "  put-edge <path> <dim> <from> <to> [causal|contradicts|supersedes|supports] [--bond type] [--coupling type] [--strength n] [--defect type]\n"
            << "  neighbors <path> <dim> <node-id> [max-hops]\n"
            << "  extract-tsv <path> <dim> <source-id> <tsv-file>\n"
            << "  import-tsv <path> <dim> <tsv-file> [--base-id N]\n"
            << "  search <path> <dim> <comma-vector> <signature>\n"
            << "  validate <path> <dim>\n"
            << "  backup <path> <dim> <destination-dir> [--verify]\n"
            << "  compact <path> <dim>\n"
            << "common options:\n"
            << "  --wal-rotate-bytes N   checkpoint and truncate WAL after N bytes\n"
            << "  --vector-index KIND    choose auto, flat, kdtree, or faiss\n"
            << "  --no-recover-stale-lock fail instead of removing a dead-owner LOCK file\n"
            << "  --json                 emit machine-readable JSON for supported commands\n";
}

int main(int argc, char** argv) {
  if (argc < 2) { usage(); return 1; }
  std::string cmd = argv[1];
  if (cmd == "init") {
    if (argc < 4) { usage(); return 1; }
    GrapheneDB db;
    DBOptions opt; opt.dimension = static_cast<uint32_t>(std::stoul(argv[3]));
    apply_common_options(opt, cmd, argc, argv);
    auto st = db.open(argv[2], opt);
    if (!st) { std::cerr << st.message << "\n"; return 2; }
    db.close();
    std::cout << "initialized\n";
    return 0;
  }
  if (argc < 4) { usage(); return 1; }
  fs::path path = argv[2];
  DBOptions opt; opt.dimension = static_cast<uint32_t>(std::stoul(argv[3]));
  apply_common_options(opt, cmd, argc, argv);
  bool json = common_json_enabled(cmd, argc, argv);
  GrapheneDB db;
  auto st = db.open(path, opt);
  if (!st) { std::cerr << st.message << "\n"; return 2; }

  if (cmd == "inspect") {
    std::string out;
    st = db.inspect(&out);
    if (!st) { std::cerr << st.message << "\n"; return 2; }
    if (json) print_json_object(parse_key_values(out));
    else std::cout << out;
    return 0;
  }
  if (cmd == "validate") {
    std::string report;
    st = db.validate(&report);
    if (!st) {
      std::cerr << report;
      return 2;
    }
    if (json) {
      print_json_object({{"validation", "ok"}, {"report", report}});
    } else {
      std::cout << report;
      std::cout << "validation=ok\n";
    }
    return 0;
  }
  if (cmd == "put-node") {
    if (argc < 7) { usage(); return 1; }
    NodeInput n;
    n.content = argv[4];
    n.vector = parse_vec(argv[5]);
    n.signature = std::stoull(argv[6]);
    if (argc >= 8) {
      std::string flag = argv[7];
      n.root = flag == "root";
      n.symptom = flag == "symptom";
      n.impact = flag == "impact";
    }
    for (int i = 8; i + 1 < argc; i += 2) {
      std::string flag = argv[i];
      if (skip_common_option(flag, i)) continue;
      if (flag == "--lattice") n.lattice = parse_lattice(argv[i + 1]);
      else if (flag == "--defect") n.defect_type = parse_defect(argv[i + 1]);
    }
    uint32_t id = 0;
    st = db.put_node(n, &id);
    if (!st) { std::cerr << st.message << "\n"; return 2; }
    std::cout << id << "\n";
    return 0;
  }
  if (cmd == "put-edge") {
    if (argc < 6) { usage(); return 1; }
    EdgeInput e;
    e.from = static_cast<uint32_t>(std::stoul(argv[4]));
    e.to = static_cast<uint32_t>(std::stoul(argv[5]));
    if (argc >= 7) {
      std::string role = argv[6];
      if (role == "contradicts") e.role = EdgeRole::Contradicts;
      else if (role == "supersedes") e.role = EdgeRole::Supersedes;
      else if (role == "supports") e.role = EdgeRole::Supports;
      else e.role = EdgeRole::Causal;
    }
    for (int i = 7; i + 1 < argc; i += 2) {
      std::string flag = argv[i];
      if (skip_common_option(flag, i)) continue;
      if (flag == "--bond") e.bond_type = parse_bond(argv[i + 1]);
      else if (flag == "--coupling") e.layer_coupling = parse_coupling(argv[i + 1]);
      else if (flag == "--strength") e.bond_strength = std::stod(argv[i + 1]);
      else if (flag == "--defect") e.defect_type = parse_defect(argv[i + 1]);
    }
    uint32_t id = 0;
    st = db.put_edge(e, &id);
    if (!st) { std::cerr << st.message << "\n"; return 2; }
    std::cout << id << "\n";
    return 0;
  }
  if (cmd == "search") {
    if (argc < 6) { usage(); return 1; }
    auto q = parse_vec(argv[4]);
    auto sig = std::stoull(argv[5]);
    auto b = db.causal_search(q, sig, QueryMode::Empirical);
    if (b.abstain) {
      std::cout << "ABSTAIN " << b.reason << "\n";
      return 0;
    }
    std::cout << "target=" << b.target_node << " confidence=" << b.confidence << " paths=" << b.paths.size() << "\n";
    for (const auto& why : b.why_retrieved) std::cout << "why=" << why << "\n";
    return 0;
  }
  if (cmd == "neighbors") {
    if (argc < 5) { usage(); return 1; }
    uint32_t node = static_cast<uint32_t>(std::stoul(argv[4]));
    uint32_t hops = argc >= 6 ? static_cast<uint32_t>(std::stoul(argv[5])) : 1;
    auto ids = db.lattice_neighbors(node, hops);
    for (uint32_t id : ids) std::cout << id << "\n";
    return 0;
  }
  if (cmd == "extract-tsv") {
    if (argc < 6) { usage(); return 1; }
    std::ifstream in(argv[5]);
    if (!in) { std::cerr << "cannot read TSV: " << argv[5] << "\n"; return 2; }
    ExtractionInput extraction;
    extraction.source_id = argv[4];
    extraction.place_missing_lattice = true;
    extraction.idempotent = true;
    std::string line;
    size_t line_no = 0;
    while (std::getline(in, line)) {
      ++line_no;
      if (line_no == 1) strip_utf8_bom(line);
      if (line.empty() || line[0] == '#') continue;
      auto p = split_tab(line);
      if (p.size() != 6) { std::cerr << "bad extraction TSV field count at line " << line_no << "\n"; return 2; }
      ExtractionNode n;
      n.external_id = p[0];
      n.content = p[1];
      try {
        n.vector = parse_vec(p[2]);
        n.signature = std::stoull(p[3]);
        n.incident = static_cast<uint32_t>(std::stoul(p[4]));
      } catch (const std::exception& e) {
        std::cerr << "bad extraction TSV numeric/vector field at line " << line_no << ": " << e.what() << "\n";
        return 2;
      }
      n.role = parse_extraction_role(p[5]);
      n.metadata["extract_role"] = p[5];
      extraction.nodes.push_back(n);
    }
    for (size_t i = 1; i < extraction.nodes.size(); ++i) {
      ExtractionRelation rel;
      rel.from_external_id = extraction.nodes[i - 1].external_id;
      rel.to_external_id = extraction.nodes[i].external_id;
      rel.role = EdgeRole::Supports;
      rel.bond_type = BondType::Sigma;
      rel.layer_coupling = LayerCoupling::SameLayer;
      rel.confidence = 0.85;
      rel.bond_strength = 0.85;
      extraction.relations.push_back(rel);
    }
    ExtractionResult result;
    st = db.put_extraction(extraction, &result);
    if (!st) { std::cerr << st.message << "\n"; return 2; }
    std::cout << "inserted_nodes=" << result.inserted_node_ids.size() << "\n";
    std::cout << "existing_nodes=" << result.existing_node_ids.size() << "\n";
    std::cout << "inserted_edges=" << result.inserted_edge_ids.size() << "\n";
    for (const auto& kv : result.external_to_node_id) std::cout << "external=" << kv.first << "\tnode=" << kv.second << "\n";
    return 0;
  }
  if (cmd == "import-tsv") {
    if (argc < 5) { usage(); return 1; }
    uint32_t base_id = static_cast<uint32_t>(db.node_count());
    for (int i = 5; i + 1 < argc; i += 2) {
      std::string flag = argv[i];
      if (skip_common_option(flag, i)) continue;
      if (flag == "--base-id") base_id = static_cast<uint32_t>(std::stoul(argv[i + 1]));
    }
    std::ifstream in(argv[4]);
    if (!in) { std::cerr << "cannot read TSV: " << argv[4] << "\n"; return 2; }
    std::vector<NodeInput> nodes;
    std::string line;
    size_t line_no = 0;
    while (std::getline(in, line)) {
      ++line_no;
      if (line_no == 1) strip_utf8_bom(line);
      if (line.empty() || line[0] == '#') continue;
      auto p = split_tab(line);
      if (p.size() != 5) { std::cerr << "bad TSV field count at line " << line_no << "\n"; return 2; }
      NodeInput n;
      n.content = p[0];
      try {
        n.vector = parse_vec(p[1]);
        n.signature = std::stoull(p[2]);
        n.incident = static_cast<uint32_t>(std::stoul(p[3]));
      } catch (const std::exception& e) {
        std::cerr << "bad TSV numeric/vector field at line " << line_no << ": " << e.what() << "\n";
        return 2;
      }
      n.root = p[4] == "root";
      n.symptom = p[4] == "symptom";
      n.impact = p[4] == "impact";
      n.metadata["import_role"] = p[4];
      n.metadata["importer"] = "graphenedb_cli_import_tsv_v1";
      nodes.push_back(n);
    }
    LatticePlacementOptions placement;
    placement.base_node_id = base_id;
    placement.strategy = PlacementStrategy::SemanticGroups;
    LatticePlacementResult placed;
    st = assign_lattice_batch(std::move(nodes), placement, &placed);
    if (!st) { std::cerr << st.message << "\n"; return 2; }
    BatchResult result;
    st = db.put_batch(placed.batch, &result);
    if (!st) { std::cerr << st.message << "\n"; return 2; }
    std::cout << "imported_nodes=" << result.node_ids.size() << "\n";
    std::cout << "imported_edges=" << result.edge_ids.size() << "\n";
    for (uint32_t id : result.node_ids) std::cout << "node=" << id << "\n";
    return 0;
  }
  if (cmd == "compact") {
    st = db.compact();
    if (!st) { std::cerr << st.message << "\n"; return 2; }
    if (json) print_json_object({{"compacted", "true"}});
    else std::cout << "compacted\n";
    return 0;
  }
  if (cmd == "backup") {
    if (argc < 5) { usage(); return 1; }
    fs::path destination = argv[4];
    bool verify = true;
    for (int i = 5; i < argc; ++i) {
      std::string flag = argv[i];
      if (skip_common_option(flag, i)) continue;
      if (flag == "--no-verify") verify = false;
      else if (flag == "--verify") verify = true;
    }
    st = db.backup(destination);
    if (!st) { std::cerr << st.message << "\n"; return 2; }
    if (!json) std::cout << "backup=" << destination.string() << "\n";
    if (verify) {
      GrapheneDB restored;
      st = restored.open(destination, opt);
      if (!st) { std::cerr << "backup verification open failed: " << st.message << "\n"; return 2; }
      std::string report;
      st = restored.validate(&report);
      restored.close();
      if (!st) { std::cerr << "backup verification failed:\n" << report; return 2; }
      if (!json) std::cout << "backup_verified=true\n";
    }
    if (json) print_json_object({{"backup", destination.string()}, {"backup_verified", verify ? "true" : "false"}});
    return 0;
  }
  usage();
  return 1;
}
