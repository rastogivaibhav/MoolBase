#include "graphene/db.hpp"
#include "graphene/platform.hpp"
#include "graphene/vector_index.hpp"
#include "graphene/kdtree_index.hpp"
#include <algorithm>
#include <bit>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <numeric>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace fs = std::filesystem;

namespace graphene {

namespace {

uint64_t fnv1a(const std::string& s) {
  uint64_t h = 1469598103934665603ull;
  for (unsigned char c : s) {
    h ^= c;
    h *= 1099511628211ull;
  }
  return h;
}

std::string hex_encode(const std::string& input) {
  static const char* hex = "0123456789abcdef";
  std::string out;
  out.reserve(input.size() * 2);
  for (unsigned char c : input) {
    out.push_back(hex[c >> 4]);
    out.push_back(hex[c & 0x0f]);
  }
  return out;
}

bool hex_decode(const std::string& input, std::string* out) {
  if (input.size() % 2 != 0) return false;
  auto val = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  };
  out->clear();
  out->reserve(input.size() / 2);
  for (size_t i = 0; i < input.size(); i += 2) {
    int hi = val(input[i]);
    int lo = val(input[i + 1]);
    if (hi < 0 || lo < 0) return false;
    out->push_back(static_cast<char>((hi << 4) | lo));
  }
  return true;
}

std::string serialize_vector(const std::vector<float>& v) {
  std::ostringstream os;
  os << std::setprecision(9);
  for (size_t i = 0; i < v.size(); ++i) {
    if (i) os << ',';
    os << v[i];
  }
  return os.str();
}

bool parse_vector(const std::string& s, std::vector<float>* out) {
  out->clear();
  if (s.empty()) return true;
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, ',')) {
    try {
      size_t pos = 0;
      float f = std::stof(item, &pos);
      if (pos != item.size() || !std::isfinite(f)) return false;
      out->push_back(f);
    } catch (...) {
      return false;
    }
  }
  return true;
}

std::string serialize_metadata(const std::map<std::string, std::string>& m) {
  std::ostringstream os;
  bool first = true;
  for (const auto& kv : m) {
    if (!first) os << ';';
    first = false;
    os << hex_encode(kv.first) << '=' << hex_encode(kv.second);
  }
  return os.str();
}

bool parse_metadata(const std::string& s, std::map<std::string, std::string>* out) {
  out->clear();
  if (s.empty()) return true;
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, ';')) {
    auto pos = item.find('=');
    if (pos == std::string::npos) return false;
    std::string k, v;
    if (!hex_decode(item.substr(0, pos), &k)) return false;
    if (!hex_decode(item.substr(pos + 1), &v)) return false;
    (*out)[k] = v;
  }
  return true;
}

std::vector<std::string> split_tab(const std::string& s) {
  std::vector<std::string> parts;
  size_t start = 0;
  while (true) {
    size_t pos = s.find('\t', start);
    if (pos == std::string::npos) {
      parts.push_back(s.substr(start));
      break;
    }
    parts.push_back(s.substr(start, pos - start));
    start = pos + 1;
  }
  return parts;
}

std::string make_frame(const std::string& payload) {
  return std::to_string(payload.size()) + "|" + std::to_string(fnv1a(payload)) + "|" + payload + "\n";
}

bool parse_frame_line(const std::string& line, std::string* payload) {
  auto p1 = line.find('|');
  if (p1 == std::string::npos) return false;
  auto p2 = line.find('|', p1 + 1);
  if (p2 == std::string::npos) return false;
  size_t len = 0;
  uint64_t checksum = 0;
  try {
    len = static_cast<size_t>(std::stoull(line.substr(0, p1)));
    checksum = std::stoull(line.substr(p1 + 1, p2 - p1 - 1));
  } catch (...) {
    return false;
  }
  std::string data = line.substr(p2 + 1);
  if (data.size() != len) return false;
  if (fnv1a(data) != checksum) return false;
  *payload = std::move(data);
  return true;
}

bool visible_node(const Node& n, uint64_t snap) {
  return n.created_version <= snap && snap < n.deleted_version;
}

bool visible_edge(const Edge& e, const std::vector<Node>& nodes, uint64_t snap) {
  if (!(e.created_version <= snap && snap < e.deleted_version)) return false;
  if (e.from >= nodes.size() || e.to >= nodes.size()) return false;
  return visible_node(nodes[e.from], snap) && visible_node(nodes[e.to], snap);
}

int popcount64(uint64_t x) { return static_cast<int>(std::popcount(x)); }

double cosine(const std::vector<float>& a, const std::vector<float>& b) {
  if (a.size() != b.size() || a.empty()) return -1.0;
  double dot = 0.0, na = 0.0, nb = 0.0;
  for (size_t i = 0; i < a.size(); ++i) {
    dot += static_cast<double>(a[i]) * b[i];
    na += static_cast<double>(a[i]) * a[i];
    nb += static_cast<double>(b[i]) * b[i];
  }
  if (na == 0.0 || nb == 0.0) return -1.0;
  return dot / (std::sqrt(na) * std::sqrt(nb));
}

std::string bool_s(bool b) { return b ? "1" : "0"; }

} // namespace

