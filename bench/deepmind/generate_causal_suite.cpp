#include "graphene/dialectic.hpp"
#include "graphene/db.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace graphene;
namespace fs = std::filesystem;

namespace {

constexpr uint32_t kDimension = 16;
constexpr const char* kAsOf = "2026-01-15T00:00:00Z";

struct Options {
  fs::path output_dir{"reports/deepmind-test/d0-latest"};
  uint64_t seed{20260724};
  size_t graphs{5000};
  size_t queries_per_graph{4};
  double wall_clock_seconds{900.0};
  double per_query_ms{250.0};
  bool formal{false};
  bool resume{false};
  bool keep_db{false};
  bool prepare_only{false};
};

using PathKey = std::vector<uint32_t>;

struct QueryTruth {
  std::string query_id;
  size_t graph_index{0};
  std::vector<float> query;
  uint64_t signature{0};
  std::set<uint32_t> roots;
  std::map<uint32_t, std::set<PathKey>> paths;
  std::set<uint32_t> forbidden_edges;
  std::set<uint32_t> false_promotion_edges;
  std::set<uint32_t> temporal_violation_edges;
  std::set<uint32_t> hyperedge_violation_edges;
  std::map<uint32_t, std::set<std::string>> provenance_findings;
  bool expect_contradiction{false};
  bool should_abstain{false};
};

struct Coverage {
  size_t graphs{0};
  size_t queries{0};
  size_t min_nodes{static_cast<size_t>(-1)};
  size_t max_nodes{0};
  size_t min_roots{static_cast<size_t>(-1)};
  size_t max_roots{0};
  size_t multi_root_graphs{0};
  size_t contradiction_graphs{0};
  size_t temporal_graphs{0};
  size_t hyperedge_graphs{0};
  size_t no_evidence_graphs{0};
  size_t chain_graphs{0};
  size_t fork_graphs{0};
  size_t join_graphs{0};
  size_t diamond_graphs{0};
  size_t cycle_graphs{0};
  size_t disconnected_graphs{0};
  size_t unique_query_vectors{0};
};

struct EpisodeScore {
  uint64_t expected_roots{0};
  uint64_t matched_roots{0};
  uint64_t expected_paths{0};
  uint64_t matched_paths{0};
  uint64_t returned_paths{0};
  uint64_t inadmissible_paths{0};
  uint64_t false_promotions{0};
  uint64_t contradiction_expected{0};
  uint64_t contradiction_detected{0};
  uint64_t abstention_expected{0};
  uint64_t abstention_correct{0};
  uint64_t temporal_violations{0};
  uint64_t hyperedge_violations{0};
  uint64_t provenance_mismatches{0};
  uint64_t budget_violations{0};
  double latency_ms{0.0};
};

struct Aggregate {
  uint64_t completed{0};
  uint64_t expected_roots{0};
  uint64_t matched_roots{0};
  uint64_t expected_paths{0};
  uint64_t matched_paths{0};
  uint64_t returned_paths{0};
  uint64_t inadmissible_paths{0};
  uint64_t false_promotions{0};
  uint64_t contradiction_expected{0};
  uint64_t contradiction_detected{0};
  uint64_t abstention_expected{0};
  uint64_t abstention_correct{0};
  uint64_t temporal_violations{0};
  uint64_t hyperedge_violations{0};
  uint64_t provenance_mismatches{0};
  uint64_t budget_violations{0};
  double latency_total_ms{0.0};
  double latency_max_ms{0.0};
};

[[noreturn]] void fail(const std::string& message) {
  throw std::runtime_error(message);
}

void require(Status status, const std::string& operation) {
  if (!status) fail(operation + ": " + status.message);
}

uint64_t parse_u64(const std::string& value, const char* option) {
  size_t used = 0;
  const uint64_t parsed = std::stoull(value, &used);
  if (used != value.size()) fail(std::string("invalid ") + option);
  return parsed;
}

double parse_double(const std::string& value, const char* option) {
  size_t used = 0;
  const double parsed = std::stod(value, &used);
  if (used != value.size() || !std::isfinite(parsed) || parsed <= 0.0) {
    fail(std::string("invalid ") + option);
  }
  return parsed;
}

Options parse_options(int argc, char** argv) {
  Options options;
  for (int index = 1; index < argc; ++index) {
    const std::string arg = argv[index];
    auto value = [&](const char* name) {
      if (++index >= argc) fail(std::string("missing value for ") + name);
      return std::string(argv[index]);
    };
    if (arg == "--output-dir") {
      options.output_dir = value("--output-dir");
    } else if (arg == "--seed") {
      options.seed = parse_u64(value("--seed"), "--seed");
    } else if (arg == "--graphs") {
      options.graphs =
          static_cast<size_t>(parse_u64(value("--graphs"), "--graphs"));
    } else if (arg == "--queries-per-graph") {
      options.queries_per_graph = static_cast<size_t>(
          parse_u64(value("--queries-per-graph"), "--queries-per-graph"));
    } else if (arg == "--wall-clock-seconds") {
      options.wall_clock_seconds =
          parse_double(value("--wall-clock-seconds"), "--wall-clock-seconds");
    } else if (arg == "--per-query-ms") {
      options.per_query_ms =
          parse_double(value("--per-query-ms"), "--per-query-ms");
    } else if (arg == "--formal") {
      options.formal = true;
    } else if (arg == "--resume") {
      options.resume = true;
    } else if (arg == "--keep-db") {
      options.keep_db = true;
    } else if (arg == "--prepare-only") {
      options.prepare_only = true;
    } else if (arg == "--help") {
      std::cout
          << "Usage: graphenedb_deepmind_causal_suite [options]\n"
          << "  --output-dir PATH          evidence directory\n"
          << "  --seed N                   explicit deterministic seed\n"
          << "  --graphs N                 graph count (formal minimum 5000)\n"
          << "  --queries-per-graph N      formal value 4\n"
          << "  --wall-clock-seconds N     whole-run budget\n"
          << "  --per-query-ms N           per-query budget\n"
          << "  --formal                   enforce D0 and G2 gates\n"
          << "  --resume                   skip completed episode ids\n"
          << "  --keep-db                  retain generated database\n"
          << "  --prepare-only             write pinned D0 truth without evaluating\n";
      std::exit(0);
    } else {
      fail("unknown option: " + arg);
    }
  }
  if (options.graphs == 0 || options.queries_per_graph == 0) {
    fail("graphs and queries-per-graph must be positive");
  }
  return options;
}

std::string json_escape(const std::string& value) {
  std::ostringstream out;
  for (unsigned char ch : value) {
    switch (ch) {
      case '"': out << "\\\""; break;
      case '\\': out << "\\\\"; break;
      case '\b': out << "\\b"; break;
      case '\f': out << "\\f"; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default:
        if (ch < 0x20) {
          out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
              << static_cast<int>(ch) << std::dec;
        } else {
          out << static_cast<char>(ch);
        }
    }
  }
  return out.str();
}

template <typename T>
std::string json_array(const std::vector<T>& values) {
  std::ostringstream out;
  out << "[";
  for (size_t index = 0; index < values.size(); ++index) {
    if (index) out << ",";
    out << values[index];
  }
  out << "]";
  return out.str();
}

std::string json_float_array(const std::vector<float>& values) {
  std::ostringstream out;
  out << "[" << std::setprecision(9);
  for (size_t index = 0; index < values.size(); ++index) {
    if (index) out << ",";
    out << values[index];
  }
  out << "]";
  return out.str();
}

template <typename T>
std::vector<T> as_vector(const std::set<T>& values) {
  return {values.begin(), values.end()};
}

PathKey canonical_path(std::vector<uint32_t> edges) {
  std::sort(edges.begin(), edges.end());
  return edges;
}

uint64_t splitmix64(uint64_t value) {
  value += 0x9e3779b97f4a7c15ULL;
  value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
  value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
  return value ^ (value >> 31U);
}

std::vector<float> vector_for(uint64_t seed, uint64_t identity) {
  std::vector<float> result(kDimension);
  double norm = 0.0;
  uint64_t state = seed ^ (identity * 0x9e3779b97f4a7c15ULL);
  for (float& value : result) {
    state = splitmix64(state);
    const double unit =
        static_cast<double>(state >> 11U) / static_cast<double>(1ULL << 53U);
    value = static_cast<float>(2.0 * unit - 1.0);
    norm += static_cast<double>(value) * static_cast<double>(value);
  }
  norm = std::sqrt(norm);
  for (float& value : result) value = static_cast<float>(value / norm);
  return result;
}

uint64_t signature_for_graph(size_t graph_index) {
  return signature_for(static_cast<uint32_t>(graph_index % 16),
                       static_cast<uint32_t>((graph_index / 16) % 16));
}

std::map<std::string, std::string> evidence(size_t graph_index,
                                            const std::string& suffix) {
  return {{"source_id", "d0-" + std::to_string(graph_index) + "-" + suffix},
          {"observed_at", "2026-01-14T12:00:00Z"}};
}

struct Builder {
  BatchInput batch;
  std::vector<QueryTruth> queries;
  Coverage coverage;
  uint64_t seed{0};
  std::set<std::vector<float>> unique_queries;

