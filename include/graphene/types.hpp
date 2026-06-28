#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace graphene {

static constexpr uint64_t kInfVersion = UINT64_MAX;

enum class ErrorCode {
  Ok = 0,
  NotOpen,
  InvalidOption,
  IoError,
  WalCorrupt,
  DataCorrupt,
  TransactionIncomplete,
  DimensionMismatch,
  NodeNotFound,
  EdgeInvalid,
  InvalidInput,
  LockBusy,
  UnsupportedMode
};

struct Status {
  ErrorCode code{ErrorCode::Ok};
  std::string message{};
  static Status ok() { return {}; }
  static Status error(ErrorCode c, std::string m) { return {c, std::move(m)}; }
  explicit operator bool() const { return code == ErrorCode::Ok; }
};

enum class EdgeOrigin : uint8_t {
  Observed = 0,
  Discovered = 1,
  Inferred = 2,
  Reinforced = 3,
  Hypothetical = 4
};

enum class EdgeRole : uint8_t {
  Mechanistic = 0,
  Compressed = 1,
  Analogical = 2,
  Predictive = 3,
  Causal = 4,
  Contradicts = 5,
  Supports = 6,
  Supersedes = 7
};

enum class QueryMode : uint8_t {
  Empirical = 0,
  Balanced = 1,
  Theoretical = 2
};

struct RetrievalTuning {
  // RC defaults. These are explicit tuning values, not scientific constants.
  double min_anchor_score{0.42};
  double high_confidence_anchor_score{0.97};
  double min_incident_concentration{0.34};
  double min_bundle_confidence{0.45};
  double symptom_or_impact_boost{0.15};
  size_t max_reverse_bfs_visits{20000};
  size_t min_candidate_floor{64};
  size_t max_anchor_inspect{12};
};

struct DBOptions {
  uint32_t dimension{0};
  bool create_if_missing{true};
  bool fsync_on_commit{true};
  // If >0, committed WAL bytes above this threshold trigger a checkpoint/rotation.
  uint64_t wal_rotate_bytes{0};
  // If true, a LOCK file owned by a dead process is removed on open.
  bool recover_stale_lock{true};
  RetrievalTuning tuning{};
};

struct NodeInput {
  std::string content;
  std::vector<float> vector;
  uint64_t signature{0};
  uint32_t incident{0};
  bool root{false};
  bool symptom{false};
  bool impact{false};
  std::map<std::string, std::string> metadata;
};

struct Node {
  uint32_t id{0};
  std::string content;
  std::vector<float> vector;
  uint64_t signature{0};
  uint32_t incident{0};
  bool root{false};
  bool symptom{false};
  bool impact{false};
  uint64_t created_version{0};
  uint64_t deleted_version{kInfVersion};
  std::map<std::string, std::string> metadata;
};

struct EdgeInput {
  uint32_t from{0};
  uint32_t to{0};
  EdgeOrigin origin{EdgeOrigin::Observed};
  EdgeRole role{EdgeRole::Causal};
  double confidence{0.9};
  std::map<std::string, std::string> metadata;
};

struct Edge {
  uint32_t id{0};
  uint32_t from{0};
  uint32_t to{0};
  EdgeOrigin origin{EdgeOrigin::Observed};
  EdgeRole role{EdgeRole::Causal};
  double confidence{0.9};
  uint64_t created_version{0};
  uint64_t deleted_version{kInfVersion};
  std::map<std::string, std::string> metadata;
};

struct Path {
  std::vector<uint32_t> nodes;
  std::vector<uint32_t> edges;
  double score{1.0};
  bool contains_hypothetical{false};
  bool contains_contradiction{false};
};

struct MemoryBundle {
  bool abstain{false};
  std::string reason;
  uint32_t target_node{0};
  uint64_t snapshot_version{0};
  std::vector<Path> paths;
  std::vector<uint32_t> semantic_candidates;
  std::vector<std::string> why_retrieved;
  double confidence{0.0};
  double degeneracy{0.0};
  double diversity{0.0};
  double contradiction{0.0};
};

struct SearchResult {
  uint32_t node_id{0};
  double score{0.0};
};

inline uint64_t signature_for(uint32_t service, uint32_t symptom) {
  return (1ull << (service % 16)) | (1ull << (16 + (symptom % 16)));
}

} // namespace graphene
