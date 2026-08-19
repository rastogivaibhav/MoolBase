#include "graphene/db.hpp"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <vector>

using namespace graphene;
namespace fs = std::filesystem;

namespace {

struct ShadowRow {
  uint32_t node_id{0};
  std::vector<int16_t> values;
};

class Int16Shadow {
 public:
  void add(uint32_t node_id, const std::vector<float>& vector) {
    ShadowRow row;
    row.node_id = node_id;
    row.values = quantize(vector);
    std::unique_lock lock(mu_);
    rows_.push_back(std::move(row));
  }

  std::vector<ShadowRow> snapshot() const {
    std::shared_lock lock(mu_);
    return rows_;
  }

  static std::vector<int16_t> quantize(const std::vector<float>& vector) {
    std::vector<int16_t> out;
    out.reserve(vector.size());
    for (float value : vector) {
      const float bounded = std::clamp(value, -1.0f, 1.0f);
      const long scaled = std::lround(static_cast<double>(bounded) * 32767.0);
      out.push_back(static_cast<int16_t>(scaled));
    }
    return out;
  }

 private:
  mutable std::shared_mutex mu_;
  std::vector<ShadowRow> rows_;
};

fs::path fresh_dir(const std::string& name) {
  const auto suffix = std::to_string(
      std::chrono::steady_clock::now().time_since_epoch().count());
  auto path = fs::temp_directory_path() / (name + "_" + suffix);
  fs::remove_all(path);
  fs::create_directories(path);
  return path;
}

void require(Status status, const char* operation) {
  if (!status) {
    std::cerr << "FAIL " << operation << ": " << status.message << "\n";
    std::abort();
  }
}

std::vector<float> unit_vector(uint32_t dimension, uint32_t seed) {
  std::vector<float> out(dimension);
  double norm2 = 0.0;
  for (uint32_t i = 0; i < dimension; ++i) {
    const double a = std::sin((static_cast<double>(seed) + 1.0) *
                              (static_cast<double>(i) + 1.0) * 0.173);
    const double b = std::cos((static_cast<double>(seed) + 3.0) *
                              (static_cast<double>(i) + 1.0) * 0.071);
    out[i] = static_cast<float>(0.62 * a + 0.38 * b);
    norm2 += static_cast<double>(out[i]) * out[i];
  }
  const double norm = std::sqrt(norm2);
  assert(norm > 0.0);
  for (float& value : out) value = static_cast<float>(value / norm);
  return out;
}

double cosine(const std::vector<float>& a, const std::vector<float>& b) {
  assert(a.size() == b.size());
  double dot = 0.0;
  double na = 0.0;
  double nb = 0.0;
  for (size_t i = 0; i < a.size(); ++i) {
    dot += static_cast<double>(a[i]) * b[i];
    na += static_cast<double>(a[i]) * a[i];
    nb += static_cast<double>(b[i]) * b[i];
  }
  if (na == 0.0 || nb == 0.0) return -1.0;
  return dot / (std::sqrt(na) * std::sqrt(nb));
}

std::vector<uint32_t> shortlist(const std::vector<ShadowRow>& rows,
                                const std::vector<float>& query,
                                size_t k) {
  const auto q = Int16Shadow::quantize(query);
  struct Candidate {
    uint32_t node_id;
    int64_t score;
  };
  std::vector<Candidate> candidates;
  candidates.reserve(rows.size());
  for (const auto& row : rows) {
    assert(row.values.size() == q.size());
    int64_t dot = 0;
    for (size_t i = 0; i < q.size(); ++i) {
      dot += static_cast<int64_t>(q[i]) * row.values[i];
    }
    candidates.push_back({row.node_id, dot});
  }
  std::stable_sort(candidates.begin(), candidates.end(),
                   [](const Candidate& a, const Candidate& b) {
                     if (a.score != b.score) return a.score > b.score;
                     return a.node_id < b.node_id;
                   });
  k = std::min(k, candidates.size());
  std::vector<uint32_t> out;
  out.reserve(k);
  for (size_t i = 0; i < k; ++i) out.push_back(candidates[i].node_id);
  return out;
}

uint32_t exact_best(GrapheneDB& db,
                    const std::vector<uint32_t>& ids,
                    const std::vector<float>& query) {
  assert(!ids.empty());
  uint32_t best_id = ids.front();
  double best_score = -std::numeric_limits<double>::infinity();
  for (uint32_t id : ids) {
    const auto node = db.get_node(id);
    assert(node.has_value());
    const double score = cosine(query, node->vector);
    if (score > best_score + 1e-12 ||
        (std::abs(score - best_score) <= 1e-12 && id < best_id)) {
      best_id = id;
      best_score = score;
    }
  }
  return best_id;
}

std::vector<uint32_t> row_ids(const std::vector<ShadowRow>& rows) {
  std::vector<uint32_t> ids;
  ids.reserve(rows.size());
  for (const auto& row : rows) ids.push_back(row.node_id);
  return ids;
}

uint64_t shadow_fingerprint(const std::vector<ShadowRow>& rows) {
  uint64_t hash = 1469598103934665603ull;
  auto mix_byte = [&](uint8_t byte) {
    hash ^= byte;
    hash *= 1099511628211ull;
  };
  for (const auto& row : rows) {
    for (unsigned shift = 0; shift < 32; shift += 8) {
      mix_byte(static_cast<uint8_t>((row.node_id >> shift) & 0xffu));
    }
    for (int16_t value : row.values) {
      const uint16_t bits = static_cast<uint16_t>(value);
      mix_byte(static_cast<uint8_t>(bits & 0xffu));
      mix_byte(static_cast<uint8_t>((bits >> 8) & 0xffu));
    }
  }
  return hash;
}

bool contains(const std::vector<uint32_t>& values, uint32_t target) {
  return std::find(values.begin(), values.end(), target) != values.end();
}

void test_incomplete_causal_hex_recovery() {
  const auto dir = fresh_dir("graphenedb_pi34_hex_recovery");
  DBOptions options;
  options.dimension = 8;
  options.fsync_on_commit = false;
  options.require_lattice = true;
  options.physical_lattice_storage = true;
  options.tuning.enable_lattice_retrieval = true;
  options.tuning.lattice_max_hops = 1;

  GrapheneDB db;
  require(db.open(dir, options), "open hex recovery db");

  NodeInput root;
  root.content = "root";
  root.vector = unit_vector(options.dimension, 1);
  root.signature = 11;
  root.incident = 77;
  root.root = true;
  root.lattice = LatticeCoord{0, 0, 0};

  NodeInput symptom;
  symptom.content = "symptom";
  symptom.vector = unit_vector(options.dimension, 2);
  symptom.signature = 12;
  symptom.incident = 77;
  symptom.symptom = true;
  symptom.lattice = LatticeCoord{1, 0, 0};

  NodeInput impact;
  impact.content = "impact-with-missing-causal-edge";
  impact.vector = unit_vector(options.dimension, 3);
  impact.signature = 13;
  impact.incident = 77;
  impact.impact = true;
  impact.lattice = LatticeCoord{1, -1, 0};

  uint32_t root_id = 0;
  uint32_t symptom_id = 0;
  uint32_t impact_id = 0;
  require(db.put_node(root, &root_id), "put root");
  require(db.put_node(symptom, &symptom_id), "put symptom");
  require(db.put_node(impact, &impact_id), "put impact");

  EdgeInput observed;
  observed.from = root_id;
  observed.to = symptom_id;
  observed.origin = EdgeOrigin::Observed;
  observed.role = EdgeRole::Causal;
  observed.confidence = 0.95;
  require(db.put_edge(observed), "put only explicit causal edge");

  // Intentionally do NOT write symptom -> impact. PI3.4 proves that physical
  // Hex topology remains recoverable after reopen without inventing a causal edge.
  assert(db.outgoing_edges(symptom_id).empty());
  auto before = db.lattice_neighbors(symptom_id, 1);
  assert(contains(before, root_id));
  assert(contains(before, impact_id));
  require(db.close(), "close hex recovery db");

  GrapheneDB reopened;
  require(reopened.open(dir, options), "reopen hex recovery db");
  auto after = reopened.lattice_neighbors(symptom_id, 1);
  assert(contains(after, root_id));
  assert(contains(after, impact_id));
  assert(reopened.outgoing_edges(symptom_id).empty());
  std::string report;
  require(reopened.validate(&report), "validate reopened hex recovery db");
  require(reopened.close(), "close reopened hex recovery db");
  fs::remove_all(dir);
}

struct ConcurrencyResult {
  uint64_t verified_reads{0};
  uint64_t failures{0};
  uint64_t final_nodes{0};
  uint64_t final_fingerprint{0};
};

ConcurrencyResult test_concurrent_shadow_and_rebuild() {
  constexpr uint32_t kDimension = 32;
  constexpr uint32_t kNodes = 400;
  constexpr size_t kShortlist = 24;
  constexpr uint32_t kReaders = 4;
  constexpr uint32_t kMinReadsPerReader = 100;

  const auto dir = fresh_dir("graphenedb_pi34_shadow_stress");
  DBOptions options;
  options.dimension = kDimension;
  options.fsync_on_commit = false;
  options.vector_index_kind = VectorIndexKind::Flat;

  GrapheneDB db;
  require(db.open(dir, options), "open shadow stress db");
  Int16Shadow shadow;
  std::atomic<bool> writer_done{false};
  std::atomic<uint64_t> verified_reads{0};
  std::atomic<uint64_t> failures{0};

  std::thread writer([&] {
    for (uint32_t i = 0; i < kNodes; ++i) {
      NodeInput node;
      node.content = "pi34-shadow-node-" + std::to_string(i);
      node.vector = unit_vector(kDimension, 1000 + i);
      node.signature = 10000 + i;
      node.incident = i / 4;
      uint32_t id = 0;
      auto status = db.put_node(node, &id);
      if (!status) {
        ++failures;
        break;
      }
      // The durable DB write is authoritative. The int16 structure is only a
      // rebuildable acceleration shadow published after commit.
      shadow.add(id, node.vector);
      if ((i % 16) == 0) std::this_thread::yield();
    }
    writer_done.store(true, std::memory_order_release);
  });

  std::vector<std::thread> readers;
  readers.reserve(kReaders);
  for (uint32_t reader_id = 0; reader_id < kReaders; ++reader_id) {
    readers.emplace_back([&, reader_id] {
      uint32_t local_reads = 0;
      while (!writer_done.load(std::memory_order_acquire) ||
             local_reads < kMinReadsPerReader) {
        const auto rows = shadow.snapshot();
        if (rows.empty()) {
          std::this_thread::yield();
          continue;
        }
        const auto ids = row_ids(rows);
        const size_t target_pos =
            (static_cast<size_t>(local_reads) * 17 + reader_id * 13) % rows.size();
        const auto target = db.get_node(rows[target_pos].node_id);
        if (!target) {
          ++failures;
          ++local_reads;
          continue;
        }
        const auto candidates = shortlist(rows, target->vector, kShortlist);
        const uint32_t authoritative_best = exact_best(db, ids, target->vector);
        if (!contains(candidates, authoritative_best)) {
          ++failures;
        } else {
          const uint32_t verified_best = exact_best(db, candidates, target->vector);
          if (verified_best != authoritative_best) ++failures;
        }
        ++local_reads;
        ++verified_reads;
        if ((local_reads % 8) == 0) std::this_thread::yield();
      }
    });
  }

  writer.join();
  for (auto& reader : readers) reader.join();

  assert(failures.load() == 0);
  assert(verified_reads.load() >=
         static_cast<uint64_t>(kReaders) * kMinReadsPerReader);
  assert(db.node_count() == kNodes);

  // Stable-state exact verification against GrapheneDB's own Flat result.
  const auto stable_rows = shadow.snapshot();
  assert(stable_rows.size() == kNodes);
  const auto stable_ids = row_ids(stable_rows);
  for (uint32_t i = 0; i < 80; ++i) {
    const auto target = db.get_node(stable_rows[(i * 29) % stable_rows.size()].node_id);
    assert(target.has_value());
    const auto candidates = shortlist(stable_rows, target->vector, kShortlist);
    const uint32_t authoritative_best = exact_best(db, stable_ids, target->vector);
    assert(contains(candidates, authoritative_best));
    assert(exact_best(db, candidates, target->vector) == authoritative_best);
    const auto engine = db.vector_search(target->vector, 1);
    assert(!engine.empty());
    assert(engine.front().node_id == authoritative_best);
  }

  const uint64_t fingerprint_before = shadow_fingerprint(stable_rows);
  std::vector<std::vector<uint32_t>> rankings_before;
  for (uint32_t i = 0; i < 32; ++i) {
    const auto target = db.get_node(stable_rows[(i * 11) % stable_rows.size()].node_id);
    assert(target.has_value());
    rankings_before.push_back(shortlist(stable_rows, target->vector, kShortlist));
  }

  require(db.close(), "close shadow stress db");

  GrapheneDB reopened;
  require(reopened.open(dir, options), "reopen shadow stress db");
  assert(reopened.node_count() == kNodes);

  Int16Shadow rebuilt;
  for (uint32_t id = 0; id < kNodes; ++id) {
    const auto node = reopened.get_node(id);
    assert(node.has_value());
    rebuilt.add(id, node->vector);
  }
  const auto rebuilt_rows = rebuilt.snapshot();
  const uint64_t fingerprint_after = shadow_fingerprint(rebuilt_rows);
  assert(fingerprint_after == fingerprint_before);

  for (uint32_t i = 0; i < 32; ++i) {
    const auto target = reopened.get_node(rebuilt_rows[(i * 11) % rebuilt_rows.size()].node_id);
    assert(target.has_value());
    assert(shortlist(rebuilt_rows, target->vector, kShortlist) == rankings_before[i]);
  }

  // PI3.4 must not create a durable int16/shadow format. The only source of
  // truth after reopen is the authoritative GrapheneDB node vector payload.
  for (const auto& entry : fs::directory_iterator(dir)) {
    std::string name = entry.path().filename().string();
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    assert(name.find("int16") == std::string::npos);
    assert(name.find("shadow") == std::string::npos);
  }

  std::string report;
  require(reopened.validate(&report), "validate rebuilt shadow db");
  require(reopened.close(), "close rebuilt shadow db");
  fs::remove_all(dir);

  return {verified_reads.load(), failures.load(), kNodes, fingerprint_after};
}

}  // namespace

int main() {
  test_incomplete_causal_hex_recovery();
  const auto concurrency = test_concurrent_shadow_and_rebuild();

  std::cout << "pi34_promotion_readiness_passed=true\n";
  std::cout << "pi34_hex_incomplete_causal_recovery=true\n";
  std::cout << "pi34_int16_shadow_concurrent_exact_verify=true\n";
  std::cout << "pi34_rebuild_after_reopen_deterministic=true\n";
  std::cout << "pi34_durable_shadow_format_added=false\n";
  std::cout << "pi34_verified_reads=" << concurrency.verified_reads << "\n";
  std::cout << "pi34_failures=" << concurrency.failures << "\n";
  std::cout << "pi34_nodes=" << concurrency.final_nodes << "\n";
  std::cout << "pi34_shadow_fingerprint=" << concurrency.final_fingerprint << "\n";
  return 0;
}
