#include "graphene/db.hpp"
#include "graphene/lattice_placement.hpp"
#include "server_runtime.hpp"
#include <arpa/inet.h>
#include <cmath>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <map>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <vector>
#include <iomanip>
#include <netinet/in.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <fstream>
#include <set>
#include <queue>
#include <unordered_set>

using namespace graphene;
namespace fs = std::filesystem;

static constexpr const char* kServerVersion = "0.6.0-rc1";
static constexpr uint32_t kApiVersion = 1;
static constexpr size_t kMaxNodeTextBytes = 1024 * 1024;
static constexpr size_t kMaxBulkPrefixBytes = 4096;
static constexpr uint32_t kMaxLatticeHops = 16;
static constexpr uint32_t kMaxBundleHops = 8;
static constexpr size_t kMaxSearchTopK = 1000;
static constexpr size_t kMaxTemporalResults = 1000;

static volatile std::sig_atomic_t g_stop = 0;
static volatile std::sig_atomic_t g_listen_fd = -1;
static std::mutex g_structured_log_mutex;

static void emit_structured_log(const std::string& line) {
  std::lock_guard<std::mutex> lock(g_structured_log_mutex);
  std::cerr << line << "\n";
}
static void on_sig(int) {
  g_stop = 1;
  // close(2) is async-signal-safe and wakes a blocking accept(2). This makes
  // SIGTERM predictable under systemd/Kubernetes rather than waiting for a
  // new connection to arrive before shutdown can begin.
  const int fd = static_cast<int>(g_listen_fd);
  if (fd >= 0) {
    ::close(fd);
    g_listen_fd = -1;
  }
}

using ServerMetrics = graphenedb::server::RuntimeMetrics;
using ConnectionTask = graphenedb::server::ConnectionTask;

static std::string json_escape(const std::string& s) {
  std::ostringstream os;
  for (unsigned char c : s) {
    switch (c) {
      case '"': os << "\\\""; break;
      case '\\': os << "\\\\"; break;
      case '\b': os << "\\b"; break;
      case '\f': os << "\\f"; break;
      case '\n': os << "\\n"; break;
      case '\r': os << "\\r"; break;
      case '\t': os << "\\t"; break;
      default:
        if (c < 0x20) {
          os << "\\u00" << std::hex << std::setw(2) << std::setfill('0')
             << static_cast<unsigned int>(c) << std::dec << std::setfill(' ');
        } else {
          os << static_cast<char>(c);
        }
    }
  }
  return os.str();
}

static std::string json_value(const std::string& body, const std::string& key) {
  std::string needle = "\"" + key + "\"";
  size_t key_pos = std::string::npos;
  size_t search_from = 0;
  while (true) {
    auto p0 = body.find(needle, search_from);
    if (p0 == std::string::npos) return {};
    size_t p = p0 + needle.size();
    while (p < body.size() && std::isspace(static_cast<unsigned char>(body[p]))) ++p;
    if (p < body.size() && body[p] == ':') { key_pos = p; break; }
    search_from = p0 + 1;
  }
  size_t p = key_pos + 1;
  while (p < body.size() && std::isspace(static_cast<unsigned char>(body[p]))) ++p;
  if (p >= body.size()) return {};
  if (body[p] == '"') {
    ++p; std::string out;
    while (p < body.size() && body[p] != '"') {
      if (body[p] == '\\' && p + 1 < body.size()) { out.push_back(body[p + 1]); p += 2; }
      else out.push_back(body[p++]);
    }
    return out;
  }
  size_t e = p;
  while (e < body.size() && body[e] != ',' && body[e] != '}' && body[e] != ']') ++e;
  std::string out = body.substr(p, e - p);
  while (!out.empty() && std::isspace(static_cast<unsigned char>(out.back()))) out.pop_back();
  return out;
}

static bool parse_vector_index_kind(const std::string& value, VectorIndexKind* out) {
  if (!out) return false;
  if (value == "auto") *out = VectorIndexKind::Auto;
  else if (value == "flat") *out = VectorIndexKind::Flat;
  else if (value == "kdtree") *out = VectorIndexKind::KDTree;
  else if (value == "faiss") *out = VectorIndexKind::Faiss;
  else return false;
  return true;
}


static uint32_t json_u32(const std::string& body, const std::string& key, uint32_t def = 0) {
  try {
    std::string v = json_value(body, key);
    if (v.empty()) return def;
    const auto parsed = std::stoull(v);
    if (parsed > UINT32_MAX) return def;
    return static_cast<uint32_t>(parsed);
  } catch (...) { return def; }
}

static double json_double(const std::string& body, const std::string& key, double def = 0.0) {
  try {
    std::string v = json_value(body, key);
    if (v.empty()) return def;
    return std::stod(v);
  } catch (...) { return def; }
}

static std::string trim_copy(std::string value) {
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.erase(value.begin());
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.pop_back();
  return value;
}

static std::string lower_copy(std::string v);

