# Phase 1 Pilot Team Handoff

> **Historical pilot guidance (August 2026).** This document preserves the v0.6.0-rc1 Phase 1 workflow. Current newcomers should start with [Developer quickstart](docs/DEVELOPER_QUICKSTART.md) and the [MoolBase Evidence Lab](https://moolbase.rasvai.com).

**GrapheneDB v0.6.0-rc1** — Developer Preview for Controlled Testing

---

## Quick Start (5 min)

```bash
# Clone and build
git clone https://github.com/rastogivaibhav/MoolBase.git
cd MoolBase
./scripts/build.sh  # or PowerShell on Windows

# Create a test database
./build/graphenedb_cli init /tmp/my_db.db 768

# Store two memories and link them
./build/graphenedb_cli put-node /tmp/my_db.db 768 \
  "service-x-crashed" "0.1,0.2,0.3" 111 root

./build/graphenedb_cli put-node /tmp/my_db.db 768 \
  "timeout-errors" "0.2,0.3,0.4" 222 symptom

./build/graphenedb_cli put-edge /tmp/my_db.db 768 111 222 causal

# Retrieve and validate
./build/graphenedb_cli inspect /tmp/my_db.db 768
./build/graphenedb_cli neighbors /tmp/my_db.db 768 111
./build/graphenedb_cli validate /tmp/my_db.db 768
```

---

## What You're Testing

| Claim | How to Test |
|-------|------------|
| **Durability** | Store data, kill process, restart, verify data intact |
| **Causal Retrieval** | Create incident chain (root→symptom→impact), query neighbors, verify ordering |
| **Vector Search** | Store nodes with embeddings, search for similar, verify recall >= 95% |
| **Scale** | Import 100k nodes, verify no crashes or data loss |
| **Extraction** | Create extraction batch with incident roles, verify metadata preserved |
| **API** | Write C++ program using headers, compile, run |

**Detailed test steps:** See [PHASE_1_TESTING_WORKBOOK.md](./PHASE_1_TESTING_WORKBOOK.md)

---

## Known Limitations (Not Tested Yet)

- ❌ **Server mode** — Only embedded library (v0.6.0-rc1)
- ❌ **Replication** — Single-machine only
- ❌ **Ops tooling** — No built-in monitoring
- ❌ **Backward compat** — v0.6.0-rc1 may not support older databases
- ⚠️ **Windows support** — CLI tested, server not yet

See [WIDER_SHARING_READINESS.md](./WIDER_SHARING_READINESS.md) for full limitations.

---

## How to Report Issues

**Format:**
```
Title: [CLI/API/Durability/Scaling] What broke

Reproduction:
1. Step 1
2. Step 2
3. ...

Expected: [what should happen]
Actual: [what happened]

Environment: OS, data size, build date
```

**Channels:**
- **Slack:** Direct message for urgent issues (crashes, data loss)
- **GitHub Issues:** Tag with `phase-1-pilot`
- **Weekly sync:** Thursdays to share findings

**Response SLA:**
- Critical (data loss, crashes): 24 hours
- High (API doesn't match docs): 1 week
- Medium (performance, nice-to-have): next release

---

## Success Criteria for Phase 1

✅ **Pilot succeeds if:**
- You can store and retrieve 100k+ nodes without crashes
- Durability claims hold (data persists, crash-recovers)
- Causal queries produce expected results
- Vector search recall >= 95%
- You feel confident in the core promises

❌ **Pilot fails if:**
- Silent data loss or corruption
- Crashes on valid input
- Claims in docs don't match behavior

---

## What's Next

**Week 1:** Setup + initial tests (Test 1-3)  
**Week 2-3:** Scale + edge cases (Test 4-7)  
**Week 4:** Final validation + Phase 2 go/no-go  

---

## Links

- **Docs:** [QUICK_START_GUIDE.md](./docs/QUICK_START_GUIDE.md)
- **API Reference:** `include/graphene/db.hpp` (well-commented header)
- **Architecture:** [ARCHITECTURE.md](./docs/ARCHITECTURE.md)
- **Limitations:** [WIDER_SHARING_READINESS.md](./WIDER_SHARING_READINESS.md)
- **Testing Plan:** [PHASE_1_TESTING_WORKBOOK.md](./PHASE_1_TESTING_WORKBOOK.md)
- **Release Notes:** [v0.6.0-rc1 tag](https://github.com/rastogivaibhav/MoolBase/releases/tag/v0.6.0-rc1)

---

## Contact

- **Questions/Issues:** [Support channels above]
- **Weekly Sync:** Thursdays 2pm UTC
- **Feedback:** We iterate based on what you find

**Let's build something durable together.**
