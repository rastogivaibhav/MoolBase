# GrapheneDB Quick Start: 10 Minutes to Your First Memory

This guide walks you through building and using GrapheneDB on your machine. No prior knowledge of lattice structures or causal reasoning required.

**Time estimate**: ~10 minutes  
**Prerequisites**: CMake 3.16+, C++20 compiler, Git

---

## Step 1: Build GrapheneDB (3 minutes)

### On Linux/macOS
```bash
git clone https://github.com/yourusername/graphenedb.git
cd graphenedb
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j $(nproc)
```

### On Windows (LLVM/Clang)
```bash
git clone https://github.com/yourusername/graphenedb.git
cd graphenedb
cmake -S . -B build -G Ninja `
  -DCMAKE_C_COMPILER=clang `
  -DCMAKE_CXX_COMPILER=clang++ `
  -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 8
```

The CLI binary is now at `build/graphenedb_cli` (Linux/macOS) or `build/graphenedb_cli.exe` (Windows).

---

## Step 2: Initialize a Database (1 minute)

Create a folder where your memory will live:

```bash
./build/graphenedb_cli init /tmp/my_memories 4
```

Output: `initialized`

**What just happened:**
- Created a durable database at `/tmp/my_memories`
- Set vector dimension to 4 (small for demo; use 64–1024 in practice)
- Initialized the Write-Ahead Log and storage layer

---

## Step 3: Store Your First Memory (2 minutes)

Add a memory node — anything with content and a vector embedding:

```bash
./build/graphenedb_cli put-node /tmp/my_memories 4 \
  "Fixed bug: off-by-one in pagination" \
  "0.1,0.2,0.3,0.4" \
  42 \
  root
```

**Output**: `0` (the node ID)

**What you did**:
- **Path**: `/tmp/my_memories` — where the database lives
- **Dimension**: `4` — must match what you initialized
- **Content**: `"Fixed bug: off-by-one in pagination"` — the human-readable memory
- **Vector**: `"0.1,0.2,0.3,0.4"` — the embedding (usually from a model; here it's demo data)
- **Signature**: `42` — a uint64 fingerprint or hash (use a real fingerprint in production)
- **Type**: `root` — this is a root-cause node (could be `impact` or `symptom` instead)

Now add a related memory:

```bash
./build/graphenedb_cli put-node /tmp/my_memories 4 \
  "Service crashed for 30 min due to pagination bug" \
  "0.12,0.18,0.35,0.42" \
  99 \
  impact
```

**Output**: `1` (the second node)

---

## Step 4: Link Memories with Causality (1 minute)

Connect the two nodes — tell GrapheneDB that one caused the other:

```bash
./build/graphenedb_cli put-edge /tmp/my_memories 4 0 1 causal
```

**Output**: `0` (the edge ID)

**What you did**:
- Link node `0` (root cause) → node `1` (impact)
- Mark it as `causal` (other options: `contradicts`, `supersedes`, `supports`)

---

## Step 5: Retrieve by Similarity (1 minute)

Search for memories similar to a query:

```bash
./build/graphenedb_cli search /tmp/my_memories 4 \
  "0.1,0.2,0.3,0.4" \
  42
```

**Output**:
```
target=0 confidence=1 paths=2
why=semantic similarity to candidate memories
why=signature-plane candidate reduction
why=causal path from root memory to anchor memory
OK
```

GrapheneDB found node 0 (your first memory) as the best match and showed you *why*:
- It's semantically similar (vector distance)
- It matched your signature
- There's a causal chain to it

---

## Step 6: Inspect the Database (1 minute)

Get a summary of what you've built:

```bash
./build/graphenedb_cli inspect /tmp/my_memories 4
```

**Output snippet**:
```
GrapheneDB v1
nodes_visible=2
edges_visible=1
vector_index=kdtree
dimension=4
wal_bytes=620
validation=ok
```

---

## Step 7: Validate Integrity (1 minute)

Verify everything's durable and consistent:

```bash
./build/graphenedb_cli validate /tmp/my_memories 4
```

**Output**: `validation=ok`

This checks:
- Write-Ahead Log integrity
- Node consistency
- Edge references
- Vector index coherence

---

## What Just Happened

You've built a **causal memory system** in 10 minutes:

1. Two memories (bug fix + service impact)
2. One causal link (root → impact)
3. Vector-based retrieval that explains *why* it returned that result
4. Durable storage (survives crashes)

---

## Next Steps

### To Integrate Into Your Code

If you want to use GrapheneDB as a library (not just CLI):

**CMakeLists.txt**:
```cmake
find_package(GrapheneDB REQUIRED)
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE GrapheneDB::graphenedb)
```

**C++ Code**:
```cpp
#include <graphene/db.hpp>

