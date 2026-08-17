# GrapheneDB Repository Structure Guide

**Status:** v0.6.0-rc1 (Master branch)  
**Last Updated:** August 17, 2026

---

## Quick Navigation

| Folder | Purpose | Keep? | Notes |
|--------|---------|-------|-------|
| `src/` | 🔴 **Core library source** | ✅ YES | Main C++ implementation |
| `include/graphene/` | 🔴 **Public API headers** | ✅ YES | C++ interfaces, ABI contracts |
| `tools/` | 🟡 **CLI & operational tools** | ✅ YES | graphenedb_cli, graphenedb_server |
| `tests/` | 🟡 **Test suite** | ✅ YES | Unit, integration, fault-injection tests |
| `docs/` | 🟡 **Documentation** | ✅ YES | Architecture, lattice model, quick-start |
| `scripts/` | 🟡 **Build & release automation** | ✅ YES | CI/CD, verification, packaging |
| `cmake/` | 🟡 **CMake configuration** | ✅ YES | Build system |
| `reports/` | 🟡 **Release evidence** | ✅ YES | GA readiness, test results (intentional) |
| `examples/` | 🟡 **Sample programs** | ✅ YES | C++ API usage examples |
| `clients/` | 🟡 **Language bindings** | ✅ YES | Python client, etc. |
| `paper/` | 🟢 **Research/whitepaper** | ⚠️ OPTIONAL | Archive reference, not active |
| `benchmarks/` | 🟢 **Historical benchmarks** | ⚠️ OPTIONAL | Older perf data, consider cleaning |
| `fuzz/` | 🟢 **Fuzzing corpus** | ⚠️ OPTIONAL | Archive data, can be cleaned |
| `deploy/` | 🟢 **Deployment configs** | ⚠️ OPTIONAL | Cloud templates, can be external |
| `updates/` | 🟢 **Update tooling** | ⚠️ OPTIONAL | Can be moved to wiki/docs |
| `.github/` | 🟡 **GitHub workflows** | ✅ YES | CI/CD pipelines |
| `.agents/` | 🟡 **Agent configuration** | ✅ YES | Claude Code agent setup |

---

## The Mess: Local Build Directories

### What's Cluttering Your Workspace

```
build-deepmind-foundation-consumer/          ← Local build (not in git)
build-deepmind-foundation-install/           ← Local build (not in git)
build-deepmind-foundation-package/           ← Local build (not in git)
build-dialectic-package/                     ← Local build (not in git)
build-dialectic-package-consumer/            ← Local build (not in git)
build-dialectic-package-install/             ← Local build (not in git)
build-fix/                                   ← Local build (not in git)
build-g2-linux/                              ← Local build (not in git)
build-package-consumer/                      ← Local build (not in git)
build-package-governed/                      ← Local build (not in git)
build-package-governed-consumer/             ← Local build (not in git)
build-package-governed-install/              ← Local build (not in git)
build-remediation/                           ← Local build (not in git)
build-remediation-default/                   ← Local build (not in git)
build-review/                                ← Local build (not in git)
build-review-debug/                          ← Local build (not in git)
build-win/                                   ← Local build (not in git)
build_test/                                  ← Local build (active, used in this session)
graphify-out/                                ← Local knowledge graph output (not in git)
tmp/                                         ← Local scratch (not in git)
```

### The Root Cause

These are **local build artifacts** from previous development sessions:
- Each `build-*` folder is a different CMake configuration
- Named after features being tested (deepmind, dialectic, governed, remediation, etc.)
- They **do NOT sync to GitHub** (covered by `.gitignore`)
- They're just taking up **disk space locally**
- They're **confusing** because you have to remember which one is "latest"

---

## Consolidation Strategy

### Option 1: Clean Approach (Recommended)

**Keep only ONE active build directory:**

```bash
# Clean up all old build directories
rm -rf build-* build_test

# Create ONE canonical build directory for all your work
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Now all builds go to ./build/ — easy to find, easy to clean
```

**Benefits:**
- ✅ One place to find binaries
- ✅ Easy to rebuild: `rm -rf build && mkdir build && cd build && cmake .. && make`
- ✅ Matches standard CMake workflow
- ✅ Clear what's "current"