struct GrapheneDB::Impl {
  mutable std::shared_mutex mu;
  bool open{false};
  fs::path dir, wal_path, data_path, manifest_path, lock_path;
  DBOptions opt;
  platform::FileHandle wal_fd{platform::kInvalidFile};
  uint64_t version{1};
  uint64_t txid{1};
  uint32_t next_node_id{0};
  uint32_t next_edge_id{0};
  size_t live_node_count{0};
  size_t live_edge_count{0};
  std::unique_ptr<VectorIndex> vector_index;
  std::vector<Node> nodes;
  std::vector<Edge> edges;
  std::unordered_map<uint32_t, std::vector<uint32_t>> in_edges;
  std::unordered_map<uint32_t, std::vector<uint32_t>> out_edges;
  std::unordered_map<uint64_t, std::vector<uint32_t>> planes;
  std::unordered_map<std::string, std::vector<uint32_t>> metadata_index;

  void ensure_node_slot(uint32_t id) {
    if (nodes.size() <= id) {
      size_t old = nodes.size();
      nodes.resize(static_cast<size_t>(id) + 1);
      for (size_t i = old; i < nodes.size(); ++i) {
        nodes[i].id = static_cast<uint32_t>(i);
        nodes[i].created_version = kInfVersion;
        nodes[i].deleted_version = kInfVersion;
      }
    }
  }

  void ensure_edge_slot(uint32_t id) {
    if (edges.size() <= id) {
      size_t old = edges.size();
      edges.resize(static_cast<size_t>(id) + 1);
      for (size_t i = old; i < edges.size(); ++i) {
        edges[i].id = static_cast<uint32_t>(i);
        edges[i].created_version = kInfVersion;
        edges[i].deleted_version = kInfVersion;
      }
    }
  }

  void index_node_metadata(const Node& n) {
    for (const auto& kv : n.metadata) {
      metadata_index[kv.first + "\x1f" + kv.second].push_back(n.id);
    }
  }

  Status checkpoint_unlocked() {
    uint64_t snap = version ? version - 1 : 0;
    fs::path tmp = data_path.string() + ".tmp";
    {
      std::ofstream out(tmp, std::ios::trunc);
      if (!out) return Status::error(ErrorCode::IoError, "cannot write data tmp");
      for (const auto& n : nodes) if (visible_node(n, snap)) out << make_frame(node_payload(n, "DATA_NODE"));
      for (const auto& e : edges) if (visible_edge(e, nodes, snap)) out << make_frame(edge_payload(e, "DATA_EDGE"));
      out.flush();
      if (!out) return Status::error(ErrorCode::IoError, "failed flushing data tmp");
    }
    std::error_code ec;
    fs::rename(tmp, data_path, ec);
    if (ec) return Status::error(ErrorCode::IoError, "checkpoint rename failed: " + ec.message());
    close_wal_fd_unlocked();
    { std::ofstream wal(wal_path, std::ios::trunc); if (!wal) return Status::error(ErrorCode::IoError, "cannot truncate WAL"); }
    auto ost = open_wal_fd_unlocked();
    if (!ost) return ost;
    return write_manifest();
  }

  Status maybe_rotate_wal_unlocked() {
    if (opt.wal_rotate_bytes == 0) return Status::ok();
    std::error_code ec;
    auto sz = fs::exists(wal_path, ec) ? fs::file_size(wal_path, ec) : 0;
    if (ec) return Status::error(ErrorCode::IoError, "cannot stat WAL: " + ec.message());
    if (sz >= opt.wal_rotate_bytes) return checkpoint_unlocked();
    return Status::ok();
  }

  Status open_wal_fd_unlocked() {
    if (wal_fd != platform::kInvalidFile) return Status::ok();
    auto st = platform::open_append(wal_path, &wal_fd);
    if (!st) return Status::error(st.code, "open WAL fd failed: " + st.message);
    return Status::ok();
  }

  void close_wal_fd_unlocked() {
    if (wal_fd != platform::kInvalidFile) { platform::close_file(wal_fd); wal_fd = platform::kInvalidFile; }
  }

  Status append_wal_unlocked(const std::vector<std::string>& payloads) {
    auto ost = open_wal_fd_unlocked();
    if (!ost) return ost;
    std::string blob;
    for (const auto& p : payloads) blob += make_frame(p);
    auto wst = platform::write_all(wal_fd, blob.data(), blob.size());
    if (!wst) return Status::error(wst.code, "write WAL failed: " + wst.message);
    if (opt.fsync_on_commit) {
      auto fst = platform::flush(wal_fd);
      if (!fst) return Status::error(fst.code, "fsync WAL failed: " + fst.message);
    }
    return Status::ok();
  }

  void rebuild_indexes() {
    in_edges.clear(); out_edges.clear(); planes.clear(); metadata_index.clear();
    vector_index = std::make_unique<KDTreeVectorIndex>(opt.dimension);
    live_node_count = 0; live_edge_count = 0;
    next_node_id = 0; next_edge_id = 0; version = std::max<uint64_t>(version, 1);
    uint64_t current_snap = 0;
    for (const auto& n : nodes) {
      if (n.created_version == kInfVersion) continue;
      current_snap = std::max(current_snap, n.created_version);
      if (n.deleted_version != kInfVersion) current_snap = std::max(current_snap, n.deleted_version);
    }
    for (const auto& e : edges) {
      if (e.created_version == kInfVersion) continue;
      current_snap = std::max(current_snap, e.created_version);
      if (e.deleted_version != kInfVersion) current_snap = std::max(current_snap, e.deleted_version);
    }
    for (auto& n : nodes) {
      if (n.created_version == kInfVersion) continue;
      next_node_id = std::max(next_node_id, n.id + 1);
      version = std::max(version, n.created_version + 1);
      if (n.deleted_version != kInfVersion) version = std::max(version, n.deleted_version + 1);
      planes[n.signature].push_back(n.id);
      index_node_metadata(n);
      if (visible_node(n, current_snap)) {
        ++live_node_count;
        if (vector_index) (void)vector_index->add(n.id, n.vector);
      }
    }
    for (auto& e : edges) {
      if (e.created_version == kInfVersion) continue;
      next_edge_id = std::max(next_edge_id, e.id + 1);
      version = std::max(version, e.created_version + 1);
      if (e.deleted_version != kInfVersion) version = std::max(version, e.deleted_version + 1);
      out_edges[e.from].push_back(e.id);
      in_edges[e.to].push_back(e.id);
      if (visible_edge(e, nodes, current_snap)) ++live_edge_count;
    }
    txid = std::max(txid, version + 1);
  }

