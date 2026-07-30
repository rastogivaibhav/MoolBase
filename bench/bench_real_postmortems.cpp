#include "graphene/dialectic.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace graphene;
namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

namespace {

struct Postmortem {
  std::string uuid;
  std::string url;
  std::string title;
  std::string summary;
  std::string company;
  std::string product;
  std::string start_time;
  std::vector<std::string> categories;
};

std::string trim(std::string value) {
  while (!value.empty() &&
         std::isspace(static_cast<unsigned char>(value.front()))) {
    value.erase(value.begin());
  }
  while (!value.empty() &&
         std::isspace(static_cast<unsigned char>(value.back()))) {
    value.pop_back();
  }
  if (value.size() >= 2 &&
      ((value.front() == '"' && value.back() == '"') ||
       (value.front() == '\'' && value.back() == '\''))) {
    value = value.substr(1, value.size() - 2);
  }
  return value;
}

bool parse_postmortem(const fs::path& path, Postmortem* out) {
  std::ifstream input(path);
  if (!input || !out) return false;
  std::string line;
  if (!std::getline(input, line) || trim(line) != "---") return false;

  Postmortem parsed;
  std::string list_field;
  while (std::getline(input, line)) {
    if (trim(line) == "---") break;
    const std::string stripped = trim(line);
    if (stripped.rfind("- ", 0) == 0) {
      if (list_field == "categories") {
        parsed.categories.push_back(trim(stripped.substr(2)));
      }
      continue;
    }
    const size_t colon = line.find(':');
    if (colon == std::string::npos) continue;
    const std::string key = trim(line.substr(0, colon));
    const std::string value = trim(line.substr(colon + 1));
    list_field = value.empty() ? key : std::string{};
    if (key == "uuid") parsed.uuid = value;
    else if (key == "url") parsed.url = value;
    else if (key == "title") parsed.title = value;
    else if (key == "summary") parsed.summary = value;
    else if (key == "company") parsed.company = value;
    else if (key == "product") parsed.product = value;
    else if (key == "start_time") parsed.start_time = value;
  }

  std::sort(parsed.categories.begin(), parsed.categories.end());
  parsed.categories.erase(
      std::unique(parsed.categories.begin(), parsed.categories.end()),
      parsed.categories.end());
  if (parsed.uuid.empty() || parsed.url.empty() || parsed.title.empty() ||
      parsed.categories.empty()) {
    return false;
  }
  *out = std::move(parsed);
  return true;
}

uint64_t fnv64(const std::string& text) {
  uint64_t hash = 1469598103934665603ull;
  for (unsigned char byte : text) {
    hash ^= byte;
    hash *= 1099511628211ull;
  }
  return hash;
}

std::vector<float> embed_text(const std::string& text, uint32_t dimension) {
  std::vector<float> vector(dimension, 0.0f);
  uint64_t hash = 1469598103934665603ull;
  for (unsigned char byte : text) {
    hash ^= byte;
    hash *= 1099511628211ull;
    vector[hash % dimension] += ((hash >> 8) & 1) ? 1.0f : -1.0f;
  }
  double magnitude = 0.0;
  for (float value : vector) magnitude += value * value;
  magnitude = std::sqrt(magnitude);
  if (magnitude > 0.0) {
    for (float& value : vector) {
      value = static_cast<float>(value / magnitude);
    }
  }
  return vector;
}

double percentile(std::vector<double> values, double requested) {
  if (values.empty()) return 0.0;
  std::sort(values.begin(), values.end());
  const size_t index = static_cast<size_t>(
      requested / 100.0 * static_cast<double>(values.size() - 1));
  return values[index];
}

void require(Status status, const char* operation) {
  if (!status) {
    std::cerr << operation << ": " << status.message << "\n";
    std::exit(2);
  }
}

bool has_competing_root_challenge(const OppositionReport& report) {
  return std::any_of(
      report.challenged_claims.begin(), report.challenged_claims.end(),
      [](const std::string& challenge) {
        return challenge.find("competing roots remain") != std::string::npos;
      });
}

} // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: graphenedb_real_postmortems_bench <corpus-data-dir> "
                 "[max-records]\n";
    return 2;
  }
  const fs::path corpus = argv[1];
  const size_t maximum =
      argc > 2 ? static_cast<size_t>(std::stoull(argv[2])) : 0;
  if (!fs::is_directory(corpus)) {
    std::cerr << "corpus directory not found: " << corpus << "\n";
    return 2;
  }

  std::vector<Postmortem> records;
  for (const auto& entry : fs::directory_iterator(corpus)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".md") continue;
    Postmortem record;
    if (parse_postmortem(entry.path(), &record)) {
      records.push_back(std::move(record));
    }
  }
  std::sort(records.begin(), records.end(),
            [](const Postmortem& left, const Postmortem& right) {
              return left.uuid < right.uuid;
            });
  if (maximum != 0 && records.size() > maximum) records.resize(maximum);
  if (records.empty()) {
    std::cerr << "no usable annotated postmortems found\n";
    return 2;
  }

  constexpr uint32_t kDimension = 64;
  const fs::path database_directory =
      fs::temp_directory_path() / "graphenedb_real_postmortems";
  fs::remove_all(database_directory);
  GrapheneDB db;
  DBOptions options;
  options.dimension = kDimension;
  options.fsync_on_commit = false;
  options.vector_index_kind = VectorIndexKind::Flat;
  require(db.open(database_directory, options), "open");

  std::map<std::string, uint32_t> category_nodes;
  std::vector<uint32_t> incident_nodes;
  std::vector<std::vector<uint32_t>> expected_roots;
  std::vector<std::vector<float>> queries;
  std::vector<uint64_t> signatures;
  size_t source_timestamps = 0;

  for (size_t index = 0; index < records.size(); ++index) {
    const Postmortem& record = records[index];
    std::vector<uint32_t> roots;
    for (const std::string& category : record.categories) {
      auto existing = category_nodes.find(category);
      if (existing == category_nodes.end()) {
        NodeInput root;
        root.content = "annotated incident category: " + category;
        root.vector = embed_text(root.content, kDimension);
        root.root = true;
        root.metadata["dataset"] = "icco/postmortems";
        root.metadata["annotation"] = "category";
        uint32_t root_id = 0;
        require(db.put_node(root, &root_id), "put category root");
        existing = category_nodes.emplace(category, root_id).first;
      }
      roots.push_back(existing->second);
    }

    const uint64_t hash = fnv64(record.uuid);
    const uint64_t signature =
        signature_for(static_cast<uint32_t>(hash & 15),
                      static_cast<uint32_t>((hash >> 8) & 15));
    NodeInput incident;
    incident.content =
        record.title + (record.summary.empty() ? "" : ". " + record.summary);
    // Use the public incident title as the incoming alert/query. This isolates
    // database retrieval behavior from the choice of an embedding provider.
    incident.vector = embed_text(record.title, kDimension);
    incident.signature = signature;
    incident.incident = static_cast<uint32_t>(index + 1);
    incident.symptom = true;
    incident.metadata["dataset"] = "icco/postmortems";
    incident.metadata["uuid"] = record.uuid;
    incident.metadata["source"] = record.url;
    incident.metadata["company"] = record.company;
    incident.metadata["product"] = record.product;
    uint32_t incident_id = 0;
    require(db.put_node(incident, &incident_id), "put incident");

    for (uint32_t root_id : roots) {
      EdgeInput edge;
      edge.from = root_id;
      edge.to = incident_id;
      edge.origin = EdgeOrigin::Observed;
      edge.role = EdgeRole::Causal;
      edge.confidence = 0.95;
      edge.metadata["source_id"] = record.url;
      edge.metadata["span"] = "curated categories";
      if (!record.start_time.empty()) {
        Rfc3339Instant parsed;
        if (parse_rfc3339(record.start_time, &parsed)) {
          edge.metadata["observed_at"] = record.start_time;
          ++source_timestamps;
        }
      }
      require(db.put_edge(edge), "put annotated category edge");
    }

    incident_nodes.push_back(incident_id);
    expected_roots.push_back(std::move(roots));
    queries.push_back(incident.vector);
    signatures.push_back(signature);
  }

  uint64_t expected_category_links = 0;
  uint64_t vector_category_hits = 0;
  uint64_t causal_category_hits = 0;
  uint64_t causal_category_links_recalled = 0;
  uint64_t dialectic_category_links_recalled = 0;
  uint64_t complete_multi_root = 0;
  uint64_t multi_root_records = 0;
  uint64_t competing_root_challenges = 0;
  uint64_t provenance_safe_records = 0;
  uint64_t false_promotions = 0;
  std::vector<double> vector_latency;
  std::vector<double> causal_latency;
  std::vector<double> dialectic_latency;

  DialecticOptions reasoning;
  reasoning.mode = QueryMode::Empirical;
  reasoning.semantic_candidates = 1;
  reasoning.max_hops = 2;
  reasoning.max_paths = 16;
  reasoning.max_paths_per_root = 4;
  reasoning.max_opposition_rounds = 1;
  DialecticEngine engine(db);

  for (size_t index = 0; index < records.size(); ++index) {
    const std::set<uint32_t> expected(expected_roots[index].begin(),
                                      expected_roots[index].end());
    expected_category_links += expected.size();
    if (expected.size() > 1) ++multi_root_records;

    const auto vector_start = Clock::now();
    const auto vector_result = db.vector_search(queries[index], 1);
    const auto vector_end = Clock::now();
    const MemoryBundle causal =
        db.causal_search(queries[index], signatures[index],
                         QueryMode::Empirical);
    const auto causal_end = Clock::now();
    const DialecticResult dialectic =
        engine.reason(queries[index], signatures[index], reasoning);
    const auto dialectic_end = Clock::now();

    if (!vector_result.empty() &&
        expected.count(vector_result.front().node_id) != 0) {
      ++vector_category_hits;
    }
    if (!causal.abstain && expected.count(causal.target_node) != 0) {
      ++causal_category_hits;
      ++causal_category_links_recalled;
    }

    std::set<uint32_t> returned_roots;
    const BundleSet& final_bundle = dialectic.has_reopened_bundle
                                        ? dialectic.reopened_bundle
                                        : dialectic.initial_bundle;
    bool provenance_safe = true;
    for (const RootBundle& root : final_bundle.roots) {
      returned_roots.insert(root.root_node);
      if (root.provenance_risk != 0.0 || root.evidence_coverage != 1.0) {
        provenance_safe = false;
      }
    }
    size_t recalled = 0;
    for (uint32_t root : expected) {
      if (returned_roots.count(root) != 0) ++recalled;
    }
    dialectic_category_links_recalled += recalled;
    if (expected.size() > 1 && recalled == expected.size()) {
      ++complete_multi_root;
    }
    if (expected.size() > 1 &&
        has_competing_root_challenge(dialectic.final_opposition)) {
      ++competing_root_challenges;
    }
    if (provenance_safe && recalled != 0) ++provenance_safe_records;
    if (dialectic.synthesis.has_answer &&
        expected.count(dialectic.synthesis.primary_node) == 0) {
      ++false_promotions;
    }

    vector_latency.push_back(
        std::chrono::duration<double, std::milli>(vector_end - vector_start)
            .count());
    causal_latency.push_back(
        std::chrono::duration<double, std::milli>(causal_end - vector_end)
            .count());
    dialectic_latency.push_back(
        std::chrono::duration<double, std::milli>(dialectic_end - causal_end)
            .count());
  }

  const auto rate = [count = records.size()](uint64_t value) {
    return static_cast<double>(value) / static_cast<double>(count);
  };
  const double causal_micro_recall =
      expected_category_links == 0
          ? 0.0
          : static_cast<double>(causal_category_links_recalled) /
                static_cast<double>(expected_category_links);
  const double dialectic_micro_recall =
      expected_category_links == 0
          ? 0.0
          : static_cast<double>(dialectic_category_links_recalled) /
                static_cast<double>(expected_category_links);
  const double multi_root_complete_rate =
      multi_root_records == 0
          ? 0.0
          : static_cast<double>(complete_multi_root) /
                static_cast<double>(multi_root_records);
  const double competing_challenge_rate =
      multi_root_records == 0
          ? 0.0
          : static_cast<double>(competing_root_challenges) /
                static_cast<double>(multi_root_records);

  std::cout << "real_postmortem_evaluation=true\n"
            << "claim_scope=annotated_graph_retrieval_not_autonomous_rca\n"
            << "records=" << records.size() << "\n"
            << "categories=" << category_nodes.size() << "\n"
            << "annotated_category_links=" << expected_category_links << "\n"
            << "source_timestamps=" << source_timestamps << "\n"
            << "vector_top1_category_hit_rate="
            << rate(vector_category_hits) << "\n"
            << "causal_any_category_hit_rate="
            << rate(causal_category_hits) << "\n"
            << "causal_category_micro_recall=" << causal_micro_recall << "\n"
            << "dialectic_category_micro_recall="
            << dialectic_micro_recall << "\n"
            << "dialectic_complete_multi_root_rate="
            << multi_root_complete_rate << "\n"
            << "dialectic_competing_root_challenge_rate="
            << competing_challenge_rate << "\n"
            << "dialectic_provenance_safe_record_rate="
            << rate(provenance_safe_records) << "\n"
            << "dialectic_false_promotion_rate="
            << rate(false_promotions) << "\n"
            << "vector_p95_ms=" << percentile(vector_latency, 95) << "\n"
            << "causal_p95_ms=" << percentile(causal_latency, 95) << "\n"
            << "dialectic_p95_ms=" << percentile(dialectic_latency, 95)
            << "\n";

  require(db.close(), "close");
  fs::remove_all(database_directory);
  return false_promotions == 0 && dialectic_category_links_recalled != 0
             ? 0
             : 1;
}
