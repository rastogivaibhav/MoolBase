#pragma once
#include "graphene/dialectic.hpp"
#include <filesystem>
#include <string>
#include <vector>

namespace graphene {

struct KoshMemoryRecord {
  std::string external_id;
  std::string content;
  std::string type;
  std::vector<float> vector;
  uint64_t signature{0};
  uint32_t incident{0};
  bool root{false};
  bool symptom{false};
  std::map<std::string, std::string> metadata;
};

class KoshAdapter {
public:
  explicit KoshAdapter(GrapheneDB& db);
  Status ingest_memory(const KoshMemoryRecord& memory, uint32_t* out_id = nullptr);
  Status link(uint32_t from,
              uint32_t to,
              EdgeRole role = EdgeRole::Causal,
              EdgeOrigin origin = EdgeOrigin::Observed,
              double confidence = 0.9,
              std::map<std::string, std::string> provenance = {});
  MemoryBundle retrieve_causal_bundle(const std::vector<float>& query, uint64_t signature, QueryMode mode = QueryMode::Empirical) const;
  DialecticResult retrieve_dialectic(const std::vector<float>& query,
                                     uint64_t signature,
                                     const DialecticOptions& options = {}) const;

  // Minimal local interchange for LLM-Kosh/KoshDB gate tests. Format is tab-separated:
  // external_id, type, signature, incident, root(0/1), symptom(0/1), vector_csv, content_hex, metadata k=v;k=v
  Status ingest_tsv(const std::filesystem::path& file, std::vector<uint32_t>* out_ids = nullptr);

private:
  GrapheneDB& db_;
};

} // namespace graphene