int main() {
  graphene::GrapheneDB db;
  graphene::DBOptions opt;
  opt.dimension = 64;
  auto st = db.open("/tmp/my_memories", opt);
  if (!st) { std::cerr << st.message << "\n"; return 1; }

  graphene::NodeInput node;
  node.content = "Your memory here";
  node.vector = {/* your embedding */};
  node.signature = 123456;
  node.root = true;

  uint32_t node_id = 0;
  st = db.put_node(node, &node_id);
  if (!st) { std::cerr << st.message << "\n"; return 1; }

  std::cout << "Created node " << node_id << "\n";
  db.close();
  return 0;
}
```

### To Use the HTTP Server (Linux/macOS only)

```bash
./build/graphenedb_server /tmp/my_memories 64 8080 --api-key demo-key
# Server now listening at http://localhost:8080
curl -H "x-api-key: demo-key" http://localhost:8080/v1/health
```

### To Understand More

- **[Data Model Explainer](GRAPHENE_LATTICE_MODEL.md)**: What nodes, edges, and lattice coordinates mean
- **[Governance & Learning](GOVERNED_LEARNING_V0_SPEC.md)**: Track AI decisions and outcomes
- **[Full Architecture](ARCHITECTURE.md)**: How storage, indexing, and retrieval work
- **[Examples](../examples/)**: Real use cases (incident memory, team brain, dialectic reasoning)

---

## Troubleshooting

### Build fails: "C++20 compiler required"
Upgrade your compiler:
- **GCC**: 11+
- **Clang**: 12+
- **MSVC**: 2019+

### CLI error: "bad signature"
The signature must be a number (uint64):
```bash
# ✗ WRONG
./build/graphenedb_cli put-node /tmp/my_memories 4 "..." "..." abc root

# ✓ CORRECT
./build/graphenedb_cli put-node /tmp/my_memories 4 "..." "..." 123 root
```

If you hit an error, it now prints a clean message instead of crashing (as of v0.6.0-rc1).

### "No such file or directory"
Make sure your database path exists and the dimension matches:
```bash
# Initialize first
./build/graphenedb_cli init /tmp/my_memories 4

# Then all commands use dimension 4
./build/graphenedb_cli put-node /tmp/my_memories 4 ...
```

### Vector format
Vectors must be comma-separated floats, no spaces:
```bash
# ✓ CORRECT
"0.1,0.2,0.3,0.4"

# ✗ WRONG
"0.1, 0.2, 0.3, 0.4"  # Space after comma breaks parsing
```

---

## What Happens Under the Hood

When you call `put-node`, GrapheneDB:

1. **Adds to WAL** (Write-Ahead Log) — durability first
2. **Indexes the vector** — builds/updates the KD-tree for fast similarity search
3. **Stores metadata** — content, signature, type (root/impact/symptom)
4. **Returns the node ID** — you got `0`, `1`, etc.

When you call `search`:

1. **Finds similar vectors** using the index
2. **Traverses causal links** (the edge you made: 0→1)
3. **Explains the result** with "why" statements (semantic match, causal path, etc.)

All of this is **synchronous, local, and durable** — no network required.

---

## Key Limits (Pre-Production Use)

- **Single-process only**: One app at a time (file locks prevent corruption)
- **No encryption**: Secure the disk if you're storing sensitive data
- **Developer preview**: API may change before v1.0
- **POSIX server**: HTTP server only on Linux/macOS (CLI works everywhere)

For production use, see [Limitations & Safety Guide](../docs/SAFETY_AND_LIMITATIONS.md).

---

## You're Done! 🎉

You've built, stored, retrieved, and validated a causal memory system. This is the core of what GrapheneDB does — everything else (governance policies, dialectic reasoning, multi-agent orchestration) layers on top.

**Questions?** Check the [Data Model Explainer](GRAPHENE_LATTICE_MODEL.md) or the [Examples](../examples/).

**Ready to ship?** See [Limitations & Safety](SAFETY_AND_LIMITATIONS.md) before production.