**How to use after cleanup:**
```bash
# Build everything once
cd graphenedb && mkdir build && cd build && cmake .. && make

# From now on:
./build/graphenedb_cli init /tmp/test.db 768
./build/graphenedb_cli put-node /tmp/test.db 768 ...

# To rebuild after code changes:
cd build && make
```

---

### Option 2: Docker Approach (If you want isolation)

Instead of 20 build directories on your machine, use containers:

```bash
# Build everything in a container (doesn't pollute your workspace)
docker run --rm -v $PWD:/src ubuntu:22.04 bash -c \
  "cd /src && mkdir build_docker && cd build_docker && cmake .. && make"

# Binaries are in build_docker/, can be cleaned up after
rm -rf build_docker
```

**Benefit:** Isolation, reproducibility. Downside: Slower, requires Docker.

---

### Option 3: Hybrid (My Recommendation)

1. **Keep ONE active build directory** (`./build/`)
2. **Archive old benchmark data** to `./benchmarks-archive/` (or S3)
3. **Clean local scratch** (`tmp/`, `graphify-out/`)
4. **Commit only essential reports** to `reports/`

```bash
# Clean in one go
rm -rf build-* build_test graphify-out tmp

# Recreate canonical build
mkdir build && cd build && cmake .. && make -j8

# Archive historical data (optional)
tar czf graphenedb-benchmarks-archive-2026-08.tar.gz benchmarks/
rm -rf benchmarks

# Confirm clean state
ls -la | grep build  # Should only show "build"
```

---

## Folder-by-Folder Breakdown

### 🔴 CORE (Keep — Part of v0.6.0-rc1)

#### `src/` — Main Implementation
```
src/
├── db.cpp                    ← Main database engine
├── db_ledger.cpp            ← Ledger/WAL implementation
├── lattice_placement.cpp    ← Lattice coordinate system
├── dialectic.cpp            ← Dialectic reasoning engine
├── epistemic.cpp            ← Epistemological tracking
└── platform_*.cpp           ← OS-specific (Windows, POSIX)
```
**Keep:** Always. This is the database.

#### `include/graphene/` — Public API
```
include/graphene/
├── db.hpp                   ← Main DB interface
├── types.hpp                ← Data structures
├── lattice_placement.hpp    ← Lattice APIs
└── *.hpp                    ← Other public interfaces
```
**Keep:** Always. This is the contract.

#### `tools/` — Operational Tools
```
tools/
├── graphenedb_cli.cpp       ← Operator CLI (fixed in this session)
├── graphenedb_cli_part_*.inc ← CLI modules
└── graphenedb_server.cpp    ← (Future) server mode
```
**Keep:** Always. Critical for ops & testing.

#### `tests/` — Test Suite
```
tests/
├── test_durability.cpp      ← ACID guarantees
├── test_lattice.cpp         ← Lattice retrieval
├── test_extraction_ingest.cpp ← External data ingestion
└── [20+ others]
```
**Keep:** Always. Validates release quality.

#### `docs/` — Documentation
```
docs/
├── QUICK_START_GUIDE.md     ← (Created this session)
├── ARCHITECTURE.md          ← System design
├── LATTICE_RETRIEVAL.md     ← How retrieval works
└── [others]
```
**Keep:** Always. Essential for users.

#### `scripts/` — Automation
```
scripts/
├── build.sh / build.ps1     ← Build automation
├── run_release_candidate_bundle.sh
├── verify_package_install.sh
└── [CI/CD helpers]
```
**Keep:** Always. Needed for reproducible builds.

---

### 🟡 OPERATIONAL (Keep — Active Use)

#### `reports/` — Release Evidence (Intentional)
```
reports/
├── GA_STATUS_REPORT.md      ← Current status (this session)
├── PHASE_1_*.md             ← Pilot materials (this session)
├── DATABASE_HANDS_ON_*.md   ← Testing report (this session)
├── ga-readiness/            ← Snapshot from 2026-07-07
├── ga-evidence/             ← Test artifacts
└── preview-hardware/        ← Hardware profile
```
**Keep:** Yes, but sparingly. These are **intentional evidence artifacts** tied to releases. Don't clutter with every run.

