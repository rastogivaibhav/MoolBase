# GrapheneDB Phase 1 Pilot Charter

**Release:** v0.6.0-rc1 (Developer Preview)  
**Date:** August 2026  
**Status:** Active Pilot

## Objective

Validate GrapheneDB's core claims through real-world usage, identify breaking issues early, and iterate collaboratively to fix them before wider rollout.

## What We're Testing

### Core Claims to Validate

1. **Durability**
   - WAL crash recovery works as specified
   - Data persists across process restarts
   - Corrupt writes don't silently succeed
   - ACID semantics on put_batch/put_node/put_edge

2. **Causal Retrieval**
   - Causal edges correctly model dependencies
   - Retrieval respects causal ordering
   - Contradicts/supersedes edges work correctly
   - Reasoning across causal chains produces expected results

3. **Lattice-Aware Storage**
   - Lattice coordinates place related memories efficiently
   - Neighbor queries honor lattice topology
   - Defect types don't corrupt queries
   - Layer coupling (same-layer, van der Waals, twisted) behaves as spec'd

4. **Vector Indexing**
   - Similarity search returns correct top-k
   - Vector recall >= 95% for all index kinds (flat, KDTree, FAISS)
   - Index doesn't silently degrade under scale
   - Extraction and storage vectors index independently

5. **Extraction Ingestion**
   - External source IDs map to node IDs correctly
   - Incident metadata chains preserve root→symptom→impact roles
   - Batch ingestion doesn't drop nodes or edges
   - Duplicate external IDs are handled correctly

6. **API Usability**
   - C++ API is intuitive for AI memory use cases
   - CLI is usable for operational inspection/debugging
   - Error messages are actionable
   - Documentation matches actual behavior

## How Pilots Work

### 1. Your Job
- Build something with GrapheneDB (or use it in existing workflow)
- Push it hard—stress test it with your real data
- When it breaks, document:
  - Exact reproduction steps
  - Input data
  - Expected vs. actual behavior
  - System info (OS, RAM, data size)

### 2. Our Job
- Respond within 1 business day
- Triage: data loss vs. performance vs. API confusion
- Fix critical issues collaboratively
- Ship fixes in weekly patches

### 3. Iterate Together
- You test the fix
- Report findings
- We iterate until stable
- Document the learning

## Success Criteria

**Pilot is successful if:**
- [ ] At least one team ingests 100k+ nodes without data loss
- [ ] Causal queries produce expected results (pilot team verifies)
- [ ] Vector search recall >= 95% at pilot data scale
- [ ] All crashes/hangs have root cause identified
- [ ] No silent data corruption discovered
- [ ] CLI/API docs match actual behavior

**Pilot is ready for Phase 2 if:**
- [ ] 0 unfixed critical issues (data loss, crashes)
- [ ] Pilot teams feel confident in durability claims
- [ ] At least one pilot team successfully uses in production prototype

## Timeline

- **Week 1:** Team onboarding, initial testing
- **Week 2-3:** Stress testing, issue discovery & fixes
- **Week 4:** Final validation, Phase 2 go/no-go decision

## Issue Escalation

**Critical (Fix ASAP):**
- Data loss or corruption
- Crash on valid input
- Silent wrong results

**High (Fix this week):**
- Performance degradation under load
- API behavior doesn't match docs
- Usability blocker

**Medium (Fix next release):**
- Edge case edge types
- Nice-to-have features
- Documentation gaps

## Support Channels

- **Slack/Email:** Direct message for quick questions
- **GitHub Issues:** For tracked bugs (tag with `phase-1-pilot`)
- **Weekly sync:** Thursdays 2pm UTC to share learnings

## What's NOT in Scope for Phase 1

- Server mode (use embedded library only)
- Distributed/replicated deployments
- Enterprise-grade monitoring/ops tooling
- Performance tuning beyond functional correctness

---

**Next step:** Teams complete the [Phase 1 Testing Workbook](./PHASE_1_TESTING_WORKBOOK.md)
