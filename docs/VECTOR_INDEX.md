# Vector Index Boundary

RC4 adds a small vector-index seam so GrapheneDB is no longer architecturally hard-wired to a flat scan.

```cpp
class VectorIndex {
public:
  virtual Status add(uint32_t id, const std::vector<float>& vector) = 0;
  virtual Status remove(uint32_t id) = 0;
  virtual std::vector<SearchResult> search(const std::vector<float>& query, size_t k) const = 0;
  virtual const char* name() const = 0;
};
```

The current implementation is `FlatVectorIndex`. It preserves RC behavior but gives the project a clean place to add:

- HNSW
- FAISS
- DiskANN-like local index
- quantized flat index
- mmap-backed vector pages

## Current limitation

The current index is still in-memory and flat. It is a boundary, not yet a high-scale vector index.