#### `.github/` — CI/CD
```
.github/workflows/
├── ci.yml          ← Main test pipeline
├── docker-*.yml    ← Container workflows
└── [others]
```
**Keep:** Always. Drives quality gates.

---

### 🟢 OPTIONAL (Can Be Archived/Cleaned)

#### `benchmarks/` — Historical Performance Data
```
benchmarks/
├── 2wiki/          ← Old benchmark dataset
├── cross_dataset/  ← Cross-dataset results
└── [older perf runs]
```
**Action:** Archive to S3 or external storage. Keep only the latest 1-2 runs in repo.

#### `paper/` — Research Whitepaper
```
paper/
├── graphene_lattice_model.pdf
└── [research materials]
```
**Action:** Move to wiki or doc archive. Repo should be code+tests, not papers.

#### `fuzz/` — Fuzzing Corpus
```
fuzz/
├── crash_artifacts/
└── [seed corpus]
```
**Action:** Archive crashes to S3. Keep seed corpus only if actively fuzzing.

#### `deploy/` — Cloud Deployment
```
deploy/
├── docker/
├── kubernetes/
└── [cloud templates]
```
**Action:** Move to separate `deployment-templates` repo or wiki. Repo is library, not ops.

---

## Cleanup Command (Safe)

```bash
cd /c/Users/vrast/Downloads/GrapheneDB/graphenedb

# 1. List what would be deleted (dry run)
echo "Build directories to remove:"
ls -d build-* build_test 2>/dev/null | wc -l

# 2. Actually remove local build artifacts
rm -rf build-* build_test graphify-out tmp

# 3. Create ONE canonical build
mkdir -p build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && make -j8

# 4. Verify
ls -la | grep -E "^d.*build"  # Should only show "build"
```

---

## Going Forward: Repository Hygiene

### Rules for Keeping Repo Clean

1. **No local build directories in git** ✅ (Already in .gitignore)
2. **One active build directory locally** → Use `./build/`
3. **Commit only essential evidence** → Reports tied to releases, not every test run
4. **Archive old data** → Benchmarks, fuzz crashes → S3/external storage
5. **Keep source, tests, docs; archive papers, old configs**

### Suggested .gitignore Additions

```bash
# Add to .gitignore if not present
graphify-out/       # Knowledge graph output
benchmarks-old/     # Archived benchmark data
*.db/               # Local test databases
fuzz/crash*/        # Fuzzing crash artifacts
```

---

## Current Session State

**What was created in this session (keep in repo):**
- ✅ `PHASE_1_PILOT_CHARTER.md`
- ✅ `PHASE_1_TESTING_WORKBOOK.md`
- ✅ `PHASE_1_TEAM_HANDOFF.md`
- ✅ `PHASE_1_INTERNAL_TEAMS_INVITATION.md`
- ✅ `HANDOFF_STATUS_SUMMARY.md`
- ✅ `DATABASE_HANDS_ON_EXPERIENCE.md`
- ✅ `WIDER_SHARING_READINESS.md` (created earlier)
- ✅ `docs/QUICK_START_GUIDE.md` (created earlier)
- ✅ CLI error handling fix (committed to master)

**What should be cleaned (local only):**
- ❌ `build-*` directories (20+ of them, take up GBs)
- ❌ `build_test/` (created this session, can be rebuilt anytime)
- ❌ `graphify-out/` (knowledge graph output, regenerable)
- ❌ `tmp/` (scratch, not needed)

---

## Recommendation Summary

**👉 Run this NOW:**

```bash
cd /c/Users/vrast/Downloads/GrapheneDB/graphenedb

# Remove old builds
rm -rf build-* build_test graphify-out tmp

# Create single canonical build
mkdir build && cd build && cmake .. && make -j8

# Verify
git status  # Should show a clean tree (no untracked build dirs)
```

**Result:** Repo is clean, clear, and ready for Phase 1 pilot handoff.

---

**After this cleanup:**
- 📁 Disk space freed: ~2-5 GB (depending on build size)
- 🎯 Clarity: One build directory, easy to find binaries
- 📋 Maintainability: Clear which folders are source vs. artifacts vs. optional
- 🚀 Readiness: v0.6.0-rc1 in clean, professional state