  uint32_t add_node(size_t graph,
                    const std::string& name,
                    bool root,
                    const std::vector<float>* fixed_vector = nullptr) {
    NodeInput node;
    const uint32_t id = static_cast<uint32_t>(batch.nodes.size());
    node.content = "D0 graph " + std::to_string(graph) + " " + name;
    node.vector =
        fixed_vector ? *fixed_vector : vector_for(seed, 1'000'000ULL + id);
    node.signature = signature_for_graph(graph);
    node.incident = static_cast<uint32_t>(graph + 1);
    node.root = root;
    node.metadata["corpus"] = "deepmind-d0";
    node.metadata["graph_id"] = std::to_string(graph);
    batch.nodes.push_back(std::move(node));
    return id;
  }

  uint32_t add_edge(uint32_t from,
                    uint32_t to,
                    EdgeOrigin origin,
                    EdgeRole role,
                    double confidence,
                    std::map<std::string, std::string> metadata) {
    const uint32_t id = static_cast<uint32_t>(batch.edges.size());
    batch.edges.push_back(
        {from, to, origin, role, confidence, std::move(metadata)});
    return id;
  }

  void expect_path(QueryTruth* truth,
                   uint32_t root,
                   std::vector<uint32_t> edges) {
    truth->roots.insert(root);
    truth->paths[root].insert(canonical_path(std::move(edges)));
  }

  void add_queries(size_t graph, QueryTruth truth, size_t count) {
    truth.graph_index = graph;
    truth.signature = signature_for_graph(graph);
    for (size_t query = 0; query < count; ++query) {
      QueryTruth instance = truth;
      instance.query_id =
          "g" + std::to_string(graph) + "-q" + std::to_string(query);
      if (query > 0) {
        const std::vector<float> noise =
            vector_for(seed, 20'000'000ULL + graph * 8ULL + query);
        double norm = 0.0;
        for (size_t dimension = 0; dimension < instance.query.size();
             ++dimension) {
          instance.query[dimension] +=
              static_cast<float>(0.001 * static_cast<double>(query) *
                                 noise[dimension]);
          norm += static_cast<double>(instance.query[dimension]) *
                  static_cast<double>(instance.query[dimension]);
        }
        norm = std::sqrt(norm);
        for (float& value : instance.query) {
          value = static_cast<float>(value / norm);
        }
      }
      unique_queries.insert(instance.query);
      queries.push_back(std::move(instance));
    }
    coverage.unique_query_vectors = unique_queries.size();
  }

