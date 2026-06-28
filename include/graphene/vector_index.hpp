#pragma once
#include "graphene/types.hpp"
#include <algorithm>
#include <cmath>
#include <memory>
#include <unordered_map>

namespace graphene {

class VectorIndex {
public:
  virtual ~VectorIndex() = default;
  virtual Status add(uint32_t id, const std::vector<float>& vector) = 0;
  virtual Status remove(uint32_t id) = 0;
  virtual std::vector<SearchResult> search(const std::vector<float>& query, size_t k) const = 0;
  virtual const char* name() const = 0;
};

class FlatVectorIndex final : public VectorIndex {
public:
  explicit FlatVectorIndex(uint32_t dimension) : dimension_(dimension) {}

  Status add(uint32_t id, const std::vector<float>& vector) override {
    if (vector.size() != dimension_) return Status::error(ErrorCode::DimensionMismatch, "FlatVectorIndex dimension mismatch");
    vectors_[id] = vector;
    return Status::ok();
  }

  Status remove(uint32_t id) override {
    vectors_.erase(id);
    return Status::ok();
  }

  std::vector<SearchResult> search(const std::vector<float>& query, size_t k) const override {
    if (query.size() != dimension_ || k == 0) return {};
    std::vector<SearchResult> scored;
    scored.reserve(vectors_.size());
    for (const auto& kv : vectors_) {
      double dot = 0.0, nq = 0.0, nv = 0.0;
      for (size_t i = 0; i < query.size(); ++i) {
        dot += static_cast<double>(query[i]) * kv.second[i];
        nq += static_cast<double>(query[i]) * query[i];
        nv += static_cast<double>(kv.second[i]) * kv.second[i];
      }
      if (nq > 0.0 && nv > 0.0) scored.push_back({kv.first, dot / (std::sqrt(nq) * std::sqrt(nv))});
    }
    const size_t kk = std::min(k, scored.size());
    std::partial_sort(scored.begin(), scored.begin() + kk, scored.end(), [](const auto& a, const auto& b) { return a.score > b.score; });
    scored.resize(kk);
    return scored;
  }

  const char* name() const override { return "flat"; }

private:
  uint32_t dimension_;
  std::unordered_map<uint32_t, std::vector<float>> vectors_;
};

} // namespace graphene
