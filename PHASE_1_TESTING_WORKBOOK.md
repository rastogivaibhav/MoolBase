# Phase 1 Pilot Testing Workbook

**Team:** [Your name/org]  
**Start Date:** ___________  
**GrapheneDB Version:** v0.6.0-rc1  

---

## Setup Checklist

- [ ] Cloned graphenedb repo
- [ ] Built from source successfully: `scripts/build.sh` or PowerShell script
- [ ] Created test database: `graphenedb_cli init /tmp/test.db 768`
- [ ] Ran one quick test command to verify CLI works
- [ ] Read [QUICK_START_GUIDE.md](./docs/QUICK_START_GUIDE.md)
- [ ] Reviewed [WIDER_SHARING_READINESS.md](./WIDER_SHARING_READINESS.md) (especially limitations)

---

## Test 1: Durability - Process Restart

**Objective:** Verify data persists across process crashes

**Steps:**
1. Create a database and store 10 nodes with distinct content
2. Query all nodes back to verify they exist
3. Kill the process (or close the DB and reopen)
4. Query again—all 10 nodes should still be there
5. Modify one node's metadata, close, reopen, verify modification persists

**Expected Result:**
- [ ] All nodes survive restart
- [ ] Modifications persist
- [ ] No data loss or corruption

**Issues Found:**
```
(Describe any data loss, crashes, or unexpected behavior)
```

---

## Test 2: Durability - Crash Recovery

**Objective:** Verify the database recovers from an unexpected crash during write

**Steps:**
1. Start writing 100 nodes in a batch
2. Interrupt the process mid-write (kill -9 or force kill)
3. Reopen the database
4. Verify the database still works (no corruption)
5. Check node count: should be either pre-crash count OR post-crash count (not partially written)

**Expected Result:**
- [ ] Database opens without errors after crash
- [ ] Node count is consistent (not corrupted)
- [ ] No orphaned edges or malformed metadata

**Issues Found:**
```
(Describe recovery failures or inconsistencies)
```

---

## Test 3: Causal Retrieval - Dependency Chain

**Objective:** Verify causal edges correctly model dependencies

**Steps:**
1. Create 3 nodes: A (root issue), B (symptom), C (solution)
2. Create edges: A→B (causal), B→C (causal)
3. Query neighbors of A with max_hops=2
4. Verify you get B and C in the results with correct causal distance

**Expected Result:**
- [ ] Neighbor query returns B at hop 1, C at hop 2
- [ ] Causal direction is respected
- [ ] Edge strength/metadata preserved

**Issues Found:**
```
```

---

## Test 4: Causal Retrieval - Contradicts/Supersedes

**Objective:** Verify non-causal edge types work correctly

**Steps:**
1. Create 3 nodes: X (old approach), Y (new approach), Z (evaluation)
2. Create edges: X→Y (supersedes), X→Z (contradicts)
3. Query neighbors of X
4. Verify edges are labeled correctly and retrievable

**Expected Result:**
- [ ] Non-causal edges are stored and retrieved
- [ ] Edge type is preserved
- [ ] Queries can filter by edge type if needed

**Issues Found:**
```
```

---

## Test 5: Vector Search - Similarity

**Objective:** Verify vector similarity search returns semantically close nodes

**Steps:**
1. Store 20 nodes with vectors (e.g., embeddings for tech topics)
   - Node A: embedding for "database"
   - Node B: embedding for "storage"
   - Node C: embedding for "cooking recipe"
2. Search for a vector close to "database"
3. Verify A and B appear in top results, C does not

**Expected Result:**
- [ ] Top results are semantically similar
- [ ] Irrelevant nodes rank low
- [ ] Recall >= 95% (i.e., if there are 5 truly similar nodes, top-5 returns all 5)

**Issues Found:**
```
```

---

## Test 6: Scale Test - 100k Nodes

**Objective:** Verify the database handles 100k node ingestion without crashes or data loss