  bool vector_valid(const std::vector<float>& v, std::string* reason = nullptr) const {
    if (opt.dimension == 0) {
      if (reason) *reason = "database dimension is zero";
      return false;
    }
    if (v.size() != opt.dimension) {
      if (reason) *reason = "expected dimension " + std::to_string(opt.dimension) + ", got " + std::to_string(v.size());
      return false;
    }
    for (float f : v) {
      if (!std::isfinite(f)) {
        if (reason) *reason = "vector contains NaN or infinity";
        return false;
      }
    }
    return true;
  }

  std::string node_payload(const Node& n, const std::string& op) const {
    return op + "\t" + std::to_string(n.id) + "\t" + std::to_string(n.created_version) + "\t" +
           std::to_string(n.deleted_version) + "\t" + std::to_string(n.signature) + "\t" +
           std::to_string(n.incident) + "\t" + bool_s(n.root) + "\t" + bool_s(n.symptom) + "\t" +
           bool_s(n.impact) + "\t" + hex_encode(n.content) + "\t" + serialize_vector(n.vector) + "\t" +
           serialize_metadata(n.metadata);
  }

  std::string edge_payload(const Edge& e, const std::string& op) const {
    return op + "\t" + std::to_string(e.id) + "\t" + std::to_string(e.from) + "\t" +
           std::to_string(e.to) + "\t" + std::to_string(static_cast<int>(e.origin)) + "\t" +
           std::to_string(static_cast<int>(e.role)) + "\t" + std::to_string(e.confidence) + "\t" +
           std::to_string(e.created_version) + "\t" + std::to_string(e.deleted_version) + "\t" +
           serialize_metadata(e.metadata);
  }

  Status apply_payload(const std::string& payload, bool from_replay, std::unordered_map<uint64_t, std::vector<std::string>>* pending = nullptr, std::unordered_set<uint64_t>* committed = nullptr) {
    auto p = split_tab(payload);
    if (p.empty()) return Status::error(ErrorCode::DataCorrupt, "empty payload");
    const auto& op = p[0];
    try {
      if (op == "BEGIN") {
        if (p.size() != 2 || !pending) return Status::error(ErrorCode::WalCorrupt, "bad BEGIN");
        uint64_t t = std::stoull(p[1]);
        (*pending)[t] = {};
        return Status::ok();
      }
      if (op == "COMMIT") {
        if (p.size() != 2 || !pending || !committed) return Status::error(ErrorCode::WalCorrupt, "bad COMMIT");
        uint64_t t = std::stoull(p[1]);
        auto it = pending->find(t);
        if (it == pending->end()) return Status::error(ErrorCode::TransactionIncomplete, "commit without begin");
        for (const auto& rec : it->second) {
          auto st = apply_payload(rec, true, nullptr, nullptr);
          if (!st) return st;
        }
        committed->insert(t);
        pending->erase(it);
        return Status::ok();
      }
      if (pending && op != "BEGIN" && op != "COMMIT") {
        if (p.size() < 2) return Status::error(ErrorCode::WalCorrupt, "transactional record missing txid");
        uint64_t t = std::stoull(p[1]);
        auto it = pending->find(t);
        if (it == pending->end()) return Status::error(ErrorCode::TransactionIncomplete, "record without begin");
        std::string without_tx;
        auto first_tab = payload.find('\t');
        auto second_tab = payload.find('\t', first_tab + 1);
        if (second_tab == std::string::npos) return Status::error(ErrorCode::WalCorrupt, "bad transactional record");
        without_tx = payload.substr(0, first_tab) + payload.substr(second_tab);
        it->second.push_back(without_tx);
        return Status::ok();
      }
      if (op == "PUT_NODE" || op == "DATA_NODE") {
        if (p.size() != 12) return Status::error(ErrorCode::DataCorrupt, "bad node record field count");
        Node n;
        n.id = static_cast<uint32_t>(std::stoul(p[1]));
        n.created_version = std::stoull(p[2]);
        n.deleted_version = std::stoull(p[3]);
        n.signature = std::stoull(p[4]);
        n.incident = static_cast<uint32_t>(std::stoul(p[5]));
        n.root = p[6] == "1";
        n.symptom = p[7] == "1";
        n.impact = p[8] == "1";
        if (!hex_decode(p[9], &n.content)) return Status::error(ErrorCode::DataCorrupt, "bad content encoding");
        if (!parse_vector(p[10], &n.vector)) return Status::error(ErrorCode::DataCorrupt, "bad vector encoding");
        if (!parse_metadata(p[11], &n.metadata)) return Status::error(ErrorCode::DataCorrupt, "bad metadata encoding");
        std::string reason;
        if (!vector_valid(n.vector, &reason)) return Status::error(ErrorCode::DimensionMismatch, reason);
        ensure_node_slot(n.id);
        nodes[n.id] = std::move(n);
        return Status::ok();
      }
      if (op == "PUT_EDGE" || op == "DATA_EDGE") {
        if (p.size() != 10) return Status::error(ErrorCode::DataCorrupt, "bad edge record field count");
        Edge e;
        e.id = static_cast<uint32_t>(std::stoul(p[1]));
        e.from = static_cast<uint32_t>(std::stoul(p[2]));
        e.to = static_cast<uint32_t>(std::stoul(p[3]));
        e.origin = static_cast<EdgeOrigin>(std::stoi(p[4]));
        e.role = static_cast<EdgeRole>(std::stoi(p[5]));
        e.confidence = std::stod(p[6]);
        e.created_version = std::stoull(p[7]);
        e.deleted_version = std::stoull(p[8]);
        if (!parse_metadata(p[9], &e.metadata)) return Status::error(ErrorCode::DataCorrupt, "bad edge metadata");
        if (e.from >= nodes.size() || e.to >= nodes.size()) return Status::error(ErrorCode::EdgeInvalid, "edge endpoint missing during replay");
        ensure_edge_slot(e.id);
        edges[e.id] = std::move(e);
        return Status::ok();
      }
      if (op == "DELETE_NODE") {
        if (p.size() != 3) return Status::error(ErrorCode::DataCorrupt, "bad delete node record");
        uint32_t id = static_cast<uint32_t>(std::stoul(p[1]));
        uint64_t v = std::stoull(p[2]);
        if (id >= nodes.size()) return Status::error(ErrorCode::NodeNotFound, "delete missing node during replay");
        nodes[id].deleted_version = v;
        return Status::ok();
      }
      return Status::error(ErrorCode::DataCorrupt, "unknown op: " + op);
    } catch (const std::exception& e) {
      return Status::error(ErrorCode::DataCorrupt, std::string("parse error: ") + e.what());
    }
  }