static std::string header_value(const std::string& headers, const std::string& name) {
  std::string wanted = name;
  std::transform(wanted.begin(), wanted.end(), wanted.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
  size_t pos = 0;
  while (pos < headers.size()) {
    size_t end = headers.find("\r\n", pos);
    if (end == std::string::npos) end = headers.size();
    std::string line = headers.substr(pos, end - pos);
    auto colon = line.find(':');
    if (colon != std::string::npos) {
      std::string key = line.substr(0, colon);
      std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
      if (key == wanted) return trim_copy(line.substr(colon + 1));
    }
    pos = end + 2;
  }
  return {};
}

static std::vector<std::string> header_values(const std::string& headers, const std::string& name) {
  std::vector<std::string> values;
  std::string wanted = lower_copy(name);
  size_t pos = 0;
  while (pos < headers.size()) {
    size_t end = headers.find("\r\n", pos);
    if (end == std::string::npos) end = headers.size();
    const std::string line = headers.substr(pos, end - pos);
    const auto colon = line.find(':');
    if (colon != std::string::npos) {
      std::string key = lower_copy(trim_copy(line.substr(0, colon)));
      if (key == wanted) values.push_back(trim_copy(line.substr(colon + 1)));
    }
    pos = end + 2;
  }
  return values;
}

struct ContentLengthResult {
  bool present{false};
  bool valid{true};
  size_t value{0};
};

static ContentLengthResult parse_content_length(const std::string& headers) {
  ContentLengthResult result;
  const auto values = header_values(headers, "content-length");
  if (values.empty()) return result;
  result.present = true;
  std::optional<size_t> parsed;
  for (const auto& raw : values) {
    if (raw.empty() || !std::all_of(raw.begin(), raw.end(), [](unsigned char c){ return std::isdigit(c); })) {
      result.valid = false;
      return result;
    }
    try {
      const auto value = static_cast<size_t>(std::stoull(raw));
      if (parsed && *parsed != value) {
        result.valid = false;
        return result;
      }
      parsed = value;
    } catch (...) {
      result.valid = false;
      return result;
    }
  }
  result.value = parsed.value_or(0);
  return result;
}

static bool endpoint_requires_json_body(const std::string& method, const std::string& path) {
  if (method != "POST") return false;
  return path == "/v1/nodes" || path == "/v1/nodes/bulk" || path == "/v1/facts" ||
         path == "/v1/edges" || path == "/v1/edges/provenance" ||
         path == "/v1/retrieve/bundle" || path == "/v1/retrieve/explain" ||
         path == "/v1/retrieve/temporal" || path == "/v1/search/hybrid" ||
         path == "/v1/admin/backup";
}

static bool application_json_content_type(const std::string& headers) {
  std::string value = lower_copy(header_value(headers, "content-type"));
  const auto semicolon = value.find(';');
  if (semicolon != std::string::npos) value.resize(semicolon);
  return trim_copy(value) == "application/json";
}

static bool constant_time_equal(const std::string& a, const std::string& b) {
  const size_t n = std::max(a.size(), b.size());
  unsigned char diff = static_cast<unsigned char>(a.size() ^ b.size());
  for (size_t i = 0; i < n; ++i) {
    const unsigned char av = i < a.size() ? static_cast<unsigned char>(a[i]) : 0;
    const unsigned char bv = i < b.size() ? static_cast<unsigned char>(b[i]) : 0;
    diff = static_cast<unsigned char>(diff | (av ^ bv));
  }
  return diff == 0;
}

static bool authorized_request(const std::string& headers, const std::string& api_key) {
  if (api_key.empty()) return true;
  const std::string direct = header_value(headers, "x-api-key");
  if (!direct.empty() && constant_time_equal(direct, api_key)) return true;
  const std::string authorization = header_value(headers, "authorization");
  constexpr const char* prefix = "Bearer ";
  if (authorization.rfind(prefix, 0) == 0) {
    return constant_time_equal(authorization.substr(std::strlen(prefix)), api_key);
  }
  return false;
}

static std::string first_forwarded_ip(const std::string& headers) {
  std::string value = header_value(headers, "x-forwarded-for");
  auto comma = value.find(',');
  if (comma != std::string::npos) value.resize(comma);
  return trim_copy(value);
}

static uint32_t query_u32(const std::string& path, const std::string& key, uint32_t def = 0) {
  auto p = path.find(key + "=");
  if (p == std::string::npos) return def;
  p += key.size() + 1;
  size_t e = p;
  while (e < path.size() && std::isdigit(static_cast<unsigned char>(path[e]))) ++e;
  try {
    const auto parsed = std::stoull(path.substr(p, e - p));
    if (parsed > UINT32_MAX) return def;
    return static_cast<uint32_t>(parsed);
  } catch (...) { return def; }
}

static int http_status_for(const Status& status) {
  switch (status.code) {
    case ErrorCode::Ok: return 200;
    case ErrorCode::InvalidOption:
    case ErrorCode::DimensionMismatch:
    case ErrorCode::EdgeInvalid:
    case ErrorCode::InvalidInput: return 400;
    case ErrorCode::NodeNotFound: return 404;
    case ErrorCode::LockBusy: return 409;
    case ErrorCode::NotOpen: return 503;
    case ErrorCode::UnsupportedMode: return 501;
    case ErrorCode::IoError:
    case ErrorCode::WalCorrupt:
    case ErrorCode::DataCorrupt:
    case ErrorCode::TransactionIncomplete: return 500;
  }
  return 500;
}

static std::vector<float> embed_text(const std::string& text, uint32_t dim) {
  std::vector<float> v(dim, 0.0f);
  uint64_t h = 1469598103934665603ull;
  for (unsigned char c : text) {
    h ^= c; h *= 1099511628211ull;
    v[h % dim] += ((h >> 8) & 1) ? 1.0f : -1.0f;
  }
  float norm = 0.0f;
  for (float x : v) norm += x * x;
  norm = std::sqrt(norm);
  if (norm > 0) for (float& x : v) x /= norm;
  return v;
}

static LatticeCoord spiral_coord(uint32_t id) {
  return graphene::hex_spiral_coord(id, 0);
}

static double cosine_vec(const std::vector<float>& a, const std::vector<float>& b) {
  if (a.empty() || b.empty() || a.size() != b.size()) return 0.0;
  double dot = 0.0, na = 0.0, nb = 0.0;
  for (size_t i = 0; i < a.size(); ++i) { dot += a[i] * b[i]; na += a[i] * a[i]; nb += b[i] * b[i]; }
  if (na <= 0.0 || nb <= 0.0) return 0.0;
  return dot / (std::sqrt(na) * std::sqrt(nb));
}

static int hex_distance(const LatticeCoord& a, const LatticeCoord& b) {
  if (a.layer != b.layer) return 1000000 + std::abs(a.layer - b.layer);
  int32_t as = -a.q - a.r;
  int32_t bs = -b.q - b.r;
  return std::max({std::abs(a.q - b.q), std::abs(a.r - b.r), std::abs(as - bs)});
}

static std::string coord_key(const LatticeCoord& c) {
  return std::to_string(c.q) + ":" + std::to_string(c.r) + ":" + std::to_string(c.layer);
}

static std::vector<LatticeCoord> coord_ring_candidates(const LatticeCoord& c, int radius) {
  std::vector<LatticeCoord> out;
  out.push_back(c);
  for (int q = -radius; q <= radius; ++q) {
    for (int r = -radius; r <= radius; ++r) {
      LatticeCoord x{c.q + q, c.r + r, c.layer};
      if (hex_distance(c, x) <= radius) out.push_back(x);
    }
  }
  return out;
}

static LatticeCoord choose_semantic_causal_temporal_cell(GrapheneDB& db,
                                                        const std::string& content,
                                                        const std::vector<float>& vector,
                                                        const std::map<std::string,std::string>& metadata,
                                                        std::atomic<uint32_t>& next_lattice_slot) {
  (void)content;
  std::unordered_set<uint32_t> candidate_ids;
  for (const auto& hit : db.vector_search(vector, 32)) candidate_ids.insert(hit.node_id);
  for (const auto& key : {"account", "service", "incident", "repo", "source"}) {
    auto it = metadata.find(key);
    if (it == metadata.end()) continue;
    auto matches = db.metadata_search(key, it->second);
    for (size_t i = 0; i < matches.size() && i < 16; ++i) candidate_ids.insert(matches[i]);
  }

  std::unordered_set<std::string> occupied_near_anchors;
  struct Anchor { uint32_t id; LatticeCoord coord; double semantic; double meta; double temporal; };
  std::vector<Anchor> anchors;
  anchors.reserve(candidate_ids.size());
  for (uint32_t id : candidate_ids) {
    auto n = db.get_node(id);
    if (!n || !n->lattice) continue;
    occupied_near_anchors.insert(coord_key(*n->lattice));
    for (uint32_t neighbor : db.lattice_neighbors(id, 3)) {
      auto nn = db.get_node(neighbor);
      if (nn && nn->lattice) occupied_near_anchors.insert(coord_key(*nn->lattice));
    }
    const double sem = cosine_vec(vector, n->vector);
    double meta = 0.0;
    size_t comparable = 0;
    for (const auto& kv : metadata) {
      if (kv.first == "placement_policy" || kv.first == "valid_from" || kv.first == "valid_until") continue;
      auto found = n->metadata.find(kv.first);
      if (found != n->metadata.end()) {
        ++comparable;
        if (found->second == kv.second) meta += 1.0;
      }
    }
    if (comparable) meta /= static_cast<double>(comparable);
    double temporal = 0.0;
    auto vf = metadata.find("valid_from"); auto vu = metadata.find("valid_until");
    auto of = n->metadata.find("valid_from"); auto ou = n->metadata.find("valid_until");
    if (vf != metadata.end() && of != n->metadata.end() && vf->second.size() >= 7 && of->second.size() >= 7)
      temporal += vf->second.substr(0,7) == of->second.substr(0,7) ? 0.6 : 0.0;
    if (vu != metadata.end() && ou != n->metadata.end() && vu->second.size() >= 7 && ou->second.size() >= 7)
      temporal += vu->second.substr(0,7) == ou->second.substr(0,7) ? 0.4 : 0.0;
    const double anchor_score = 0.65 * sem + 0.25 * meta + 0.10 * temporal;
    if (anchor_score > 0.05) anchors.push_back({id, *n->lattice, sem, meta, temporal});
  }
  std::sort(anchors.begin(), anchors.end(), [](const Anchor& a, const Anchor& b){
    return (0.65*a.semantic + 0.25*a.meta + 0.10*a.temporal) > (0.65*b.semantic + 0.25*b.meta + 0.10*b.temporal);
  });
  if (anchors.size() > 24) anchors.resize(24);

  struct Candidate { LatticeCoord coord; double score; };
  Candidate best{{0,0,0}, -1e9};
  for (const auto& a : anchors) {
    for (const auto& c : coord_ring_candidates(a.coord, 3)) {
      if (occupied_near_anchors.count(coord_key(c))) continue;
      double local_score = 0.0;
      int local_count = 0;
      for (const auto& b : anchors) {
        const int d = hex_distance(c, b.coord);
        if (d > 4) continue;
        const double decay = 1.0 / (1.0 + d);
        local_score += decay * (0.45 * b.semantic + 0.25 * b.meta + 0.15 * b.temporal);
        ++local_count;
      }
      const double overcrowding = std::max(0, local_count - 6) * 0.025;
      const double score = local_score - overcrowding;
      if (score > best.score) best = {c, score};
    }
  }
  if (best.score > 0.02) return best.coord;
  return spiral_coord(next_lattice_slot.fetch_add(1, std::memory_order_relaxed));
}


static Status put_node_with_lattice_retry(GrapheneDB& db, NodeInput& ni, uint32_t* out_id, std::atomic<uint32_t>& next_lattice_slot) {
  // Semantic-causal placement is computed from a snapshot of occupied cells. Under concurrent HTTP writes,
  // two requests can choose the same best empty cell. Keep the mathematical placement attempt, but preserve
  // storage correctness by retrying with deterministic frontier cells on coordinate collision.
  Status st = db.put_node(ni, out_id);
  if (st) return st;
  const std::string first_error = st.message;
  for (uint32_t tries = 0; tries < 1024; ++tries) {
    ni.lattice = spiral_coord(next_lattice_slot.fetch_add(1, std::memory_order_relaxed));
    st = db.put_node(ni, out_id);
    if (st) return st;
    if (st.message.find("coordinate") == std::string::npos && st.message.find("lattice") == std::string::npos && st.message.find("duplicate") == std::string::npos) {
      return st;
    }
  }
  return Status::error(ErrorCode::InvalidInput, "lattice placement retry exhausted after initial error: " + first_error);
}

static std::string lattice_quality_json(GrapheneDB& db) {
  const size_t total = db.node_count();
  size_t checked = 0, neighbor_pairs = 0;
  double sem_sum = 0.0, causal_local = 0.0, causal_total = 0.0, temporal_pairs = 0.0, temporal_aligned = 0.0;
  for (uint32_t id = 0; id < total + 1024; ++id) {
    auto n = db.get_node(id);
    if (!n) { if (id > total + 16) break; continue; }
    ++checked;
    auto ns = db.lattice_neighbors(id, 1);
    for (uint32_t nid : ns) {
      if (nid <= id) continue;
      auto m = db.get_node(nid); if (!m) continue;
      sem_sum += cosine_vec(n->vector, m->vector);
      neighbor_pairs++;
      auto nf = n->metadata.find("valid_from"); auto mf = m->metadata.find("valid_from");
      if (nf != n->metadata.end() && mf != m->metadata.end()) { temporal_pairs += 1.0; if (nf->second.substr(0,7) == mf->second.substr(0,7)) temporal_aligned += 1.0; }
    }
  }
  const size_t edge_total = db.edge_count();
  for (uint32_t eid = 0; eid < edge_total + 1024; ++eid) {
    auto e = db.get_edge(eid);
    if (!e) { if (eid > edge_total + 16) break; continue; }
    auto a = db.get_node(e->from); auto b = db.get_node(e->to);
    if (!a || !b || !a->lattice || !b->lattice) continue;
    causal_total += 1.0;
    if (hex_distance(*a->lattice, *b->lattice) <= 2) causal_local += 1.0;
  }
  double local_coherence = neighbor_pairs ? sem_sum / neighbor_pairs : 0.0;
  double causal_locality = causal_total ? causal_local / causal_total : 0.0;
  double temporal_consistency = temporal_pairs ? temporal_aligned / temporal_pairs : 0.0;
  std::ostringstream js;
  js << "{\"node_count\":" << checked
     << ",\"neighbor_pairs\":" << neighbor_pairs
     << ",\"local_coherence\":" << local_coherence
     << ",\"causal_locality\":" << causal_locality
     << ",\"temporal_consistency\":" << temporal_consistency
     << ",\"mathematical_claim\":\"hex distance is optimized as a proxy for semantic-causal-temporal locality\"}";
  return js.str();
}

static std::string lower_copy(std::string v) {
  std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
  return v;
}

static EdgeOrigin parse_origin(const std::string& raw) {
  std::string v = lower_copy(raw);
  if (v == "discovered") return EdgeOrigin::Discovered;
  if (v == "inferred") return EdgeOrigin::Inferred;
  if (v == "reinforced") return EdgeOrigin::Reinforced;
  if (v == "hypothetical") return EdgeOrigin::Hypothetical;
  return EdgeOrigin::Observed;
}

static EdgeRole parse_role(const std::string& raw) {
  std::string v = lower_copy(raw);
  if (v == "mechanistic") return EdgeRole::Mechanistic;
  if (v == "compressed") return EdgeRole::Compressed;
  if (v == "analogical") return EdgeRole::Analogical;
  if (v == "predictive") return EdgeRole::Predictive;
  if (v == "contradicts" || v == "contradiction") return EdgeRole::Contradicts;
  if (v == "supports" || v == "support") return EdgeRole::Supports;
  if (v == "supersedes" || v == "supersession") return EdgeRole::Supersedes;
  return EdgeRole::Causal;
}

static std::string origin_name(EdgeOrigin o) {
  switch (o) {
    case EdgeOrigin::Observed: return "observed";
    case EdgeOrigin::Discovered: return "discovered";
    case EdgeOrigin::Inferred: return "inferred";
    case EdgeOrigin::Reinforced: return "reinforced";
    case EdgeOrigin::Hypothetical: return "hypothetical";
  }
  return "observed";
}

static std::string role_name(EdgeRole r) {
  switch (r) {
    case EdgeRole::Mechanistic: return "mechanistic";
    case EdgeRole::Compressed: return "compressed";
    case EdgeRole::Analogical: return "analogical";
    case EdgeRole::Predictive: return "predictive";
    case EdgeRole::Causal: return "causal";
    case EdgeRole::Contradicts: return "contradicts";
    case EdgeRole::Supports: return "supports";
    case EdgeRole::Supersedes: return "supersedes";
  }
  return "causal";
}

static bool edge_allowed(EdgeOrigin o, const std::string& mode) {
  std::string m = lower_copy(mode);
  if (m == "empirical") return o == EdgeOrigin::Observed || o == EdgeOrigin::Discovered;
  if (m == "exploratory" || m == "theoretical") return true;
  // balanced: allow inferred/reinforced with labels, reject hypothetical by default.
  return o != EdgeOrigin::Hypothetical;
}

static std::string metadata_json(const std::map<std::string, std::string>& md) {
  std::ostringstream js; js << "{";
  bool first = true;
  for (const auto& kv : md) {
    if (!first) js << ',';
    first = false;
    js << "\"" << json_escape(kv.first) << "\":\"" << json_escape(kv.second) << "\"";
  }
  js << "}";
  return js.str();
}


static bool node_valid_at(const Node& n, const std::string& as_of) {
  if (as_of.empty()) return true;
  auto vf = n.metadata.find("valid_from");
  auto vu = n.metadata.find("valid_until");
  if (vf != n.metadata.end() && !vf->second.empty() && as_of < vf->second) return false;
  if (vu != n.metadata.end() && !vu->second.empty() && as_of > vu->second) return false;
  return true;
}

static bool edge_valid_at(GrapheneDB& db, const Edge& e, const std::string& as_of) {
  if (as_of.empty()) return true;
  auto from = db.get_node(e.from);
  auto to = db.get_node(e.to);
  if (from && !node_valid_at(*from, as_of)) return false;
  if (to && !node_valid_at(*to, as_of)) return false;
  auto vf = e.metadata.find("valid_from");
  auto vu = e.metadata.find("valid_until");
  if (vf != e.metadata.end() && !vf->second.empty() && as_of < vf->second) return false;
  if (vu != e.metadata.end() && !vu->second.empty() && as_of > vu->second) return false;
  return true;
}

static std::string build_bundle_json(GrapheneDB& db, uint32_t anchor, uint32_t max_hops, const std::string& mode, const std::string& as_of = "") {
  struct State { uint32_t node; std::vector<uint32_t> nodes; std::vector<uint32_t> edges; double confidence; bool contradiction; bool hypothetical; };
  std::queue<State> q;
  std::unordered_map<uint32_t, std::vector<State>> by_target;
  std::set<std::pair<uint32_t,uint32_t>> seen_at_depth;
  auto anchor_node = db.get_node(anchor);
  if (!anchor_node || !node_valid_at(*anchor_node, as_of)) {
    return "{\"anchor\":" + std::to_string(anchor) + ",\"mode\":\"" + json_escape(mode.empty()?"balanced":mode) + "\",\"as_of\":\"" + json_escape(as_of) + "\",\"targets\":[],\"warnings\":[\"anchor_not_found_or_not_temporally_valid\"]}";
  }
  q.push({anchor, {anchor}, {}, 1.0, false, false});
  seen_at_depth.insert({anchor, 0});
  const size_t edge_total = db.edge_count();
  while (!q.empty()) {
    State cur = q.front(); q.pop();
    uint32_t depth = static_cast<uint32_t>(cur.edges.size());
    if (depth >= max_hops) continue;
    for (uint32_t eid = 0; eid < edge_total + 1000; ++eid) {
      auto e = db.get_edge(eid);
      if (!e) {
        if (eid > edge_total + 16) break;
        continue;
      }
      if (e->from != cur.node) continue;
      if (!edge_allowed(e->origin, mode)) continue;
      if (!edge_valid_at(db, *e, as_of)) continue;
      uint32_t next = e->to;
      if (std::find(cur.nodes.begin(), cur.nodes.end(), next) != cur.nodes.end()) continue;
      auto next_node = db.get_node(next);
      if (next_node && !node_valid_at(*next_node, as_of)) continue;
      State ns = cur;
      ns.node = next;
      ns.nodes.push_back(next);
      ns.edges.push_back(eid);
      ns.confidence *= e->confidence;
      ns.contradiction = ns.contradiction || e->role == EdgeRole::Contradicts;
      ns.hypothetical = ns.hypothetical || e->origin == EdgeOrigin::Hypothetical;
      by_target[next].push_back(ns);
      if (seen_at_depth.insert({next, depth + 1}).second) q.push(ns);
    }
  }

  std::ostringstream js;
  js << "{\"anchor\":" << anchor << ",\"mode\":\"" << json_escape(mode.empty()?"balanced":mode) << "\",\"as_of\":\"" << json_escape(as_of) << "\",\"max_hops\":" << max_hops << ",\"targets\":[";
  bool first_target = true;
  for (auto& kv : by_target) {
    if (!first_target) js << ',';
    first_target = false;
    bool has_contra = false; double best = 0.0;
    for (const auto& st : kv.second) { has_contra = has_contra || st.contradiction; best = std::max(best, st.confidence); }
    std::vector<Edge> attached_contradictions;
    std::set<uint32_t> bundle_nodes;
    for (const auto& st : kv.second) for (uint32_t n : st.nodes) bundle_nodes.insert(n);
    std::set<uint32_t> attached_ids;
    for (uint32_t eid = 0; eid < edge_total + 1000; ++eid) {
      auto e = db.get_edge(eid);
      if (!e) { if (eid > edge_total + 16) break; continue; }
      if (!edge_valid_at(db, *e, as_of)) continue;
      if (e->role == EdgeRole::Contradicts && (bundle_nodes.count(e->from) || bundle_nodes.count(e->to))) {
        if (attached_ids.insert(e->id).second) attached_contradictions.push_back(*e);
        has_contra = true;
      }
    }
    js << "{\"target\":" << kv.first << ",\"degeneracy\":" << kv.second.size() << ",\"best_confidence\":" << best << ",\"has_contradiction\":" << (has_contra?"true":"false") << ",\"attached_contradictions\":[";
    for (size_t ci=0; ci<attached_contradictions.size(); ++ci) {
      const auto& ce = attached_contradictions[ci];
      if (ci) js << ',';
      js << "{\"edge\":" << ce.id << ",\"from\":" << ce.from << ",\"to\":" << ce.to << ",\"origin\":\"" << origin_name(ce.origin) << "\",\"role\":\"" << role_name(ce.role) << "\",\"confidence\":" << ce.confidence << "}";
    }
    js << "],\"paths\":[";
    bool first_path = true;
    for (const auto& st : kv.second) {
      if (!first_path) js << ',';
      first_path = false;
      js << "{\"nodes\":[";
      for (size_t i=0;i<st.nodes.size();++i){ if(i) js << ','; js << st.nodes[i]; }
      js << "],\"edges\":[";
      for (size_t i=0;i<st.edges.size();++i){ if(i) js << ','; js << st.edges[i]; }
      js << "],\"roles\":[";
      for (size_t i=0;i<st.edges.size();++i){ auto e=db.get_edge(st.edges[i]); if(i) js << ','; js << "\"" << (e?role_name(e->role):"") << "\""; }
      js << "],\"origins\":[";
      for (size_t i=0;i<st.edges.size();++i){ auto e=db.get_edge(st.edges[i]); if(i) js << ','; js << "\"" << (e?origin_name(e->origin):"") << "\""; }
      js << "],\"confidence\":" << st.confidence << ",\"temporal_consistent\":" << (as_of.empty()?"true":"true") << ",\"contains_contradiction\":" << (st.contradiction?"true":"false") << ",\"contains_hypothetical\":" << (st.hypothetical?"true":"false") << "}";
    }
    js << "]}";
  }
  js << "]}";
  return js.str();
}

static uint64_t physical_capacity(uint32_t radius, uint32_t layers = 1) {
  const uint64_t per_layer = 1ull + 3ull * radius * (static_cast<uint64_t>(radius) + 1ull);
  return per_layer * std::max<uint32_t>(1, layers);
}

static uint32_t minimum_radius_for_nodes(uint64_t nodes) {
  if (nodes <= 1) return 0;
  const double root = (-3.0 + std::sqrt(std::max(0.0, 12.0 * static_cast<double>(nodes) - 3.0))) / 6.0;
  return static_cast<uint32_t>(std::ceil(root));
}

static bool loopback_address(const std::string& address) {
  return address == "127.0.0.1" || address == "::1" || address == "localhost";
}

static std::string status_text(int code) {
  switch (code) {
    case 200: return "OK";
    case 201: return "Created";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 408: return "Request Timeout";
    case 409: return "Conflict";
    case 411: return "Length Required";
    case 413: return "Payload Too Large";
    case 415: return "Unsupported Media Type";
    case 429: return "Too Many Requests";
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    case 503: return "Service Unavailable";
    case 507: return "Insufficient Storage";
    default: return "Error";
  }
}

static std::string response(int code, const std::string& body,
                            const std::string& request_id = {},
                            const std::string& content_type = "application/json",
                            int retry_after_seconds = 0) {
  std::ostringstream os;
  os << "HTTP/1.1 " << code << ' ' << status_text(code)
     << "\r\nContent-Type: " << content_type
     << "\r\nContent-Length: " << body.size()
     << "\r\nConnection: close"
     << "\r\nCache-Control: no-store"
     << "\r\nX-Content-Type-Options: nosniff"
     << "\r\nX-GrapheneDB-Version: " << kServerVersion
     << "\r\nX-GrapheneDB-API-Version: " << kApiVersion;
  if (!request_id.empty()) os << "\r\nX-Request-ID: " << request_id;
  if (retry_after_seconds > 0) os << "\r\nRetry-After: " << retry_after_seconds;
  os << "\r\n\r\n" << body;
  return os.str();
}

static bool write_all(int fd, const std::string& payload) {
  size_t sent = 0;
  while (sent < payload.size()) {
    const ssize_t n = ::write(fd, payload.data() + sent, payload.size() - sent);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) return false;
    sent += static_cast<size_t>(n);
  }
  return true;
}