  void build_graph(size_t graph, size_t query_count) {
    const size_t node_start = batch.nodes.size();
    size_t declared_roots = 0;
    QueryTruth truth;
    const std::vector<float> anchor_vector =
        vector_for(seed, 10'000'000ULL + graph);
    const size_t shape = graph % 5;

    if (shape == 0) {
      ++coverage.chain_graphs;
      ++coverage.contradiction_graphs;
      const uint32_t root = add_node(graph, "root", true);
      ++declared_roots;
      uint32_t anchor = 0;
      if (graph == 0) {
        anchor = add_node(graph, "anchor", false, &anchor_vector);
        const uint32_t causal =
            add_edge(root, anchor, EdgeOrigin::Observed, EdgeRole::Causal,
                     0.96, {});
        truth.provenance_findings[causal].insert("MISSING_EVIDENCE");
        const uint32_t contradiction =
            add_edge(root, anchor, EdgeOrigin::Discovered,
                     EdgeRole::Contradicts, 0.88,
                     evidence(graph, "contradiction"));
        expect_path(&truth, root, {causal});
        expect_path(&truth, root, {contradiction});
      } else {
        const uint32_t middle = add_node(graph, "chain-middle", false);
        anchor = add_node(graph, "anchor", false, &anchor_vector);
        const uint32_t first =
            add_edge(root, middle, EdgeOrigin::Observed,
                     EdgeRole::Mechanistic, 0.97, {});
        truth.provenance_findings[first].insert("MISSING_EVIDENCE");
        const uint32_t second =
            add_edge(middle, anchor, EdgeOrigin::Discovered,
                     EdgeRole::Causal, 0.96, evidence(graph, "chain"));
        const uint32_t contradiction =
            add_edge(root, anchor, EdgeOrigin::Observed,
                     EdgeRole::Contradicts, 0.87,
                     evidence(graph, "contradiction"));
        expect_path(&truth, root, {first, second});
        expect_path(&truth, root, {contradiction});
      }
      truth.query = anchor_vector;
      truth.expect_contradiction = true;
    } else if (shape == 1) {
      ++coverage.fork_graphs;
      ++coverage.multi_root_graphs;
      const uint32_t root_a = add_node(graph, "fork-root-a", true);
      const uint32_t root_b = add_node(graph, "fork-root-b", true);
      const uint32_t inferred = add_node(graph, "candidate-a", true);
      const uint32_t reinforced = add_node(graph, "candidate-b", true);
      const uint32_t hypothetical = add_node(graph, "candidate-c", true);
      const uint32_t analogical = add_node(graph, "candidate-d", true);
      declared_roots += 6;
      const uint32_t anchor =
          add_node(graph, "anchor", false, &anchor_vector);
      const double second_confidence = graph % 10 == 1 ? 0.91 : 0.9001;
      const uint32_t edge_a =
          add_edge(root_a, anchor, EdgeOrigin::Observed, EdgeRole::Causal,
                   0.91, evidence(graph, "fork-a"));
      const uint32_t edge_b =
          add_edge(root_b, anchor, EdgeOrigin::Discovered, EdgeRole::Causal,
                   second_confidence, evidence(graph, "fork-b"));
      expect_path(&truth, root_a, {edge_a});
      expect_path(&truth, root_b, {edge_b});

      const uint32_t inferred_edge =
          add_edge(inferred, anchor, EdgeOrigin::Inferred, EdgeRole::Causal,
                   0.999, {});
      const uint32_t reinforced_edge =
          add_edge(reinforced, anchor, EdgeOrigin::Reinforced,
                   EdgeRole::Predictive, 0.999,
                   {{"promotion_status", "discovered"}});
      const uint32_t hypothetical_edge =
          add_edge(hypothetical, anchor, EdgeOrigin::Hypothetical,
                   EdgeRole::Causal, 0.999, {});
      const uint32_t analogical_edge =
          add_edge(analogical, anchor, EdgeOrigin::Observed,
                   EdgeRole::Analogical, 0.999, evidence(graph, "analogy"));
      truth.forbidden_edges.insert(
          {inferred_edge, reinforced_edge, hypothetical_edge, analogical_edge});
      truth.false_promotion_edges.insert(
          {inferred_edge, reinforced_edge, hypothetical_edge});
      truth.provenance_findings[inferred_edge].insert(
          "INFERRED_WITHOUT_DERIVATION");
      truth.provenance_findings[reinforced_edge].insert(
          "REINFORCED_TRUTH_PROMOTION");
      truth.query = anchor_vector;
    } else if (shape == 2) {
      ++coverage.diamond_graphs;
      ++coverage.temporal_graphs;
      const uint32_t root = add_node(graph, "diamond-root", true);
      const uint32_t bad_compressed =
          add_node(graph, "shortcut-root", true);
      const uint32_t stale_root = add_node(graph, "stale-root", true);
      const uint32_t future_root = add_node(graph, "future-root", true);
      const uint32_t malformed_root =
          add_node(graph, "malformed-root", true);
      declared_roots += 5;
      const uint32_t left = add_node(graph, "diamond-left", false);
      const uint32_t right = add_node(graph, "diamond-right", false);
      const uint32_t anchor =
          add_node(graph, "anchor", false, &anchor_vector);
      auto current = evidence(graph, "temporal-current");
      current["valid_from"] = "2025-01-01T00:00:00Z";
      current["valid_until"] = "2027-01-01T00:00:00Z";
      const uint32_t root_left =
          add_edge(root, left, EdgeOrigin::Observed, EdgeRole::Mechanistic,
                   0.96, current);
      const uint32_t left_anchor =
          add_edge(left, anchor, EdgeOrigin::Discovered, EdgeRole::Causal,
                   0.95, current);
      const uint32_t root_right =
          add_edge(root, right, EdgeOrigin::Observed, EdgeRole::Supersedes,
                   0.94, current);
      const uint32_t right_anchor =
          add_edge(right, anchor, EdgeOrigin::Observed, EdgeRole::Causal,
                   0.93, current);
      auto derived = evidence(graph, "compressed-mechanism");
      derived["derived_from"] =
          std::to_string(root_left) + "," + std::to_string(left_anchor);
      const uint32_t valid_compressed =
          add_edge(root, anchor, EdgeOrigin::Observed, EdgeRole::Compressed,
                   0.92, derived);
      expect_path(&truth, root, {root_left, left_anchor});
      expect_path(&truth, root, {root_right, right_anchor});
      expect_path(&truth, root, {valid_compressed});

      const uint32_t unsafe_compressed =
          add_edge(bad_compressed, anchor, EdgeOrigin::Observed,
                   EdgeRole::Compressed, 0.999,
                   evidence(graph, "unexplained-shortcut"));
      auto stale = evidence(graph, "stale");
      stale["valid_until"] = "2025-01-01T00:00:00Z";
      const uint32_t stale_edge =
          add_edge(stale_root, anchor, EdgeOrigin::Observed, EdgeRole::Causal,
                   0.999, stale);
      auto future = evidence(graph, "future");
      future["valid_from"] = "2027-01-01T00:00:00Z";
      const uint32_t future_edge =
          add_edge(future_root, anchor, EdgeOrigin::Observed, EdgeRole::Causal,
                   0.999, future);
      auto malformed = evidence(graph, "malformed");
      malformed["valid_from"] = "not-a-time";
      const uint32_t malformed_edge =
          add_edge(malformed_root, anchor, EdgeOrigin::Observed,
                   EdgeRole::Causal, 0.999, malformed);
      truth.forbidden_edges.insert(
          {unsafe_compressed, stale_edge, future_edge, malformed_edge});
      truth.temporal_violation_edges.insert(
          {stale_edge, future_edge, malformed_edge});
      truth.provenance_findings[unsafe_compressed].insert(
          "COMPRESSED_WITHOUT_MECHANISM");
      truth.provenance_findings[malformed_edge].insert(
          "INVALID_TEMPORAL_METADATA");
      truth.query = anchor_vector;
    } else if (shape == 3) {
      ++coverage.join_graphs;
      ++coverage.hyperedge_graphs;
      ++coverage.multi_root_graphs;
      const uint32_t source_a = add_node(graph, "joint-source-a", true);
      const uint32_t source_b = add_node(graph, "joint-source-b", true);
      const uint32_t incomplete_source =
          add_node(graph, "joint-incomplete-source", true);
      declared_roots += 3;
      const uint32_t anchor =
          add_node(graph, "anchor", false, &anchor_vector);
      std::vector<uint32_t> sources{source_a, source_b};
      std::sort(sources.begin(), sources.end());
      const std::string source_list =
          std::to_string(sources[0]) + "," + std::to_string(sources[1]);
      auto joint = evidence(graph, "joint");
      joint["hyperedge_id"] = "d0-joint-" + std::to_string(graph);
      joint["hyperedge_group_id"] =
          "d0-joint-group-" + std::to_string(graph);
      joint["hyperedge_semantics"] = "all_sources";
      joint["hyperedge_sources"] = source_list;
      joint["hyperedge_arity"] = "2";
      const uint32_t member_a =
          add_edge(source_a, anchor, EdgeOrigin::Observed, EdgeRole::Causal,
                   0.94, joint);
      const uint32_t member_b =
          add_edge(source_b, anchor, EdgeOrigin::Observed, EdgeRole::Causal,
                   0.93, joint);
      expect_path(&truth, source_a, {member_a, member_b});
      expect_path(&truth, source_b, {member_a, member_b});

      auto incomplete = evidence(graph, "incomplete-joint");
      incomplete["hyperedge_id"] =
          "d0-incomplete-" + std::to_string(graph);
      incomplete["hyperedge_group_id"] =
          "d0-incomplete-group-" + std::to_string(graph);
      incomplete["hyperedge_semantics"] = "all_sources";
      incomplete["hyperedge_sources"] =
          std::to_string(source_a) + "," +
          std::to_string(incomplete_source);
      incomplete["hyperedge_arity"] = "2";
      const uint32_t incomplete_edge =
          add_edge(incomplete_source, anchor, EdgeOrigin::Observed,
                   EdgeRole::Causal, 0.999, incomplete);
      truth.forbidden_edges.insert(incomplete_edge);
      truth.hyperedge_violation_edges.insert(incomplete_edge);
      truth.query = anchor_vector;
    } else {
      ++coverage.cycle_graphs;
      ++coverage.disconnected_graphs;
      ++coverage.no_evidence_graphs;
      const uint32_t root = add_node(graph, "disconnected-root", true);
      ++declared_roots;
      if (graph == 4) {
        for (size_t index = 0; index < 7; ++index) {
          add_node(graph, "extra-root-" + std::to_string(index), true);
          ++declared_roots;
        }
      }
      const uint32_t cycle_a = add_node(graph, "cycle-a", false);
      const uint32_t cycle_b = add_node(graph, "cycle-b", false);
      const uint32_t anchor =
          add_node(graph, "anchor", false, &anchor_vector);
      add_edge(cycle_a, cycle_b, EdgeOrigin::Observed, EdgeRole::Supports,
               0.8, evidence(graph, "cycle-a"));
      add_edge(cycle_b, cycle_a, EdgeOrigin::Observed, EdgeRole::Supports,
               0.8, evidence(graph, "cycle-b"));
      const uint32_t excluded =
          add_edge(root, anchor, EdgeOrigin::Inferred, EdgeRole::Causal,
                   0.999, {});
      truth.forbidden_edges.insert(excluded);
      truth.false_promotion_edges.insert(excluded);
      truth.provenance_findings[excluded].insert(
          "INFERRED_WITHOUT_DERIVATION");
      truth.query = anchor_vector;
      truth.should_abstain = true;
    }

    size_t desired_nodes = batch.nodes.size() - node_start;
    if (graph == 1) desired_nodes = 200;
    while (batch.nodes.size() - node_start < desired_nodes) {
      add_node(graph, "padding-" +
                          std::to_string(batch.nodes.size() - node_start),
               false);
    }
    const size_t graph_nodes = batch.nodes.size() - node_start;
    coverage.min_nodes = std::min(coverage.min_nodes, graph_nodes);
    coverage.max_nodes = std::max(coverage.max_nodes, graph_nodes);
    coverage.min_roots = std::min(coverage.min_roots, declared_roots);
    coverage.max_roots = std::max(coverage.max_roots, declared_roots);
    ++coverage.graphs;
    coverage.queries += query_count;
    add_queries(graph, std::move(truth), query_count);
  }
};

void write_text_atomic(const fs::path& path, const std::string& content) {
  fs::create_directories(path.parent_path());
  const fs::path temporary = path.string() + ".tmp";
  {
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output) fail("cannot open " + temporary.string());
    output << content;
    output.flush();
    if (!output) fail("cannot write " + temporary.string());
  }
  std::error_code ignored;
  fs::remove(path, ignored);
  fs::rename(temporary, path);
}