**Steps:**
1. Prepare a TSV file with 100k nodes (use extract/import-tsv CLI)
2. Import: `graphenedb_cli import-tsv /db/scale-test.db 768 nodes.tsv`
3. Verify import completes without errors
4. Query: `graphenedb_cli inspect /db/scale-test.db 768`
5. Verify node_count == 100k
6. Run random searches and neighbors queries
7. Validate: `graphenedb_cli validate /db/scale-test.db 768`

**Expected Result:**
- [ ] Import completes in <5 min (depends on hardware)
- [ ] Node count matches import
- [ ] Validate passes with no errors
- [ ] Random queries complete in <100ms

**Issues Found / Performance Notes:**
```
(Include import duration, DB file size, validation results)
```

---

## Test 7: Extraction Ingestion - Incident Metadata

**Objective:** Verify extraction API preserves incident chains

**Steps:**
1. Create an extraction with source="incident-123":
   - Root node: "service-X-outage"
   - Symptom nodes: "timeout-errors", "high-latency"
   - Impact nodes: "customer-A-affected", "customer-B-affected"
   - Relations between them
2. Call `put_extraction()` with the batch
3. Query nodes by external ID: "incident-123/timeout-errors"
4. Verify the node exists and metadata is preserved

**Expected Result:**
- [ ] Extraction completes without errors
- [ ] Nodes are created with correct roles (root/symptom/impact)
- [ ] External IDs are correctly mapped
- [ ] Relations are preserved

**Issues Found:**
```
```

---

## Test 8: API Usability - C++ Integration

**Objective:** Verify the C++ API is intuitive for AI memory workflows

**Steps:**
1. Read the C++ API header: `include/graphene/db.hpp`
2. Write a small program (~50 lines) that:
   - Opens a DB
   - Stores 5 nodes
   - Queries neighbors
   - Closes gracefully
3. Try to infer what the API does from the headers alone
4. Test if your program compiles and works

**Expected Result:**
- [ ] API is understandable without external docs
- [ ] Compiler errors are clear
- [ ] Program runs without unexpected behavior

**Issues / Confusions:**
```
(Any API inconsistencies, unclear parameter names, missing functions)
```

---

## Test 9: Error Handling - Malformed Input

**Objective:** Verify the CLI handles bad input gracefully

**Steps:**
1. Try: `graphenedb_cli put-node /db 768 "content" "0.1,0.2" "not-a-number"`
2. Expected: Error message, exit code 2, no crash
3. Try: `graphenedb_cli put-edge /db 768 invalid_from invalid_to`
4. Expected: Error message, exit code 2, no crash
5. Try: `graphenedb_cli init /nonexistent/path 768`
6. Expected: Error about path, exit code 2, no crash

**Expected Result:**
- [ ] All malformed inputs produce error messages (not crashes)
- [ ] Exit codes are consistent
- [ ] Error messages hint at the fix

**Issues Found:**
```
(Any crashes, unclear error messages, missing validation)
```

---

## Test 10: Documentation Accuracy

**Objective:** Verify the docs match the actual behavior

**Steps:**
1. Pick 3 commands from the CLI usage
2. Follow the docs exactly
3. Verify the output matches the documentation

**Expected Result:**
- [ ] Docs are accurate
- [ ] Examples work as written
- [ ] No surprises in actual behavior

**Issues Found:**
```
(Outdated docs, wrong examples, missing flags)
```

---

## Summary

**Tests Completed:** _____ / 10  
**Critical Issues Found:** _____  
**High Issues Found:** _____  
**Data Loss Observed:** [ ] No [ ] Yes (explain below)  

**Overall Assessment:**
```
(Would you trust this in a real system? Why or why not?)
```

**Next Steps / Blockers:**
```
(What's needed to continue testing or deploy to production?)
```

---

**Please share this completed workbook with us. We'll review, triage issues, and iterate with you.**