static void structured_request_log(const std::string& request_id,
                                   const std::string& client_ip,
                                   const std::string& method,
                                   const std::string& path,
                                   int status,
                                   uint64_t duration_us,
                                   size_t bytes_in,
                                   size_t bytes_out,
                                   bool rate_limited) {
  std::ostringstream log;
  log << "{\"timestamp\":\"" << graphenedb::server::utc_timestamp()
      << "\",\"level\":\"" << (status >= 500 ? "error" : status >= 400 ? "warn" : "info")
      << "\",\"event\":\"http_request\",\"request_id\":\"" << json_escape(request_id)
      << "\",\"client_ip\":\"" << json_escape(client_ip)
      << "\",\"method\":\"" << json_escape(method)
      << "\",\"path\":\"" << json_escape(path)
      << "\",\"status\":" << status
      << ",\"duration_us\":" << duration_us
      << ",\"bytes_in\":" << bytes_in
      << ",\"bytes_out\":" << bytes_out
      << ",\"rate_limited\":" << (rate_limited ? "true" : "false") << "}";
  emit_structured_log(log.str());
}


int main(int argc, char** argv) {
  if (argc < 4) {
    std::cerr << "usage: graphenedb_server <db_dir> <dimension> <port> [options]\n"
              << "  --bind-address IP\n"
              << "  --physical-lattice-primary --physical-lattice-radius N\n"
              << "  --expected-max-nodes N --wal-rotate-bytes N\n"
              << "  --workers N --queue-capacity N\n"
              << "  --max-bulk-nodes N\n"
              << "  --rate-limit-rps N --rate-limit-burst N\n"
              << "  --max-request-bytes N --socket-timeout-seconds N\n"
              << "  --vector-index auto|flat|kdtree|faiss\n"
              << "  --api-key KEY --trust-proxy --behind-tls-proxy\n"
              << "  --require-forwarded-https --allow-insecure-public-bind\n"
              << "  --no-shutdown-checkpoint\n";
    return 2;
  }
  std::signal(SIGINT, on_sig); std::signal(SIGTERM, on_sig);
#ifdef SIGPIPE
  // A client may disconnect before reading the response. Ignore SIGPIPE so one
  // reset connection cannot terminate the entire database server.
  std::signal(SIGPIPE, SIG_IGN);
#endif
  const fs::path dir = argv[1];
  const uint32_t dim = static_cast<uint32_t>(std::stoul(argv[2]));
  const int port = std::stoi(argv[3]);
  DBOptions opt; opt.dimension = dim; opt.create_if_missing = true; opt.vector_index_kind = VectorIndexKind::Auto;
  std::string api_key;
  if (const char* env_key = std::getenv("GRAPHENEDB_API_KEY")) api_key = env_key;
  std::string bind_address = "127.0.0.1";
  size_t worker_count = std::clamp<size_t>(std::thread::hardware_concurrency() ? std::thread::hardware_concurrency() : 4, 4, 32);
  size_t queue_capacity = 1024;
  size_t max_bulk_nodes = 10000;
  double rate_limit_rps = 200.0;
  double rate_limit_burst = 400.0;
  size_t max_request_bytes = 4 * 1024 * 1024;
  int socket_timeout_seconds = 30;
  uint64_t expected_max_nodes = 0;
  bool trust_proxy = false;
  bool behind_tls_proxy = false;
  bool require_forwarded_https = false;
  bool allow_insecure_public_bind = false;
  bool shutdown_checkpoint = true;
  for (int i = 4; i < argc; ++i) {
    std::string f = argv[i];
    if (f == "--bind-address" && i + 1 < argc) bind_address = argv[++i];
    else if (f == "--physical-lattice-primary") { opt.require_lattice = true; opt.physical_lattice_storage = true; opt.physical_lattice_primary = true; }
    else if (f == "--physical-lattice-radius" && i + 1 < argc) opt.physical_lattice_radius = static_cast<uint32_t>(std::stoul(argv[++i]));
    else if (f == "--expected-max-nodes" && i + 1 < argc) expected_max_nodes = std::stoull(argv[++i]);
    else if (f == "--wal-rotate-bytes" && i + 1 < argc) opt.wal_rotate_bytes = std::stoull(argv[++i]);
    else if (f == "--vector-index" && i + 1 < argc) {
      if (!parse_vector_index_kind(argv[++i], &opt.vector_index_kind)) {
        std::cerr << "invalid --vector-index; expected auto|flat|kdtree|faiss\n";
        return 2;
      }
    }
    else if (f == "--api-key" && i + 1 < argc) api_key = argv[++i];
    else if (f == "--workers" && i + 1 < argc) worker_count = std::max<size_t>(1, std::stoull(argv[++i]));
    else if (f == "--queue-capacity" && i + 1 < argc) queue_capacity = std::max<size_t>(1, std::stoull(argv[++i]));
    else if (f == "--max-bulk-nodes" && i + 1 < argc) max_bulk_nodes = std::clamp<size_t>(std::stoull(argv[++i]), 1, 50000);
    else if (f == "--rate-limit-rps" && i + 1 < argc) rate_limit_rps = std::stod(argv[++i]);
    else if (f == "--rate-limit-burst" && i + 1 < argc) rate_limit_burst = std::stod(argv[++i]);
    else if (f == "--max-request-bytes" && i + 1 < argc) max_request_bytes = std::max<size_t>(1024, std::stoull(argv[++i]));
    else if (f == "--socket-timeout-seconds" && i + 1 < argc) socket_timeout_seconds = std::max(1, std::stoi(argv[++i]));
    else if (f == "--trust-proxy") trust_proxy = true;
    else if (f == "--behind-tls-proxy") behind_tls_proxy = true;
    else if (f == "--require-forwarded-https") { behind_tls_proxy = true; require_forwarded_https = true; }
    else if (f == "--allow-insecure-public-bind") allow_insecure_public_bind = true;
    else if (f == "--no-shutdown-checkpoint") shutdown_checkpoint = false;
    else { std::cerr << "unknown or incomplete option: " << f << "\n"; return 2; }
  }
  if (!loopback_address(bind_address) && !allow_insecure_public_bind) {
    if (api_key.empty()) {
      std::cerr << "refusing non-loopback bind without GRAPHENEDB_API_KEY/--api-key\n";
      return 2;
    }
    if (!behind_tls_proxy) {
      std::cerr << "refusing non-loopback bind without --behind-tls-proxy; use a TLS reverse proxy or explicitly pass --allow-insecure-public-bind\n";
      return 2;
    }
  }
  if (trust_proxy && !behind_tls_proxy) {
    std::cerr << "--trust-proxy requires --behind-tls-proxy\n";
    return 2;
  }
  if (opt.physical_lattice_primary) {
    const uint64_t capacity = physical_capacity(opt.physical_lattice_radius);
    if (expected_max_nodes > capacity) {
      std::cerr << "physical lattice radius " << opt.physical_lattice_radius << " has capacity " << capacity
                << ", below --expected-max-nodes " << expected_max_nodes
                << "; minimum radius is " << minimum_radius_for_nodes(expected_max_nodes) << "\n";
      return 2;
    }
  }
  GrapheneDB db;
  auto st = db.open(dir, opt);
  if (!st) { std::cerr << st.message << "\n"; return 1; }

  int server_fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) { perror("socket"); return 1; }
  int yes = 1; setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_port = htons(static_cast<uint16_t>(port));
  if (inet_pton(AF_INET, bind_address.c_str(), &addr.sin_addr) != 1) {
    std::cerr << "invalid --bind-address: " << bind_address << "\n"; return 2;
  }
  if (bind(server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) { perror("bind"); return 1; }
  const int listen_backlog = static_cast<int>(std::min<size_t>(queue_capacity, 4096));
  if (listen(server_fd, std::max(16, listen_backlog)) < 0) { perror("listen"); return 1; }
  g_listen_fd = server_fd;
  const uint64_t lattice_capacity = opt.physical_lattice_primary ? physical_capacity(opt.physical_lattice_radius) : 0;
  {
    std::ostringstream log;
    log << "{\"timestamp\":\"" << graphenedb::server::utc_timestamp()
        << "\",\"level\":\"info\",\"event\":\"server_started\",\"bind_address\":\"" << json_escape(bind_address)
        << "\",\"port\":" << port
        << ",\"workers\":" << worker_count
        << ",\"queue_capacity\":" << queue_capacity
        << ",\"max_bulk_nodes\":" << max_bulk_nodes
        << ",\"rate_limit_rps\":" << rate_limit_rps
        << ",\"rate_limit_burst\":" << rate_limit_burst
        << ",\"max_request_bytes\":" << max_request_bytes
        << ",\"shutdown_checkpoint\":" << (shutdown_checkpoint ? "true" : "false")
        << ",\"physical_lattice_radius\":" << opt.physical_lattice_radius
        << ",\"physical_lattice_capacity\":" << lattice_capacity << "}";
    emit_structured_log(log.str());
  }

  ServerMetrics metrics;
  graphenedb::server::TokenBucketRateLimiter rate_limiter(rate_limit_rps, rate_limit_burst);
  std::atomic<uint64_t> request_sequence{1};
  std::atomic<uint32_t> next_lattice_slot{static_cast<uint32_t>(db.node_count())};
  std::mutex idempotency_mutex;
  auto handle_client = [&](ConnectionTask task) {
    const int fd = task.fd;
    metrics.requests.fetch_add(1, std::memory_order_relaxed);
    metrics.active.fetch_add(1, std::memory_order_relaxed);
    const auto request_started = std::chrono::steady_clock::now();
    const std::string request_id = std::to_string(::getpid()) + "-" + std::to_string(request_sequence.fetch_add(1, std::memory_order_relaxed));
    timeval socket_timeout{}; socket_timeout.tv_sec = socket_timeout_seconds; socket_timeout.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &socket_timeout, sizeof(socket_timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &socket_timeout, sizeof(socket_timeout));
    std::string req;
    char buf[8192];
    size_t content_length = 0;
    size_t header_end = std::string::npos;
    bool too_large = false;
    bool read_timeout = false;
    bool malformed_content_length = false;
    bool unsupported_transfer_encoding = false;
    bool content_length_present = false;
    while (true) {
      ssize_t n = ::read(fd, buf, sizeof(buf));
      if (n < 0 && errno == EINTR) continue;
      if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) { read_timeout = true; break; }
      if (n <= 0) break;
      req.append(buf, static_cast<size_t>(n));
      if (req.size() > max_request_bytes) { too_large = true; break; }
      header_end = req.find("\r\n\r\n");
      if (header_end != std::string::npos) {
        std::string headers = req.substr(0, header_end + 4);
        const std::string transfer_encoding = lower_copy(header_value(headers, "transfer-encoding"));
        if (!transfer_encoding.empty() && transfer_encoding != "identity") {
          unsupported_transfer_encoding = true;
          break;
        }
        const auto cl = parse_content_length(headers);
        content_length_present = cl.present;
        if (!cl.valid) {
          malformed_content_length = true;
          break;
        }
        content_length = cl.value;
        if (header_end + 4 + content_length > max_request_bytes) { too_large = true; break; }
        const size_t have_body = req.size() - (header_end + 4);
        if (!content_length_present || have_body >= content_length) break;
      }
    }
    if (req.empty() && !too_large && !read_timeout) {
      ::close(fd);
      metrics.active.fetch_sub(1, std::memory_order_relaxed);
      metrics.completed.fetch_add(1, std::memory_order_relaxed);
      return;
    }
    auto first_end = req.find("\r\n");
    auto bp = req.find("\r\n\r\n");
    std::string first = first_end == std::string::npos ? std::string{} : req.substr(0, first_end);
    std::string header_text = bp == std::string::npos ? std::string{} : req.substr(0, bp + 4);
    std::istringstream fsline(first); std::string method, path, http_version; fsline >> method >> path >> http_version;
    std::string client_ip = task.peer_ip;
    if (trust_proxy) {
      const std::string forwarded = first_forwarded_ip(header_text);
      if (!forwarded.empty()) client_ip = forwarded;
    }
    const bool public_probe = path == "/v1/health" || path == "/v1/ready" || path == "/v1/version";
    const bool authorized = public_probe || authorized_request(header_text, api_key);
    const bool forwarded_https_ok = !require_forwarded_https || public_probe || lower_copy(header_value(header_text, "x-forwarded-proto")) == "https";
    const bool rate_allowed = public_probe || rate_limiter.allow(client_ip);
    const std::string idempotency_key = header_value(header_text, "idempotency-key");
    std::string body;
    if (bp != std::string::npos) {
      body = req.substr(bp + 4);
      if (content_length > 0 && body.size() > content_length) body.resize(content_length);
    }
    metrics.request_bytes.fetch_add(req.size(), std::memory_order_relaxed);
    std::string out;
    std::string response_content_type = "application/json";
    int retry_after = 0;
    int code = 200;
    try {
      if (too_large) {
        code = 413; out = "{\"error\":\"request_too_large\",\"max_request_bytes\":" + std::to_string(max_request_bytes) + "}";
      } else if (read_timeout) {
        code = 408; out = "{\"error\":\"request_timeout\"}";
      } else if (first.empty() || method.empty() || path.empty() || http_version.rfind("HTTP/1.", 0) != 0) {
        code = 400; out = "{\"error\":\"malformed_request_line\"}";
      } else if (malformed_content_length) {
        code = 400; out = "{\"error\":\"invalid_content_length\"}";
      } else if (unsupported_transfer_encoding) {
        code = 501; out = "{\"error\":\"transfer_encoding_not_supported\",\"supported\":\"content-length\"}";
      } else if (endpoint_requires_json_body(method, path) && !content_length_present) {
        code = 411; out = "{\"error\":\"content_length_required\"}";
      } else if (endpoint_requires_json_body(method, path) && !application_json_content_type(header_text)) {
        code = 415; out = "{\"error\":\"application_json_required\"}";
      } else if (!rate_allowed) {
        metrics.rate_limited.fetch_add(1, std::memory_order_relaxed);
        code = 429; retry_after = 1; out = "{\"error\":\"rate_limited\"}";
      } else if (!forwarded_https_ok) {
        code = 403; out = "{\"error\":\"https_required_by_proxy_policy\"}";
      } else if (!authorized) {
        metrics.auth_failures.fetch_add(1, std::memory_order_relaxed);
        code = 401; out = "{\"error\":\"unauthorized\"}";
      } else if (method == "GET" && path == "/v1/health") {
        out = "{\"status\":\"ok\",\"engine\":\"physical-hex-lattice\",\"server_version\":\"" + std::string(kServerVersion) + "\",\"api_version\":" + std::to_string(kApiVersion) + "}";
      } else if (method == "GET" && path == "/v1/version") {
        std::ostringstream js;
        js << "{\"server_version\":\"" << kServerVersion
           << "\",\"api_version\":" << kApiVersion
           << ",\"storage_formats\":{\"manifest\":" << kManifestFormatVersion
           << ",\"storage\":" << kStorageFormatVersion
           << ",\"wal_frame\":" << kWalFrameFormatVersion
           << ",\"lattice\":" << kLatticeFormatVersion
           << ",\"extraction\":" << kExtractionFormatVersion
           << "},\"features\":[\"physical_lattice_primary\",\"vector_search\",\"lattice_search\",\"checkpoint\",\"backup\",\"validation\",\"idempotent_node_writes\"]}";
        out = js.str();
      } else if (method == "GET" && path == "/v1/ready") {
        std::string report; auto ist = db.inspect(&report);
        const uint64_t nodes = db.node_count();
        const bool capacity_ok = lattice_capacity == 0 || nodes < lattice_capacity;
        if (g_stop) {
          code = 503;
          out = "{\"status\":\"not_ready\",\"reason\":\"shutting_down\"}";
        } else if (!ist || !db.is_open() || !capacity_ok) {
          code = 503;
          out = "{\"status\":\"not_ready\",\"reason\":\"" + std::string(capacity_ok ? "storage_not_ready" : "physical_lattice_capacity_exhausted") + "\"}";
        } else {
          const double utilization = lattice_capacity ? static_cast<double>(nodes) / static_cast<double>(lattice_capacity) : 0.0;
          std::ostringstream js;
          js << "{\"status\":\"ready\",\"node_count\":" << nodes
             << ",\"physical_lattice_capacity\":" << lattice_capacity
             << ",\"physical_lattice_utilization\":" << utilization << "}";
          out = js.str();
        }
      } else if (method == "GET" && path == "/v1/metrics") {
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - metrics.started).count();
        const uint64_t duration_count = metrics.completed.load();
        const double avg_duration_ms = duration_count ? (static_cast<double>(metrics.duration_us_sum.load()) / duration_count / 1000.0) : 0.0;
        std::ostringstream js;
        js << "{\"uptime_seconds\":" << uptime
           << ",\"requests\":" << metrics.requests.load()
           << ",\"completed\":" << metrics.completed.load()
           << ",\"active\":" << metrics.active.load()
           << ",\"queue_depth\":" << metrics.queue_depth.load()
           << ",\"queue_rejected\":" << metrics.queue_rejected.load()
           << ",\"rate_limited\":" << metrics.rate_limited.load()
           << ",\"auth_failures\":" << metrics.auth_failures.load()
           << ",\"request_bytes\":" << metrics.request_bytes.load()
           << ",\"response_bytes\":" << metrics.response_bytes.load()
           << ",\"request_duration_avg_ms\":" << avg_duration_ms
           << ",\"request_duration_max_ms\":" << (metrics.duration_us_max.load()/1000.0)
           << ",\"status_2xx\":" << metrics.status_2xx.load()
           << ",\"status_4xx\":" << metrics.status_4xx.load()
           << ",\"status_5xx\":" << metrics.status_5xx.load()
           << ",\"node_inserts\":" << metrics.node_inserts.load()
           << ",\"edge_inserts\":" << metrics.edge_inserts.load()
           << ",\"vector_searches\":" << metrics.vector_searches.load()
           << ",\"lattice_searches\":" << metrics.lattice_searches.load()
           << ",\"validation_calls\":" << metrics.validation_calls.load()
           << ",\"checkpoint_calls\":" << metrics.checkpoint_calls.load()
           << ",\"backup_calls\":" << metrics.backup_calls.load()
           << ",\"errors\":" << metrics.errors.load()
           << ",\"node_count\":" << db.node_count()
           << ",\"edge_count\":" << db.edge_count()
           << ",\"physical_lattice_capacity\":" << lattice_capacity << "}";
        out = js.str();
      } else if (method == "GET" && path == "/v1/metrics/prometheus") {
        response_content_type = "text/plain; version=0.0.4";
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - metrics.started).count();
        std::ostringstream pm;
        pm << "# TYPE graphenedb_uptime_seconds gauge\n"
           << "graphenedb_uptime_seconds " << uptime << "\n"
           << "# TYPE graphenedb_http_requests_total counter\n"
           << "graphenedb_http_requests_total " << metrics.requests.load() << "\n"
           << "graphenedb_http_requests_completed_total " << metrics.completed.load() << "\n"
           << "graphenedb_http_active_requests " << metrics.active.load() << "\n"
           << "graphenedb_http_queue_depth " << metrics.queue_depth.load() << "\n"
           << "graphenedb_http_queue_rejected_total " << metrics.queue_rejected.load() << "\n"
           << "graphenedb_http_rate_limited_total " << metrics.rate_limited.load() << "\n"
           << "graphenedb_http_auth_failures_total " << metrics.auth_failures.load() << "\n"
           << "graphenedb_http_request_duration_seconds_sum " << (metrics.duration_us_sum.load()/1000000.0) << "\n"
           << "graphenedb_http_request_duration_seconds_count " << metrics.completed.load() << "\n"
           << "graphenedb_nodes " << db.node_count() << "\n"
           << "graphenedb_edges " << db.edge_count() << "\n"
           << "graphenedb_physical_lattice_capacity " << lattice_capacity << "\n";
        out = pm.str();
      } else if (method == "GET" && path == "/v1/admin/capacity") {
        const uint64_t nodes = db.node_count();
        const uint64_t remaining = lattice_capacity > nodes ? lattice_capacity - nodes : 0;
        const double utilization = lattice_capacity ? static_cast<double>(nodes) / lattice_capacity : 0.0;
        std::ostringstream js;
        js << "{\"physical_lattice_primary\":" << (opt.physical_lattice_primary ? "true" : "false")
           << ",\"radius\":" << opt.physical_lattice_radius
           << ",\"capacity\":" << lattice_capacity
           << ",\"nodes\":" << nodes
           << ",\"remaining\":" << remaining
           << ",\"utilization\":" << utilization
           << ",\"recommended_radius_for_expected_max_nodes\":" << minimum_radius_for_nodes(expected_max_nodes ? expected_max_nodes : nodes) << "}";
        out = js.str();
      } else if (method == "POST" && path == "/v1/admin/validate") {
        metrics.validation_calls.fetch_add(1, std::memory_order_relaxed);
        std::string report; auto vst = db.validate(&report);
        if (!vst) { code = 500; out = "{\"ok\":false,\"error\":\"" + json_escape(vst.message) + "\",\"report\":\"" + json_escape(report) + "\"}"; }
        else out = "{\"ok\":true,\"report\":\"" + json_escape(report) + "\"}";
      } else if (method == "POST" && (path == "/v1/admin/compact" || path == "/v1/admin/checkpoint")) {
        metrics.checkpoint_calls.fetch_add(1, std::memory_order_relaxed);
        auto cst = db.compact();
        if (!cst) { code = 500; out = "{\"ok\":false,\"error\":\"" + json_escape(cst.message) + "\"}"; }
        else out = "{\"ok\":true}";
      } else if (method == "GET" && path == "/v1/admin/inspect") {
        std::string report; auto ist = db.inspect(&report);
        if (!ist) { code = 500; out = "{\"ok\":false,\"error\":\"" + json_escape(ist.message) + "\"}"; }
        else out = "{\"ok\":true,\"report\":\"" + json_escape(report) + "\"}";
      } else if (method == "POST" && path == "/v1/admin/backup") {
        metrics.backup_calls.fetch_add(1, std::memory_order_relaxed);
        std::string dest = json_value(body, "destination");
        if (dest.empty()) { code = 400; out = "{\"error\":\"destination is required\"}"; }
        else { auto bst = db.backup(dest); if (!bst) { code = 500; out = "{\"ok\":false,\"error\":\"" + json_escape(bst.message) + "\"}"; } else out = "{\"ok\":true,\"destination\":\"" + json_escape(dest) + "\"}"; }
      } else if (method == "POST" && path == "/v1/nodes/bulk") {
        uint32_t count = json_u32(body, "count", 0);
        const uint64_t remaining_capacity = lattice_capacity > db.node_count() ? lattice_capacity - db.node_count() : 0;
        if (lattice_capacity && count > remaining_capacity) { code = 507; out = "{\"error\":\"physical_lattice_capacity_exceeded\",\"remaining\":" + std::to_string(remaining_capacity) + "}"; }
        else
        if (count == 0 || count > max_bulk_nodes) { code = 400; out = "{\"error\":\"count_out_of_range\",\"minimum\":1,\"maximum\":" + std::to_string(max_bulk_nodes) + "}"; }
        else {
          std::string prefix = json_value(body, "prefix");
          if (prefix.empty()) prefix = "bulk node";
          if (prefix.size() > kMaxBulkPrefixBytes) {
            code = 400;
            out = "{\"error\":\"bulk_prefix_too_large\",\"max_bytes\":" + std::to_string(kMaxBulkPrefixBytes) + "}";
          }
          std::string source = json_value(body, "source");
          if (source.empty()) source = "bulk";
          if (code != 200) {
            // Validation has already produced a response.
          } else {
          BatchInput batch;
          batch.nodes.reserve(count);
          const uint32_t base_slot = next_lattice_slot.fetch_add(count, std::memory_order_relaxed);
          for (uint32_t i = 0; i < count; ++i) {
            std::string text = prefix + " " + std::to_string(i);
            NodeInput ni; ni.content = text; ni.vector = embed_text(text, dim); ni.metadata["source"] = source;
            ni.lattice = spiral_coord(base_slot + i);
            batch.nodes.push_back(std::move(ni));
          }
          auto t0 = std::chrono::steady_clock::now();
          BatchResult batch_result;
          st = db.put_batch(batch, &batch_result);
          if (!st) {
            code = st.message.find("capacity") != std::string::npos || st.message.find("physical lattice") != std::string::npos ? 507 : 500;
            out = "{\"error\":\"" + json_escape(st.message) + "\",\"inserted\":0,\"atomic\":true}";
          } else {
            const uint32_t inserted = static_cast<uint32_t>(batch_result.node_ids.size());
            const uint32_t first_id = batch_result.node_ids.empty() ? UINT32_MAX : batch_result.node_ids.front();
            const uint32_t last_id = batch_result.node_ids.empty() ? UINT32_MAX : batch_result.node_ids.back();
            metrics.node_inserts.fetch_add(inserted, std::memory_order_relaxed);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
            code = 201; out = "{\"inserted\":" + std::to_string(inserted) + ",\"first_id\":" + std::to_string(first_id) + ",\"last_id\":" + std::to_string(last_id) + ",\"elapsed_ms\":" + std::to_string(ms) + ",\"atomic\":true}";
          }
          }
        }
      } else if (method == "POST" && path == "/v1/facts") {
        if (idempotency_key.size() > 128) { code = 400; out = "{\"error\":\"idempotency_key_too_long\",\"max_length\":128}"; }
        else if (lattice_capacity && db.node_count() >= lattice_capacity && idempotency_key.empty()) { code = 507; out = "{\"error\":\"physical_lattice_capacity_exhausted\"}"; }
        else {
        std::string text = json_value(body, "text");
        if (text.empty()) text = json_value(body, "content");
        if (text.empty()) { code = 400; out = "{\"error\":\"text/content is required\"}"; }
        else if (text.size() > kMaxNodeTextBytes) { code = 400; out = "{\"error\":\"text_too_large\",\"max_bytes\":" + std::to_string(kMaxNodeTextBytes) + "}"; }
        else {
          std::unique_lock<std::mutex> idempotency_lock;
          bool idempotency_handled = false;
          const std::string scoped_idempotency_key = path + ":" + idempotency_key;
          if (!idempotency_key.empty()) {
            idempotency_lock = std::unique_lock<std::mutex>(idempotency_mutex);
            const auto existing = db.metadata_search("_idempotency_key", scoped_idempotency_key);
            if (!existing.empty()) {
              const auto prior = db.get_node(existing.front());
              if (!prior || prior->content != text) {
                code = 409; out = "{\"error\":\"idempotency_conflict\"}"; idempotency_handled = true;
              } else {
                code = 200; out = "{\"id\":" + std::to_string(existing.front()) + ",\"type\":\"temporal_fact\",\"idempotent_replay\":true}"; idempotency_handled = true;
              }
            }
          }
          if (!idempotency_handled && lattice_capacity && db.node_count() >= lattice_capacity) {
            code = 507; out = "{\"error\":\"physical_lattice_capacity_exhausted\"}"; idempotency_handled = true;
          }
          if (!idempotency_handled) {
          NodeInput ni; ni.content = text; ni.vector = embed_text(text, dim);
          ni.metadata["source"] = json_value(body, "source").empty() ? "server" : json_value(body, "source");
          ni.metadata["graphene_type"] = "temporal_fact";
          if (!idempotency_key.empty()) ni.metadata["_idempotency_key"] = scoped_idempotency_key;
          for (const char* k : {"documented_at","valid_from","valid_until","confidence","fact_type","account","service","incident","repo","placement_policy"}) { std::string v = json_value(body, k); if (!v.empty()) ni.metadata[k] = v; }
          std::string placement_policy = json_value(body, "placement_policy");
          if (placement_policy == "semantic_causal_temporal" || placement_policy == "semantic") ni.lattice = choose_semantic_causal_temporal_cell(db, text, ni.vector, ni.metadata, next_lattice_slot);
          else ni.lattice = spiral_coord(next_lattice_slot.fetch_add(1, std::memory_order_relaxed));
          uint32_t id = 0; st = put_node_with_lattice_retry(db, ni, &id, next_lattice_slot);
          if (!st) { code = st.message.find("capacity") != std::string::npos || st.message.find("physical lattice") != std::string::npos ? 507 : http_status_for(st); out = "{\"error\":\"" + json_escape(st.message) + "\"}"; }
          else { metrics.node_inserts.fetch_add(1, std::memory_order_relaxed); code = 201; out = "{\"id\":" + std::to_string(id) + ",\"type\":\"temporal_fact\",\"q\":" + std::to_string(ni.lattice->q) + ",\"r\":" + std::to_string(ni.lattice->r) + ",\"layer\":" + std::to_string(ni.lattice->layer) + "}"; }
          }
        }
        }
      } else if (method == "POST" && path == "/v1/nodes") {
        if (idempotency_key.size() > 128) { code = 400; out = "{\"error\":\"idempotency_key_too_long\",\"max_length\":128}"; }
        else if (lattice_capacity && db.node_count() >= lattice_capacity && idempotency_key.empty()) { code = 507; out = "{\"error\":\"physical_lattice_capacity_exhausted\"}"; }
        else {
        std::string text = json_value(body, "text");
        if (text.empty()) text = json_value(body, "content");
        if (text.empty()) { code = 400; out = "{\"error\":\"text/content is required\"}"; }
        else if (text.size() > kMaxNodeTextBytes) { code = 400; out = "{\"error\":\"text_too_large\",\"max_bytes\":" + std::to_string(kMaxNodeTextBytes) + "}"; }
        else {
          std::unique_lock<std::mutex> idempotency_lock;
          bool idempotency_handled = false;
          const std::string scoped_idempotency_key = path + ":" + idempotency_key;
          if (!idempotency_key.empty()) {
            idempotency_lock = std::unique_lock<std::mutex>(idempotency_mutex);
            const auto existing = db.metadata_search("_idempotency_key", scoped_idempotency_key);
            if (!existing.empty()) {
              const auto prior = db.get_node(existing.front());
              if (!prior || prior->content != text) {
                code = 409; out = "{\"error\":\"idempotency_conflict\"}"; idempotency_handled = true;
              } else {
                code = 200; out = "{\"id\":" + std::to_string(existing.front()) + ",\"idempotent_replay\":true}"; idempotency_handled = true;
              }
            }
          }
          if (!idempotency_handled && lattice_capacity && db.node_count() >= lattice_capacity) {
            code = 507; out = "{\"error\":\"physical_lattice_capacity_exhausted\"}"; idempotency_handled = true;
          }
          if (!idempotency_handled) {
          NodeInput ni; ni.content = text; ni.vector = embed_text(text, dim);
          ni.metadata["source"] = json_value(body, "source").empty() ? "server" : json_value(body, "source");
          if (!idempotency_key.empty()) ni.metadata["_idempotency_key"] = scoped_idempotency_key;
          for (const char* k : {"documented_at","valid_from","valid_until","confidence","fact_type","account","service","incident","repo","placement_policy"}) { std::string v = json_value(body, k); if (!v.empty()) ni.metadata[k] = v; }
          std::string placement_policy = json_value(body, "placement_policy");
          if (placement_policy == "semantic_causal_temporal" || placement_policy == "semantic") ni.lattice = choose_semantic_causal_temporal_cell(db, text, ni.vector, ni.metadata, next_lattice_slot);
          else ni.lattice = spiral_coord(next_lattice_slot.fetch_add(1, std::memory_order_relaxed));
          uint32_t id = 0; st = put_node_with_lattice_retry(db, ni, &id, next_lattice_slot);
          if (!st) { code = st.message.find("capacity") != std::string::npos || st.message.find("physical lattice") != std::string::npos ? 507 : http_status_for(st); out = "{\"error\":\"" + json_escape(st.message) + "\"}"; }
          else { metrics.node_inserts.fetch_add(1, std::memory_order_relaxed); code = 201; out = "{\"id\":" + std::to_string(id) + ",\"q\":" + std::to_string(ni.lattice->q) + ",\"r\":" + std::to_string(ni.lattice->r) + ",\"layer\":" + std::to_string(ni.lattice->layer) + "}"; }
          }
        }
        }
      } else if (method == "POST" && path == "/v1/edges") {
        EdgeInput ei; ei.from = json_u32(body, "from", UINT32_MAX); ei.to = json_u32(body, "to", UINT32_MAX); ei.confidence = json_double(body, "confidence", 0.9);
        std::string role = json_value(body, "role");
        std::string origin = json_value(body, "origin");
        ei.role = parse_role(role);
        ei.origin = parse_origin(origin);
        for (const char* k : {"source_id","span","observed_at","derived_from","promotion_status","evidence_ref","reinforcement_count","salience_boost"}) { std::string v = json_value(body, k); if (!v.empty()) ei.metadata[k] = v; }
        uint32_t eid = 0; st = db.put_edge(ei, &eid);
        if (!st) { code = http_status_for(st); out = "{\"error\":\"" + json_escape(st.message) + "\"}"; }
        else { metrics.edge_inserts.fetch_add(1, std::memory_order_relaxed); code = 201; out = "{\"id\":" + std::to_string(eid) + "}"; }
      } else if (method == "POST" && path == "/v1/edges/provenance") {
        EdgeInput ei; ei.from = json_u32(body, "from", UINT32_MAX); ei.to = json_u32(body, "to", UINT32_MAX); ei.confidence = json_double(body, "confidence", 0.9);
        ei.role = parse_role(json_value(body, "role"));
        ei.origin = parse_origin(json_value(body, "origin"));
        for (const char* k : {"source_id","span","observed_at","derived_from","promotion_status","evidence_ref","reinforcement_count","salience_boost"}) { std::string v = json_value(body, k); if (!v.empty()) ei.metadata[k] = v; }
        uint32_t eid = 0; st = db.put_edge(ei, &eid);
        if (!st) { code = http_status_for(st); out = "{\"error\":\"" + json_escape(st.message) + "\"}"; }
        else { metrics.edge_inserts.fetch_add(1, std::memory_order_relaxed); code = 201; out = "{\"id\":" + std::to_string(eid) + ",\"origin\":\"" + origin_name(ei.origin) + "\",\"role\":\"" + role_name(ei.role) + "\"}"; }
      } else if (method == "POST" && (path == "/v1/retrieve/bundle" || path == "/v1/retrieve/explain")) {
        uint32_t anchor = json_u32(body, "anchor", json_u32(body, "anchor_node_id", 0));
        uint32_t hops = std::clamp<uint32_t>(json_u32(body, "max_hops", json_u32(body, "hops", 3)), 1, kMaxBundleHops);
        std::string mode = json_value(body, "mode"); if (mode.empty()) mode = "balanced";
        std::string as_of = json_value(body, "as_of"); if (as_of.empty()) as_of = json_value(body, "at");
        out = build_bundle_json(db, anchor, hops, mode, as_of);
      } else if (method == "POST" && path == "/v1/admin/validate/provenance") {
        uint32_t missing = 0, inferred_without_derived = 0, reinforced_truth_risk = 0;
        const size_t total = db.edge_count();
        for (uint32_t eid = 0; eid < total + 1000; ++eid) {
          auto e = db.get_edge(eid); if (!e) { if (eid > total + 16) break; continue; }
          bool has_evidence = e->metadata.count("source_id") || e->metadata.count("evidence_ref");
          if (!has_evidence && e->origin != EdgeOrigin::Inferred && e->origin != EdgeOrigin::Hypothetical) ++missing;
          if (e->origin == EdgeOrigin::Inferred && !e->metadata.count("derived_from")) ++inferred_without_derived;
          if (e->origin == EdgeOrigin::Reinforced && e->metadata.find("promotion_status") != e->metadata.end() && e->metadata.at("promotion_status") == "discovered") ++reinforced_truth_risk;
        }
        out = "{\"ok\":" + std::string((missing==0 && inferred_without_derived==0 && reinforced_truth_risk==0)?"true":"false") + ",\"edges_checked\":" + std::to_string(total) + ",\"missing_evidence\":" + std::to_string(missing) + ",\"inferred_without_derived_from\":" + std::to_string(inferred_without_derived) + ",\"reinforced_truth_promotion_risk\":" + std::to_string(reinforced_truth_risk) + "}";
      } else if (method == "POST" && path == "/v1/admin/lattice/quality") {
        out = lattice_quality_json(db);
      } else if (method == "POST" && path == "/v1/retrieve/temporal") {
        std::string as_of = json_value(body, "as_of"); if (as_of.empty()) as_of = json_value(body, "at");
        std::string key = json_value(body, "metadata_key");
        std::string val = json_value(body, "metadata_value");
        const uint32_t limit = std::clamp<uint32_t>(json_u32(body, "limit", 100), 1, static_cast<uint32_t>(kMaxTemporalResults));
        const uint32_t offset = json_u32(body, "offset", 0);
        std::ostringstream js; js << "{\"as_of\":\"" << json_escape(as_of) << "\",\"limit\":" << limit << ",\"offset\":" << offset << ",\"results\":[";
        bool first = true;
        uint32_t matched = 0;
        uint32_t returned = 0;
        size_t total = db.node_count();
        for (uint32_t id = 0; id < total + 1000; ++id) {
          auto n0 = db.get_node(id);
          if (!n0) { if (id > total + 16) break; continue; }
          if (!node_valid_at(*n0, as_of)) continue;
          if (!key.empty()) {
            auto it = n0->metadata.find(key);
            if (it == n0->metadata.end() || it->second != val) continue;
          }
          if (matched++ < offset) continue;
          if (returned >= limit) break;
          if (!first) js << ',';
          first = false;
          js << "{\"id\":" << id << ",\"content\":\"" << json_escape(n0->content) << "\",\"metadata\":" << metadata_json(n0->metadata) << "}";
          ++returned;
        }
        js << "],\"returned\":" << returned << ",\"next_offset\":" << (offset + returned) << "}"; out = js.str();
      } else if (method == "POST" && path == "/v1/search/hybrid") {
        metrics.vector_searches.fetch_add(1, std::memory_order_relaxed);
        std::string q = json_value(body, "query");
        if (q.empty()) {
          code = 400;
          out = "{\"error\":\"query is required\"}";
        } else {
        size_t topk = 5;
        try { auto k = json_value(body, "top_k"); if (!k.empty()) topk = static_cast<size_t>(std::stoull(k)); } catch (...) {}
        topk = std::clamp<size_t>(topk, 1, kMaxSearchTopK);
        auto hits = db.vector_search(embed_text(q, dim), topk);
        std::ostringstream js; js << "{\"results\":[";
        for (size_t i = 0; i < hits.size(); ++i) {
          auto n0 = db.get_node(hits[i].node_id);
          if (i) js << ',';
          js << "{\"id\":" << hits[i].node_id << ",\"score\":" << hits[i].score;
          if (n0) js << ",\"content\":\"" << json_escape(n0->content) << "\"";
          js << '}';
        }
        js << "]}"; out = js.str();
        }
      } else if (method == "GET" && path.rfind("/v1/nodes/", 0) == 0) {
        const std::string raw_id = path.substr(10);
        if (raw_id.empty() || !std::all_of(raw_id.begin(), raw_id.end(), [](unsigned char c){ return std::isdigit(c); })) {
          code = 400; out = "{\"error\":\"invalid_node_id\"}";
        } else {
          const auto parsed = std::stoull(raw_id);
          if (parsed > UINT32_MAX) { code = 400; out = "{\"error\":\"invalid_node_id\"}"; }
          else {
            const uint32_t id = static_cast<uint32_t>(parsed);
            auto n0 = db.get_node(id);
            if (!n0) { code = 404; out = "{\"error\":\"not found\"}"; }
            else out = "{\"id\":" + std::to_string(id) + ",\"content\":\"" + json_escape(n0->content) + "\",\"metadata\":" + metadata_json(n0->metadata) + "}";
          }
        }
      } else if (method == "GET" && path.rfind("/v1/search/lattice", 0) == 0) {
        metrics.lattice_searches.fetch_add(1, std::memory_order_relaxed);
        uint32_t id = query_u32(path, "node_id", 0);
        uint32_t hops = std::clamp<uint32_t>(query_u32(path, "hops", 2), 1, kMaxLatticeHops);
        if (!db.get_node(id)) { code = 404; out = "{\"error\":\"not found\"}"; }
        else {
        auto ns = db.lattice_neighbors(id, hops);
        std::ostringstream js; js << "{\"neighbors\":[";
        for (size_t i = 0; i < ns.size(); ++i) { if (i) js << ','; js << ns[i]; }
        js << "],\"hops\":" << hops << "}"; out = js.str();
        }
      } else {
        code = 404; out = "{\"error\":\"unknown endpoint\"}";
      }
    } catch (const std::exception& ex) {
      code = 500; out = "{\"error\":\"" + json_escape(ex.what()) + "\"}";
    }
    if (code >= 400) metrics.errors.fetch_add(1, std::memory_order_relaxed);
    if (code >= 200 && code < 300) metrics.status_2xx.fetch_add(1, std::memory_order_relaxed);
    else if (code >= 400 && code < 500) metrics.status_4xx.fetch_add(1, std::memory_order_relaxed);
    else if (code >= 500) metrics.status_5xx.fetch_add(1, std::memory_order_relaxed);
    auto resp = response(code, out, request_id, response_content_type, retry_after);
    (void)write_all(fd, resp);
    metrics.response_bytes.fetch_add(resp.size(), std::memory_order_relaxed);
    ::close(fd);
    const uint64_t duration_us = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - request_started).count());
    metrics.duration_us_sum.fetch_add(duration_us, std::memory_order_relaxed);
    graphenedb::server::atomic_max(metrics.duration_us_max, duration_us);
    metrics.active.fetch_sub(1, std::memory_order_relaxed);
    metrics.completed.fetch_add(1, std::memory_order_relaxed);
    structured_request_log(request_id, client_ip, method, path, code, duration_us, req.size(), resp.size(), !rate_allowed);
  };

  graphenedb::server::BoundedWorkerPool pool(worker_count, queue_capacity, handle_client, metrics);
  while (!g_stop) {
    sockaddr_in peer{};
    socklen_t peer_len = sizeof(peer);
    int fd = ::accept(server_fd, reinterpret_cast<sockaddr*>(&peer), &peer_len);
    if (fd < 0) {
      if (g_stop) break;
      if (errno == EINTR) continue;
      perror("accept");
      break;
    }
    char ipbuf[INET_ADDRSTRLEN]{};
    const char* ip = inet_ntop(AF_INET, &peer.sin_addr, ipbuf, sizeof(ipbuf));
    ConnectionTask task{fd, ip ? std::string(ip) : std::string("unknown")};
    if (!pool.submit(std::move(task))) {
      const std::string body = "{\"error\":\"server_overloaded\"}";
      const auto resp = response(503, body, {}, "application/json", 1);
      (void)write_all(fd, resp);
      ::close(fd);
    }
  }
  if (g_listen_fd >= 0) {
    ::close(static_cast<int>(g_listen_fd));
    g_listen_fd = -1;
  }
  {
    std::ostringstream log;
    log << "{\"timestamp\":\"" << graphenedb::server::utc_timestamp()
        << "\",\"level\":\"info\",\"event\":\"shutdown_started\",\"active_requests\":"
        << metrics.active.load() << ",\"queued_requests\":" << metrics.queue_depth.load() << "}";
    emit_structured_log(log.str());
  }
  pool.shutdown();
  int exit_code = 0;
  if (shutdown_checkpoint) {
    const auto checkpoint_started = std::chrono::steady_clock::now();
    const auto cst = db.compact();
    const auto checkpoint_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - checkpoint_started).count();
    std::ostringstream log;
    log << "{\"timestamp\":\"" << graphenedb::server::utc_timestamp()
        << "\",\"level\":\"" << (cst ? "info" : "error")
        << "\",\"event\":\"shutdown_checkpoint\",\"ok\":" << (cst ? "true" : "false")
        << ",\"duration_ms\":" << checkpoint_ms;
    if (!cst) {
      log << ",\"error\":\"" << json_escape(cst.message) << "\"";
      exit_code = 1;
    }
    log << "}";
    emit_structured_log(log.str());
  }
  const auto close_status = db.close();
  if (!close_status) exit_code = 1;
  {
    std::ostringstream log;
    log << "{\"timestamp\":\"" << graphenedb::server::utc_timestamp()
        << "\",\"level\":\"" << (exit_code == 0 ? "info" : "error")
        << "\",\"event\":\"shutdown_complete\",\"exit_code\":" << exit_code << "}";
    emit_structured_log(log.str());
  }
  return exit_code;
}