std::string config_text(const Options& options) {
  std::ostringstream out;
  out << "schema=deepmind-d0-v1\n"
      << "seed=" << options.seed << "\n"
      << "graphs=" << options.graphs << "\n"
      << "queries_per_graph=" << options.queries_per_graph << "\n"
      << "as_of=" << kAsOf << "\n";
  return out.str();
}

void write_dataset(const fs::path& path,
                   const std::vector<QueryTruth>& queries) {
  const fs::path temporary = path.string() + ".tmp";
  std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
  if (!output) fail("cannot open dataset output");
  for (const auto& truth : queries) {
    output << "{\"schema\":\"deepmind-d0-truth-v1\",\"query_id\":\""
           << json_escape(truth.query_id) << "\",\"graph_index\":"
           << truth.graph_index << ",\"query_vector\":"
           << json_float_array(truth.query) << ",\"query_signature\":"
           << truth.signature << ",\"expected_roots\":"
           << json_array(as_vector(truth.roots))
           << ",\"expected_paths\":[";
    bool first_path = true;
    for (const auto& [root, paths] : truth.paths) {
      for (const auto& path_edges : paths) {
        if (!first_path) output << ",";
        first_path = false;
        output << "{\"root\":" << root << ",\"edges\":"
               << json_array(path_edges) << "}";
      }
    }
    output << "],\"forbidden_edges\":"
           << json_array(as_vector(truth.forbidden_edges))
           << ",\"provenance_findings\":[";
    bool first_finding = true;
    for (const auto& [edge, codes] : truth.provenance_findings) {
      for (const auto& code : codes) {
        if (!first_finding) output << ",";
        first_finding = false;
        output << "{\"edge\":" << edge << ",\"code\":\""
               << json_escape(code) << "\"}";
      }
    }
    output << "]"
           << ",\"expect_contradiction\":"
           << (truth.expect_contradiction ? "true" : "false")
           << ",\"should_abstain\":"
           << (truth.should_abstain ? "true" : "false") << "}\n";
  }
  output.flush();
  if (!output) fail("cannot write dataset output");
  output.close();
  std::error_code ignored;
  fs::remove(path, ignored);
  fs::rename(temporary, path);
}