  Status load_framed_file(const fs::path& path, bool transactional, bool stop_on_bad_frame) {
    if (!fs::exists(path)) return Status::ok();
    std::ifstream in(path);
    if (!in) return Status::error(ErrorCode::IoError, "cannot read " + path.string());
    std::string line;
    std::unordered_map<uint64_t, std::vector<std::string>> pending;
    std::unordered_set<uint64_t> committed;
    while (std::getline(in, line)) {
      if (line.empty()) continue;
      std::string payload;
      if (!parse_frame_line(line, &payload)) {
        if (stop_on_bad_frame) break; // torn tail: recover to last valid frame
        return Status::error(ErrorCode::DataCorrupt, "corrupt frame in " + path.string());
      }
      Status st = transactional ? apply_payload(payload, true, &pending, &committed)
                                : apply_payload(payload, true, nullptr, nullptr);
      if (!st) return st;
    }
    // Pending transactions are intentionally ignored: no COMMIT means no durability.
    return Status::ok();
  }

  Status write_manifest() const {
    std::ostringstream os;
    os << "graphenedb_manifest_v1\n";
    os << "dimension=" << opt.dimension << "\n";
    os << "version=" << version << "\n";
    os << "next_node_id=" << next_node_id << "\n";
    os << "next_edge_id=" << next_edge_id << "\n";
    os << "next_txid=" << txid << "\n";
    std::string body = os.str();
    body += "checksum=" + std::to_string(fnv1a(body)) + "\n";
    fs::path tmp = manifest_path.string() + ".tmp";
    std::ofstream out(tmp, std::ios::trunc);
    if (!out) return Status::error(ErrorCode::IoError, "cannot write manifest tmp");
    out << body;
    out.flush();
    out.close();
    fs::rename(tmp, manifest_path);
    return Status::ok();
  }

  Status read_manifest(uint32_t* dim, uint64_t* next_txid) const {
    if (!fs::exists(manifest_path)) return Status::ok();
    std::ifstream in(manifest_path);
    std::string line;
    while (std::getline(in, line)) {
      if (line.rfind("dimension=", 0) == 0) {
        *dim = static_cast<uint32_t>(std::stoul(line.substr(10)));
      } else if (line.rfind("next_txid=", 0) == 0 && next_txid) {
        *next_txid = std::stoull(line.substr(10));
      }
    }
    return Status::ok();
  }

  Status acquire_lock() {
    std::error_code ec;
    if (fs::exists(lock_path, ec)) {
      bool removed_stale = false;
      if (opt.recover_stale_lock) {
        std::ifstream in(lock_path);
        uint64_t pid = 0;
        in >> pid;
        if (pid > 1 && !platform::process_is_alive(pid)) {
          fs::remove(lock_path, ec);
          removed_stale = !fs::exists(lock_path, ec);
        }
      }
      if (!removed_stale && fs::exists(lock_path, ec)) {
        return Status::error(ErrorCode::LockBusy, "database lock exists: " + lock_path.string());
      }
    }
    std::ofstream out(lock_path, std::ios::out | std::ios::trunc);
    if (!out) {
      if (fs::exists(lock_path)) return Status::error(ErrorCode::LockBusy, "database lock exists: " + lock_path.string());
      return Status::error(ErrorCode::IoError, "cannot create lock: " + lock_path.string());
    }
    out << platform::current_pid() << "\n";
    return Status::ok();
  }

