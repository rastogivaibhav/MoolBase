#include "graphene/db.hpp"
#include "graphene/lattice_placement.hpp"
#include "graphene/hypokosh_runtime.hpp"
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


static QueryMode parse_query_mode(const std::string& value) {
  if (value == "empirical") return QueryMode::Empirical;
  if (value == "theoretical") return QueryMode::Theoretical;
  if (value == "balanced") return QueryMode::Balanced;
  throw std::invalid_argument("unknown reasoning mode: " + value);
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
  if (cmd == "neighbors" || cmd == "search" || cmd == "reason" || cmd == "extract-tsv") return 6;
  if (cmd == "import-tsv" || cmd == "backup") return 5;
  return 4;
}

static bool is_common_value_option(const std::string& flag) {
  return flag == "--wal-rotate-bytes" || flag == "--vector-index";
}

static bool is_common_switch(const std::string& flag) {
  return flag == "--json" || flag == "--no-recover-stale-lock" || flag == "--physical-lattice" || flag == "--physical-lattice-primary";
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
    } else if (flag == "--physical-lattice") {
      opt.physical_lattice_storage = true;
      opt.require_lattice = true;
    } else if (flag == "--physical-lattice-primary") {
      opt.physical_lattice_storage = true;
      opt.physical_lattice_primary = true;
      opt.require_lattice = true;
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