void add(Aggregate* total, const EpisodeScore& score) {
  ++total->completed;
  total->expected_roots += score.expected_roots;
  total->matched_roots += score.matched_roots;
  total->expected_paths += score.expected_paths;
  total->matched_paths += score.matched_paths;
  total->returned_paths += score.returned_paths;
  total->inadmissible_paths += score.inadmissible_paths;
  total->false_promotions += score.false_promotions;
  total->contradiction_expected += score.contradiction_expected;
  total->contradiction_detected += score.contradiction_detected;
  total->abstention_expected += score.abstention_expected;
  total->abstention_correct += score.abstention_correct;
  total->temporal_violations += score.temporal_violations;
  total->hyperedge_violations += score.hyperedge_violations;
  total->provenance_mismatches += score.provenance_mismatches;
  total->budget_violations += score.budget_violations;
  total->latency_total_ms += score.latency_ms;
  total->latency_max_ms =
      std::max(total->latency_max_ms, score.latency_ms);
}

std::string state_line(const std::string& query_id,
                       const EpisodeScore& score) {
  std::ostringstream out;
  out << query_id << "\t" << score.expected_roots << "\t"
      << score.matched_roots << "\t" << score.expected_paths << "\t"
      << score.matched_paths << "\t" << score.returned_paths << "\t"
      << score.inadmissible_paths << "\t" << score.false_promotions << "\t"
      << score.contradiction_expected << "\t"
      << score.contradiction_detected << "\t"
      << score.abstention_expected << "\t" << score.abstention_correct << "\t"
      << score.temporal_violations << "\t" << score.hyperedge_violations
      << "\t" << score.provenance_mismatches << "\t"
      << score.budget_violations << "\t" << std::setprecision(17)
      << score.latency_ms << "\n";
  return out.str();
}

std::unordered_map<std::string, EpisodeScore> load_state(
    const fs::path& path,
    Aggregate* total) {
  std::unordered_map<std::string, EpisodeScore> completed;
  std::ifstream input(path);
  std::string line;
  while (std::getline(input, line)) {
    std::istringstream fields(line);
    std::string query_id;
    EpisodeScore score;
    if (!(std::getline(fields, query_id, '\t') >>
          score.expected_roots)) {
      fail("malformed resume state");
    }
    auto tab_value = [&](auto* value) {
      if (fields.get() != '\t' || !(fields >> *value)) {
        fail("malformed resume state");
      }
    };
    tab_value(&score.matched_roots);
    tab_value(&score.expected_paths);
    tab_value(&score.matched_paths);
    tab_value(&score.returned_paths);
    tab_value(&score.inadmissible_paths);
    tab_value(&score.false_promotions);
    tab_value(&score.contradiction_expected);
    tab_value(&score.contradiction_detected);
    tab_value(&score.abstention_expected);
    tab_value(&score.abstention_correct);
    tab_value(&score.temporal_violations);
    tab_value(&score.hyperedge_violations);
    tab_value(&score.provenance_mismatches);
    tab_value(&score.budget_violations);
    tab_value(&score.latency_ms);
    if (!completed.emplace(query_id, score).second) {
      fail("duplicate query id in resume state: " + query_id);
    }
    add(total, score);
  }
  return completed;
}

bool intersects(const std::vector<uint32_t>& values,
                const std::set<uint32_t>& forbidden) {
  return std::any_of(values.begin(), values.end(),
                     [&](uint32_t value) {
                       return forbidden.count(value) != 0;
                     });
}

bool contradiction_reported(const DialecticResult& result,
                            const BundleSet& bundle) {
  for (const auto& root : bundle.roots) {
    for (const auto& path : root.paths) {
      if (path.contains_contradiction) return true;
    }
  }
  return std::any_of(
      result.final_opposition.challenged_claims.begin(),
      result.final_opposition.challenged_claims.end(),
      [](const std::string& claim) {
        return claim.find("contradiction") != std::string::npos;
      });
}