  void release_lock() {
    if (!lock_path.empty()) {
      std::error_code ec;
      fs::remove(lock_path, ec);
    }
  }

  Path reverse_root(uint32_t anchor, QueryMode mode, uint64_t snap) const {
    Path path;
    if (anchor >= nodes.size() || !visible_node(nodes[anchor], snap)) return path;
    std::vector<uint32_t> queue{anchor};
    std::unordered_map<uint32_t, uint32_t> parent_node;
    std::unordered_map<uint32_t, uint32_t> parent_edge;
    std::unordered_set<uint32_t> seen{anchor};
    uint32_t root = UINT32_MAX;
    for (size_t i = 0; i < queue.size() && i < opt.tuning.max_reverse_bfs_visits; ++i) {
      uint32_t cur = queue[i];
      if (nodes[cur].root) { root = cur; break; }
      auto it = in_edges.find(cur);
      if (it == in_edges.end()) continue;
      for (uint32_t eid : it->second) {
        if (eid >= edges.size() || !visible_edge(edges[eid], nodes, snap)) continue;
        const auto& e = edges[eid];
        if (mode == QueryMode::Empirical && (e.origin == EdgeOrigin::Hypothetical || e.role == EdgeRole::Analogical)) continue;
        if (!seen.count(e.from)) {
          seen.insert(e.from);
          parent_node[e.from] = cur;
          parent_edge[e.from] = eid;
          queue.push_back(e.from);
        }
      }
    }
    if (root == UINT32_MAX) return path;
    uint32_t cur = root;
    path.nodes.push_back(cur);
    while (cur != anchor) {
      uint32_t eid = parent_edge.at(cur);
      path.edges.push_back(eid);
      const auto& e = edges[eid];
      path.contains_hypothetical |= (e.origin == EdgeOrigin::Hypothetical);
      path.contains_contradiction |= (e.role == EdgeRole::Contradicts);
      path.score *= std::max(0.0, std::min(1.0, e.confidence));
      cur = parent_node.at(cur);
      path.nodes.push_back(cur);
    }
    return path;
  }
};

GrapheneDB::GrapheneDB() : impl_(new Impl()) {}
GrapheneDB::~GrapheneDB() { close(); }

Status GrapheneDB::open(const fs::path& path, const DBOptions& options) {
  std::unique_lock lock(impl_->mu);
  if (impl_->open) return Status::ok();
  if (options.dimension == 0) return Status::error(ErrorCode::InvalidOption, "dimension must be greater than zero");
  impl_->dir = path;
  impl_->wal_path = path / "graphene.wal";
  impl_->data_path = path / "graphene.data";
  impl_->manifest_path = path / "MANIFEST";
  impl_->lock_path = path / "LOCK";
  impl_->opt = options;
  std::error_code ec;
  fs::create_directories(path, ec);
  if (ec) return Status::error(ErrorCode::IoError, "cannot create DB dir: " + ec.message());
  uint32_t manifest_dim = 0;
  uint64_t manifest_txid = 1;
  auto mst = impl_->read_manifest(&manifest_dim, &manifest_txid);
  if (!mst) return mst;
  if (manifest_dim != 0 && manifest_dim != options.dimension) {
    return Status::error(ErrorCode::DimensionMismatch, "existing DB dimension " + std::to_string(manifest_dim) + " does not match requested " + std::to_string(options.dimension));
  }
  auto lst = impl_->acquire_lock();
  if (!lst) return lst;
  impl_->nodes.clear(); impl_->edges.clear(); impl_->version = 1; impl_->txid = std::max<uint64_t>(1, manifest_txid); impl_->live_node_count = 0; impl_->live_edge_count = 0; impl_->vector_index.reset();
  auto dst = impl_->load_framed_file(impl_->data_path, false, false);
  if (!dst) { impl_->release_lock(); return dst; }
  auto wst = impl_->load_framed_file(impl_->wal_path, true, true);
  if (!wst) { impl_->release_lock(); return wst; }
  impl_->rebuild_indexes();
  auto ow = impl_->open_wal_fd_unlocked();
  if (!ow) { impl_->release_lock(); return ow; }
  auto wm = impl_->write_manifest();
  if (!wm) { impl_->close_wal_fd_unlocked(); impl_->release_lock(); return wm; }
  impl_->open = true;
  return Status::ok();
}

Status GrapheneDB::close() {
  std::unique_lock lock(impl_->mu);
  if (!impl_->open) return Status::ok();
  auto st = impl_->write_manifest();
  impl_->close_wal_fd_unlocked();
  impl_->open = false;
  impl_->release_lock();
  return st;
}

bool GrapheneDB::is_open() const { std::shared_lock lock(impl_->mu); return impl_->open; }
uint64_t GrapheneDB::snapshot() const { std::shared_lock lock(impl_->mu); return impl_->version ? impl_->version - 1 : 0; }
uint32_t GrapheneDB::dimension() const { std::shared_lock lock(impl_->mu); return impl_->opt.dimension; }