EpisodeScore score_episode(const QueryTruth& truth,
                           const DialecticResult& result,
                           const BundleSet& bundle,
                           double latency_ms,
                           const DialecticOptions& options,
                           std::string* prediction_json) {
  EpisodeScore score;
  score.expected_roots = truth.roots.size();
  score.expected_paths = 0;
  for (const auto& [_, paths] : truth.paths) {
    score.expected_paths += paths.size();
  }
  std::set<uint32_t> returned_roots;
  std::vector<std::pair<uint32_t, PathKey>> returned_paths;
  bool false_promotion = false;
  bool temporal_violation = false;
  bool hyperedge_violation = false;
  std::map<uint32_t, std::set<std::string>> predicted_findings;
  for (const auto& root : bundle.roots) {
    returned_roots.insert(root.root_node);
    for (const auto& path : root.paths) {
      const PathKey key = canonical_path(path.edges);
      returned_paths.emplace_back(root.root_node, key);
      ++score.returned_paths;
      const auto expected_root = truth.paths.find(root.root_node);
      if (expected_root != truth.paths.end() &&
          expected_root->second.count(key) != 0) {
        ++score.matched_paths;
      } else {
        ++score.inadmissible_paths;
      }
      if (intersects(path.edges, truth.false_promotion_edges)) {
        false_promotion = true;
        ++score.false_promotions;
      }
      if (intersects(path.edges, truth.temporal_violation_edges)) {
        temporal_violation = true;
        ++score.temporal_violations;
      }
      if (intersects(path.edges, truth.hyperedge_violation_edges)) {
        hyperedge_violation = true;
        ++score.hyperedge_violations;
      }
      for (const auto& finding : path.provenance_findings) {
        predicted_findings[finding.edge_id].insert(finding.code);
      }
    }
  }
  for (uint32_t root : truth.roots) {
    if (returned_roots.count(root) != 0) ++score.matched_roots;
  }
  for (uint32_t root : returned_roots) {
    if (truth.roots.count(root) == 0) ++score.inadmissible_paths;
  }
  score.contradiction_expected = truth.expect_contradiction ? 1 : 0;
  score.contradiction_detected =
      truth.expect_contradiction && contradiction_reported(result, bundle)
          ? 1
          : 0;
  score.abstention_expected = truth.should_abstain ? 1 : 0;
  score.abstention_correct =
      truth.should_abstain && !result.synthesis.has_answer ? 1 : 0;
  std::set<uint32_t> returned_edges;
  for (const auto& [_, path] : returned_paths) {
    returned_edges.insert(path.begin(), path.end());
  }
  for (uint32_t edge : returned_edges) {
    const auto expected_it = truth.provenance_findings.find(edge);
    const std::set<std::string> expected =
        expected_it == truth.provenance_findings.end()
            ? std::set<std::string>{}
            : expected_it->second;
    const auto predicted_it = predicted_findings.find(edge);
    const std::set<std::string> predicted =
        predicted_it == predicted_findings.end()
            ? std::set<std::string>{}
            : predicted_it->second;
    if (expected != predicted) ++score.provenance_mismatches;
  }
  if (bundle.truncated || bundle.visited_states > options.max_visited_states ||
      result.rounds > options.max_opposition_rounds ||
      latency_ms > 0.0 && !std::isfinite(latency_ms)) {
    ++score.budget_violations;
  }
  score.latency_ms = latency_ms;

  std::ostringstream raw;
  raw << "{\"schema\":\"deepmind-d0-prediction-v1\",\"query_id\":\""
      << json_escape(truth.query_id) << "\",\"graph_index\":"
      << truth.graph_index << ",\"returned_roots\":"
      << json_array(as_vector(returned_roots)) << ",\"returned_paths\":[";
  for (size_t index = 0; index < returned_paths.size(); ++index) {
    if (index) raw << ",";
    raw << "{\"root\":" << returned_paths[index].first << ",\"edges\":"
        << json_array(returned_paths[index].second) << "}";
  }
  raw << "],\"has_answer\":"
      << (result.synthesis.has_answer ? "true" : "false")
      << ",\"epistemic_status\":\""
      << json_escape(result.synthesis.epistemic_status)
      << "\",\"contradiction_detected\":"
      << (contradiction_reported(result, bundle) ? "true" : "false")
      << ",\"false_promotion\":"
      << (false_promotion ? "true" : "false")
      << ",\"temporal_violation\":" << (temporal_violation ? "true" : "false")
      << ",\"hyperedge_violation\":"
      << (hyperedge_violation ? "true" : "false")
      << ",\"provenance_findings\":[";
  bool first_prediction_finding = true;
  for (const auto& [edge, codes] : predicted_findings) {
    for (const auto& code : codes) {
      if (!first_prediction_finding) raw << ",";
      first_prediction_finding = false;
      raw << "{\"edge\":" << edge << ",\"code\":\""
          << json_escape(code) << "\"}";
    }
  }
  raw << "]"
      << ",\"visited_states\":" << bundle.visited_states
      << ",\"truncated\":" << (bundle.truncated ? "true" : "false")
      << ",\"rounds\":" << result.rounds << ",\"latency_ms\":"
      << std::fixed << std::setprecision(6) << latency_ms
      << ",\"tokens\":0,\"tool_calls\":0,\"cost_usd\":0.0,\"warnings\":[";
  for (size_t index = 0; index < bundle.warnings.size(); ++index) {
    if (index) raw << ",";
    raw << "\"" << json_escape(bundle.warnings[index]) << "\"";
  }
  raw << "]}\n";
  *prediction_json = raw.str();
  return score;
}

double ratio(uint64_t numerator, uint64_t denominator) {
  return denominator == 0 ? 1.0
                          : static_cast<double>(numerator) /
                                static_cast<double>(denominator);
}

bool coverage_passes(const Coverage& coverage, const Options& options) {
  const double graphs = static_cast<double>(coverage.graphs);
  return options.graphs >= 5000 && options.queries_per_graph == 4 &&
         coverage.queries >= 20000 && coverage.min_nodes >= 2 &&
         coverage.unique_query_vectors == coverage.queries &&
         coverage.max_nodes <= 200 && coverage.min_nodes == 2 &&
         coverage.max_nodes == 200 && coverage.min_roots >= 1 &&
         coverage.max_roots <= 8 && coverage.max_roots == 8 &&
         ratio(coverage.multi_root_graphs, coverage.graphs) >= 0.25 &&
         ratio(coverage.contradiction_graphs, coverage.graphs) >= 0.20 &&
         ratio(coverage.temporal_graphs, coverage.graphs) >= 0.20 &&
         ratio(coverage.hyperedge_graphs, coverage.graphs) >= 0.10 &&
         ratio(coverage.no_evidence_graphs, coverage.graphs) >= 0.10 &&
         coverage.chain_graphs > 0 && coverage.fork_graphs > 0 &&
         coverage.join_graphs > 0 && coverage.diamond_graphs > 0 &&
         coverage.cycle_graphs > 0 && coverage.disconnected_graphs > 0 &&
         graphs > 0.0;
}