Status GrapheneDB::put_node(const NodeInput& input, uint32_t* out_id) {
  std::unique_lock lock(impl_->mu);
  if (!impl_->open) return Status::error(ErrorCode::NotOpen, "database is not open");
  std::string reason;
  if (!impl_->vector_valid(input.vector, &reason)) return Status::error(ErrorCode::DimensionMismatch, reason);
  if (input.content.size() > 16 * 1024 * 1024) return Status::error(ErrorCode::InvalidInput, "content too large");
  Node n;
  n.id = impl_->next_node_id++;
  n.content = input.content;
  n.vector = input.vector;
  n.signature = input.signature;
  n.incident = input.incident;
  n.root = input.root; n.symptom = input.symptom; n.impact = input.impact;
  n.created_version = impl_->version++;
  n.metadata = input.metadata;
  uint64_t tx = impl_->txid++;
  std::vector<std::string> frames = {
    "BEGIN\t" + std::to_string(tx),
    "PUT_NODE\t" + std::to_string(tx) + "\t" + impl_->node_payload(n, "PUT_NODE").substr(std::string("PUT_NODE\t").size()),
    "COMMIT\t" + std::to_string(tx)
  };
  auto st = impl_->append_wal_unlocked(frames);
  if (!st) return st;
  impl_->ensure_node_slot(n.id);
  impl_->nodes[n.id] = n;
  impl_->planes[n.signature].push_back(n.id);
  impl_->index_node_metadata(n);
  ++impl_->live_node_count;
  if (impl_->vector_index) { auto vst = impl_->vector_index->add(n.id, n.vector); if (!vst) return vst; }
  auto rst = impl_->maybe_rotate_wal_unlocked();
  if (!rst) return rst;
  if (out_id) *out_id = n.id;
  return Status::ok();
}

Status GrapheneDB::put_edge(const EdgeInput& input, uint32_t* out_id) {
  std::unique_lock lock(impl_->mu);
  if (!impl_->open) return Status::error(ErrorCode::NotOpen, "database is not open");
  uint64_t snap = impl_->version - 1;
  if (input.from >= impl_->nodes.size() || input.to >= impl_->nodes.size() || !visible_node(impl_->nodes[input.from], snap) || !visible_node(impl_->nodes[input.to], snap)) {
    return Status::error(ErrorCode::EdgeInvalid, "edge endpoints must exist and be visible");
  }
  if (!std::isfinite(input.confidence) || input.confidence < 0.0 || input.confidence > 1.0) return Status::error(ErrorCode::InvalidInput, "confidence must be between 0 and 1");
  Edge e;
  e.id = impl_->next_edge_id++;
  e.from = input.from; e.to = input.to; e.origin = input.origin; e.role = input.role; e.confidence = input.confidence;
  e.created_version = impl_->version++;
  e.metadata = input.metadata;
  uint64_t tx = impl_->txid++;
  std::vector<std::string> frames = {"BEGIN\t" + std::to_string(tx), "PUT_EDGE\t" + std::to_string(tx) + "\t" + impl_->edge_payload(e, "PUT_EDGE").substr(std::string("PUT_EDGE\t").size()), "COMMIT\t" + std::to_string(tx)};
  auto st = impl_->append_wal_unlocked(frames);
  if (!st) return st;
  impl_->ensure_edge_slot(e.id);
  impl_->edges[e.id] = e;
  impl_->out_edges[e.from].push_back(e.id);
  impl_->in_edges[e.to].push_back(e.id);
  ++impl_->live_edge_count;
  auto rst = impl_->maybe_rotate_wal_unlocked();
  if (!rst) return rst;
  if (out_id) *out_id = e.id;
  return Status::ok();
}

Status GrapheneDB::delete_node(uint32_t id) {
  std::unique_lock lock(impl_->mu);
  if (!impl_->open) return Status::error(ErrorCode::NotOpen, "database is not open");
  uint64_t snap = impl_->version - 1;
  if (id >= impl_->nodes.size() || !visible_node(impl_->nodes[id], snap)) return Status::error(ErrorCode::NodeNotFound, "node not found or already deleted");
  uint64_t delver = impl_->version++;
  uint64_t tx = impl_->txid++;
  std::vector<std::string> frames = {"BEGIN\t" + std::to_string(tx), "DELETE_NODE\t" + std::to_string(tx) + "\t" + std::to_string(id) + "\t" + std::to_string(delver), "COMMIT\t" + std::to_string(tx)};
  auto st = impl_->append_wal_unlocked(frames);
  if (!st) return st;
  std::unordered_set<uint32_t> affected_edges;
  auto in_it = impl_->in_edges.find(id);
  if (in_it != impl_->in_edges.end()) affected_edges.insert(in_it->second.begin(), in_it->second.end());
  auto out_it = impl_->out_edges.find(id);
  if (out_it != impl_->out_edges.end()) affected_edges.insert(out_it->second.begin(), out_it->second.end());
  uint64_t before_snap = delver - 1;
  size_t hidden_edges = 0;
  for (uint32_t eid : affected_edges) {
    if (eid < impl_->edges.size() && visible_edge(impl_->edges[eid], impl_->nodes, before_snap)) ++hidden_edges;
  }
  impl_->nodes[id].deleted_version = delver;
  if (impl_->live_node_count > 0) --impl_->live_node_count;
  impl_->live_edge_count = hidden_edges > impl_->live_edge_count ? 0 : impl_->live_edge_count - hidden_edges;
  if (impl_->vector_index) (void)impl_->vector_index->remove(id);
  auto rst = impl_->maybe_rotate_wal_unlocked();
  if (!rst) return rst;
  return Status::ok();
}

size_t GrapheneDB::node_count(uint64_t snap) const {
  std::shared_lock lock(impl_->mu);
  uint64_t current = impl_->version - 1;
  if (snap == kInfVersion || snap == current) return impl_->live_node_count;
  size_t c = 0; for (const auto& n : impl_->nodes) if (visible_node(n, snap)) ++c; return c;
}