std::string metrics_json(const Options& options,
                         const Coverage& coverage,
                         const Aggregate& total,
                         bool formal_eligible,
                         bool g2_pass,
                         double elapsed_seconds,
                         bool complete) {
  const double root_recall =
      ratio(total.matched_roots, total.expected_roots);
  const double path_recall =
      ratio(total.matched_paths, total.expected_paths);
  const double inadmissible_rate =
      ratio(total.inadmissible_paths, total.returned_paths);
  const double promotion_rate =
      ratio(total.false_promotions, total.returned_paths);
  const double contradiction_recall =
      ratio(total.contradiction_detected, total.contradiction_expected);
  const double abstention_rate =
      ratio(total.abstention_correct, total.abstention_expected);
  std::ostringstream out;
  out << std::fixed << std::setprecision(9)
      << "{\n"
      << "  \"schema\": \"deepmind-g2-metrics-v1\",\n"
      << "  \"seed\": " << options.seed << ",\n"
      << "  \"formal_requested\": " << (options.formal ? "true" : "false")
      << ",\n"
      << "  \"formal_eligible\": " << (formal_eligible ? "true" : "false")
      << ",\n"
      << "  \"complete\": " << (complete ? "true" : "false") << ",\n"
      << "  \"g2_pass\": " << (g2_pass ? "true" : "false") << ",\n"
      << "  \"labels_in_database\": false,\n"
      << "  \"graphs\": " << coverage.graphs << ",\n"
      << "  \"queries_expected\": " << coverage.queries << ",\n"
      << "  \"queries_completed\": " << total.completed << ",\n"
      << "  \"unique_query_vectors\": " << coverage.unique_query_vectors
      << ",\n"
      << "  \"coverage\": {\n"
      << "    \"node_range\": [" << coverage.min_nodes << ","
      << coverage.max_nodes << "],\n"
      << "    \"root_range\": [" << coverage.min_roots << ","
      << coverage.max_roots << "],\n"
      << "    \"multi_root_fraction\": "
      << ratio(coverage.multi_root_graphs, coverage.graphs) << ",\n"
      << "    \"contradiction_fraction\": "
      << ratio(coverage.contradiction_graphs, coverage.graphs) << ",\n"
      << "    \"temporal_fraction\": "
      << ratio(coverage.temporal_graphs, coverage.graphs) << ",\n"
      << "    \"hyperedge_fraction\": "
      << ratio(coverage.hyperedge_graphs, coverage.graphs) << ",\n"
      << "    \"no_evidence_fraction\": "
      << ratio(coverage.no_evidence_graphs, coverage.graphs) << ",\n"
      << "    \"chains\": " << coverage.chain_graphs << ",\n"
      << "    \"forks\": " << coverage.fork_graphs << ",\n"
      << "    \"joins\": " << coverage.join_graphs << ",\n"
      << "    \"diamonds\": " << coverage.diamond_graphs << ",\n"
      << "    \"cycles\": " << coverage.cycle_graphs << ",\n"
      << "    \"disconnected\": " << coverage.disconnected_graphs << "\n"
      << "  },\n"
      << "  \"metrics\": {\n"
      << "    \"root_set_recall\": " << root_recall << ",\n"
      << "    \"admissible_path_recall\": " << path_recall << ",\n"
      << "    \"inadmissible_path_acceptance\": " << inadmissible_rate
      << ",\n"
      << "    \"inadmissible_path_count\": " << total.inadmissible_paths
      << ",\n"
      << "    \"false_promotion_rate\": " << promotion_rate << ",\n"
      << "    \"false_promotion_count\": " << total.false_promotions << ",\n"
      << "    \"contradiction_detection_recall\": "
      << contradiction_recall << ",\n"
      << "    \"correct_abstention_rate\": " << abstention_rate << ",\n"
      << "    \"temporal_violations_admitted\": "
      << total.temporal_violations << ",\n"
      << "    \"hyperedge_violations_admitted\": "
      << total.hyperedge_violations << ",\n"
      << "    \"provenance_truth_mismatches\": "
      << total.provenance_mismatches << ",\n"
      << "    \"budget_violations\": " << total.budget_violations << "\n"
      << "  },\n"
      << "  \"budgets\": {\n"
      << "    \"wall_clock_seconds\": " << options.wall_clock_seconds << ",\n"
      << "    \"per_query_ms\": " << options.per_query_ms << ",\n"
      << "    \"elapsed_seconds\": " << elapsed_seconds << ",\n"
      << "    \"mean_query_ms\": "
      << ratio(static_cast<uint64_t>(total.latency_total_ms * 1'000'000.0),
               total.completed) /
             1'000'000.0
      << ",\n"
      << "    \"max_query_ms\": " << total.latency_max_ms << "\n"
      << "  },\n"
      << "  \"accounting\": {\"tokens\": 0, \"tool_calls\": 0, "
         "\"cost_usd\": 0.0}\n"
      << "}\n";
  return out.str();
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const Options options = parse_options(argc, argv);
    const fs::path config_path = options.output_dir / "run_config.txt";
    const std::string expected_config = config_text(options);
    if (options.resume) {
      std::ifstream existing(config_path, std::ios::binary);
      std::ostringstream content;
      content << existing.rdbuf();
      if (!existing || content.str() != expected_config) {
        fail("--resume requires an existing evidence directory with the same "
             "seed and suite dimensions");
      }
    } else {
      if (fs::exists(options.output_dir) &&
          !fs::is_empty(options.output_dir)) {
        fail("output directory is not empty; select a new directory or use "
             "--resume");
      }
      fs::create_directories(options.output_dir);
      write_text_atomic(config_path, expected_config);
    }

    const auto run_started = std::chrono::steady_clock::now();
    Builder builder;
    builder.seed = options.seed;
    builder.batch.nodes.reserve(options.graphs * 14);
    builder.batch.edges.reserve(options.graphs * 8);
    builder.queries.reserve(options.graphs * options.queries_per_graph);
    for (size_t graph = 0; graph < options.graphs; ++graph) {
      builder.build_graph(graph, options.queries_per_graph);
    }
    write_dataset(options.output_dir / "dataset.jsonl", builder.queries);
    if (options.prepare_only) {
      write_text_atomic(
          options.output_dir / "preparation.json",
          "{\"schema\":\"deepmind-d0-preparation-v1\",\"seed\":" +
              std::to_string(options.seed) + ",\"graphs\":" +
              std::to_string(options.graphs) + ",\"queries\":" +
              std::to_string(builder.coverage.queries) +
              ",\"evaluation_started\":false}\n");
      std::cout << "deepmind_d0_prepared=true queries="
                << builder.coverage.queries << "\n";
      return 0;
    }

    const fs::path db_path = options.output_dir / "working-db";
    std::error_code cleanup_error;
    fs::remove_all(db_path, cleanup_error);
    GrapheneDB db;
    DBOptions db_options;
    db_options.dimension = kDimension;
    db_options.fsync_on_commit = false;
    db_options.vector_index_kind = VectorIndexKind::KDTree;
    require(db.open(db_path, db_options), "open D0 database");
    BatchResult inserted;
    require(db.put_batch(builder.batch, &inserted), "insert D0 corpus");
    if (inserted.node_ids.size() != builder.batch.nodes.size() ||
        inserted.edge_ids.size() != builder.batch.edges.size() ||
        (!inserted.node_ids.empty() && inserted.node_ids.front() != 0) ||
        (!inserted.edge_ids.empty() && inserted.edge_ids.front() != 0)) {
      fail("generated truth ids do not match inserted database ids");
    }

    Aggregate total;
    const fs::path state_path = options.output_dir / "resume.state.tsv";
    std::unordered_map<std::string, EpisodeScore> completed;
    if (options.resume) completed = load_state(state_path, &total);
    std::ofstream state(state_path, std::ios::app);
    std::ofstream raw(options.output_dir / "raw_predictions.jsonl",
                      std::ios::app);
    if (!state || !raw) fail("cannot open resumable evidence streams");

    DialecticEngine engine(db);
    DialecticOptions dialectic;
    dialectic.mode = QueryMode::Empirical;
    dialectic.semantic_candidates = 1;
    dialectic.max_hops = 8;
    dialectic.max_paths = 64;
    dialectic.max_paths_per_root = 16;
    dialectic.max_visited_states = 4096;
    dialectic.max_selected_paths = 16;
    dialectic.max_opposition_rounds = 0;
    dialectic.minimum_confidence = 0.45;
    dialectic.as_of = kAsOf;

    bool stopped_for_wall_clock = false;
    for (const auto& truth : builder.queries) {
      if (completed.count(truth.query_id) != 0) continue;
      const double elapsed =
          std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                       run_started)
              .count();
      if (elapsed > options.wall_clock_seconds) {
        stopped_for_wall_clock = true;
        break;
      }
      const auto query_started = std::chrono::steady_clock::now();
      const DialecticResult result =
          engine.reason(truth.query, truth.signature, dialectic);
      const double latency_ms =
          std::chrono::duration<double, std::milli>(
              std::chrono::steady_clock::now() - query_started)
              .count();
      const BundleSet& bundle = result.has_reopened_bundle
                                    ? result.reopened_bundle
                                    : result.initial_bundle;
      std::string prediction;
      EpisodeScore score = score_episode(
          truth, result, bundle, latency_ms, dialectic, &prediction);
      if (latency_ms > options.per_query_ms) ++score.budget_violations;
      raw << prediction;
      state << state_line(truth.query_id, score);
      raw.flush();
      state.flush();
      if (!raw || !state) fail("cannot persist resumable episode evidence");
      add(&total, score);
    }

    require(db.close(), "close D0 database");
    if (!options.keep_db) {
      fs::remove_all(db_path, cleanup_error);
      if (cleanup_error) {
        fail("cannot remove generated working database: " +
             cleanup_error.message());
      }
    }

    const double elapsed_seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                     run_started)
            .count();
    const bool complete =
        total.completed == builder.coverage.queries && !stopped_for_wall_clock;
    const bool formal_eligible =
        coverage_passes(builder.coverage, options);
    const double root_recall =
        ratio(total.matched_roots, total.expected_roots);
    const double path_recall =
        ratio(total.matched_paths, total.expected_paths);
    const double contradiction_recall =
        ratio(total.contradiction_detected, total.contradiction_expected);
    const double abstention_rate =
        ratio(total.abstention_correct, total.abstention_expected);
    const bool g2_pass =
        formal_eligible && complete && root_recall == 1.0 &&
        path_recall == 1.0 && total.inadmissible_paths == 0 &&
        total.false_promotions == 0 && contradiction_recall >= 0.99 &&
        abstention_rate >= 0.99 && total.temporal_violations == 0 &&
        total.hyperedge_violations == 0 && total.budget_violations == 0 &&
        total.provenance_mismatches == 0 &&
        elapsed_seconds <= options.wall_clock_seconds;
    const std::string metrics =
        metrics_json(options, builder.coverage, total, formal_eligible, g2_pass,
                     elapsed_seconds, complete);
    write_text_atomic(options.output_dir / "metrics.json", metrics);
    std::cout << metrics;
    if (options.formal && !g2_pass) return 2;
    return complete ? 0 : 3;
  } catch (const std::exception& error) {
    std::cerr << "deepmind_d0_error=" << error.what() << "\n";
    return 1;
  }
}