size_t GrapheneDB::edge_count(uint64_t snap) const {
  std::shared_lock lock(impl_->mu);
  uint64_t current = impl_->version - 1;
  if (snap == kInfVersion || snap == current) return impl_->live_edge_count;
  size_t c = 0; for (const auto& e : impl_->edges) if (visible_edge(e, impl_->nodes, snap)) ++c; return c;
}

std::vector<uint32_t> GrapheneDB::metadata_search(const std::string& key, const std::string& value, uint64_t snap) const {
  std::shared_lock lock(impl_->mu);
  if (snap == kInfVersion) snap = impl_->version - 1;
  std::vector<uint32_t> out;
  auto it = impl_->metadata_index.find(key + "\x1f" + value);
  if (it == impl_->metadata_index.end()) return out;
  std::unordered_set<uint32_t> seen;
  for (uint32_t id : it->second) {
    if (seen.insert(id).second && id < impl_->nodes.size() && visible_node(impl_->nodes[id], snap)) out.push_back(id);
  }
  return out;
}

std::optional<Node> GrapheneDB::get_node(uint32_t id, uint64_t snap) const {
  std::shared_lock lock(impl_->mu);
  if (snap == kInfVersion) snap = impl_->version - 1;
  if (id >= impl_->nodes.size() || !visible_node(impl_->nodes[id], snap)) return std::nullopt;
  return impl_->nodes[id];
}

std::optional<Edge> GrapheneDB::get_edge(uint32_t id, uint64_t snap) const {
  std::shared_lock lock(impl_->mu);
  if (snap == kInfVersion) snap = impl_->version - 1;
  if (id >= impl_->edges.size() || !visible_edge(impl_->edges[id], impl_->nodes, snap)) return std::nullopt;
  return impl_->edges[id];
}

std::vector<SearchResult> GrapheneDB::vector_search(const std::vector<float>& query, size_t k, uint64_t snap) const {
  std::shared_lock lock(impl_->mu);
  std::string reason;
  if (!impl_->vector_valid(query, &reason) || k == 0) return {};
  uint64_t current = impl_->version - 1;
  if (snap == kInfVersion) snap = current;
  if (snap == current && impl_->vector_index) {
    return impl_->vector_index->search(query, k);
  }
  std::vector<SearchResult> scored;
  for (const auto& n : impl_->nodes) {
    if (!visible_node(n, snap)) continue;
    double s = cosine(query, n.vector);
    if (s >= -0.5) scored.push_back({n.id, s});
  }
  size_t kk = std::min(k, scored.size());
  std::partial_sort(scored.begin(), scored.begin() + kk, scored.end(), [](auto& a, auto& b) { return a.score > b.score; });
  scored.resize(kk);
  return scored;
}

MemoryBundle GrapheneDB::causal_search(const std::vector<float>& query, uint64_t query_signature, QueryMode mode, uint64_t snap) const {
  std::shared_lock lock(impl_->mu);
  MemoryBundle out;
  std::string reason;
  if (!impl_->vector_valid(query, &reason)) { out.abstain = true; out.reason = "VECTOR_DIMENSION_MISMATCH: " + reason; return out; }
  if (snap == kInfVersion) snap = impl_->version - 1;
  out.snapshot_version = snap;
  std::vector<uint32_t> candidates;
  for (const auto& kv : impl_->planes) {
    int overlap = popcount64(kv.first & query_signature);
    int required = std::max(1, popcount64(query_signature) - 1);
    if (overlap >= required) {
      for (uint32_t id : kv.second) if (id < impl_->nodes.size() && visible_node(impl_->nodes[id], snap)) candidates.push_back(id);
    }
  }
  if (candidates.size() < impl_->opt.tuning.min_candidate_floor) {
    std::vector<SearchResult> fallback;
    for (const auto& n : impl_->nodes) if (visible_node(n, snap)) fallback.push_back({n.id, cosine(query, n.vector)});
    std::sort(fallback.begin(), fallback.end(), [](auto& a, auto& b) { return a.score > b.score; });
    for (size_t i = 0; i < std::min<size_t>(impl_->opt.tuning.min_candidate_floor, fallback.size()); ++i) candidates.push_back(fallback[i].node_id);
  }
  std::sort(candidates.begin(), candidates.end());
  candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
  std::vector<SearchResult> anchors;
  for (uint32_t id : candidates) {
    const auto& n = impl_->nodes[id];
    double s = cosine(query, n.vector) + ((n.symptom || n.impact) ? impl_->opt.tuning.symptom_or_impact_boost : 0.0);
    anchors.push_back({id, s});
  }
  std::sort(anchors.begin(), anchors.end(), [](auto& a, auto& b) { return a.score > b.score; });
  if (anchors.empty() || anchors.front().score < impl_->opt.tuning.min_anchor_score) { out.abstain = true; out.reason = "NO_STABLE_ANCHOR"; return out; }
  size_t inspect = std::min<size_t>(impl_->opt.tuning.max_anchor_inspect, anchors.size());
  std::map<uint32_t, int> incidents;
  for (size_t i = 0; i < inspect; ++i) incidents[impl_->nodes[anchors[i].node_id].incident]++;
  int max_inc = 0; for (auto& kv : incidents) max_inc = std::max(max_inc, kv.second);
  if (anchors.front().score < impl_->opt.tuning.high_confidence_anchor_score && static_cast<double>(max_inc) / inspect < impl_->opt.tuning.min_incident_concentration && mode != QueryMode::Theoretical) {
    out.abstain = true; out.reason = "AMBIGUOUS"; return out;
  }
  std::map<uint32_t, MemoryBundle> by_root;
  for (size_t i = 0; i < inspect; ++i) {
    Path p = impl_->reverse_root(anchors[i].node_id, mode, snap);
    if (p.nodes.empty()) continue;
    p.score *= anchors[i].score;
    uint32_t root = p.nodes.front();
    auto& b = by_root[root];
    b.target_node = root;
    b.paths.push_back(p);
    b.semantic_candidates.push_back(anchors[i].node_id);
  }
  if (by_root.empty()) { out.abstain = true; out.reason = "NO_PATH"; return out; }
  double best_score = -1.0;
  for (auto& kv : by_root) {
    auto& b = kv.second;
    std::set<uint32_t> unique_edges;
    int contradictions = 0;
    double sum = 0.0;
    for (const auto& p : b.paths) {
      unique_edges.insert(p.edges.begin(), p.edges.end());
      contradictions += p.contains_contradiction ? 1 : 0;
      sum += p.score;
    }
    b.degeneracy = static_cast<double>(b.paths.size());
    b.diversity = b.paths.empty() ? 0.0 : static_cast<double>(unique_edges.size()) / std::max<size_t>(1, b.paths.size());
    b.contradiction = b.paths.empty() ? 0.0 : static_cast<double>(contradictions) / b.paths.size();
    b.confidence = 0.35 * std::min(1.0, b.degeneracy / 3.0) + 0.25 * std::min(1.0, b.diversity / 4.0) - 0.25 * b.contradiction + sum / std::max<size_t>(1, b.paths.size());
    if (b.confidence > best_score) { best_score = b.confidence; out = b; }
  }
  out.snapshot_version = snap;
  if (out.confidence < impl_->opt.tuning.min_bundle_confidence && mode != QueryMode::Theoretical) { out.abstain = true; out.reason = "LOW_STABILITY"; }
  if (!out.abstain) {
    out.why_retrieved.push_back("semantic similarity to candidate memories");
    out.why_retrieved.push_back("signature-plane candidate reduction");
    out.why_retrieved.push_back("causal path from root memory to anchor memory");
    if (out.contradiction > 0.0) out.why_retrieved.push_back("contains contradiction path; confidence reduced");
  }
  return out;
}

Status GrapheneDB::compact() {
  std::unique_lock lock(impl_->mu);
  if (!impl_->open) return Status::error(ErrorCode::NotOpen, "database is not open");
  return impl_->checkpoint_unlocked();
}

Status GrapheneDB::backup(const fs::path& destination_dir) const {
  std::shared_lock lock(impl_->mu);
  if (!impl_->open) return Status::error(ErrorCode::NotOpen, "database is not open");
  std::error_code ec;
  fs::create_directories(destination_dir, ec);
  if (ec) return Status::error(ErrorCode::IoError, "cannot create backup dir: " + ec.message());
  for (const auto& src : {impl_->data_path, impl_->wal_path, impl_->manifest_path}) {
    if (fs::exists(src)) fs::copy_file(src, destination_dir / src.filename(), fs::copy_options::overwrite_existing, ec);
    if (ec) return Status::error(ErrorCode::IoError, "backup failed: " + ec.message());
  }
  return Status::ok();
}

Status GrapheneDB::inspect(std::string* out) const {
  std::shared_lock lock(impl_->mu);
  if (!out) return Status::error(ErrorCode::InvalidInput, "out cannot be null");
  std::ostringstream os;
  uint64_t snap = impl_->version - 1;
  os << "GrapheneDB v1\n";
  os << "path=" << impl_->dir.string() << "\n";
  os << "dimension=" << impl_->opt.dimension << "\n";
  os << "version=" << snap << "\n";
  os << "nodes_visible=" << impl_->live_node_count << "\n";
  os << "edges_visible=" << impl_->live_edge_count << "\n";
  os << "vector_index=" << (impl_->vector_index ? impl_->vector_index->name() : "none") << "\n";
  os << "planes=" << impl_->planes.size() << "\n";
  *out = os.str();
  return Status::ok();
}

Status GrapheneDB::validate(std::string* report) const {
  std::shared_lock lock(impl_->mu);
  std::ostringstream os;
  bool ok = true;
  if (impl_->opt.dimension == 0) { ok = false; os << "FAIL dimension zero\n"; }
  for (const auto& n : impl_->nodes) {
    if (n.created_version == kInfVersion) continue;
    if (n.vector.size() != impl_->opt.dimension) { ok = false; os << "FAIL node " << n.id << " dimension mismatch\n"; }
    for (float f : n.vector) if (!std::isfinite(f)) { ok = false; os << "FAIL node " << n.id << " non-finite vector\n"; }
  }
  for (const auto& e : impl_->edges) {
    if (e.created_version == kInfVersion) continue;
    if (e.from >= impl_->nodes.size() || e.to >= impl_->nodes.size()) { ok = false; os << "FAIL edge " << e.id << " missing endpoint\n"; }
  }
  if (ok) os << "OK\n";
  if (report) *report = os.str();
  return ok ? Status::ok() : Status::error(ErrorCode::DataCorrupt, "validation failed");
}

} // namespace graphene
