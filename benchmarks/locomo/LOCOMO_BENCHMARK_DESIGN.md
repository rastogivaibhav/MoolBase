# GrapheneDB vs. LoCoMo: Long-Term Conversational Memory Benchmark

**Objective:** Validate GrapheneDB's effectiveness on long-term conversational memory retrieval using the ACL 2024 LoCoMo benchmark dataset.

**Dataset:** 10 very long conversations with:
- Multiple sessions per conversation (5-7 sessions each)
- 3-7 hours of conversation per conversation
- Annotated Q&A tasks (ground truth answers)
- Event summaries with temporal/causal structure

---

## Benchmark Design

### 1. Data Ingestion Pipeline

**Convert LoCoMo → GrapheneDB:**

```
For each conversation:
  For each session:
    → Node: SessionStarted (timestamp, participants)
    
    For each message turn:
      → Node: Message (speaker, content, timestamp, embedding)
      → Edge: FollowsInTime (previous_message → this_message)
      → Edge: SpokenBy (message → speaker)
    
    → Node: SessionSummary (generated, timestamp)
    → Edge: Summarizes (summary → session messages)
    
    For each annotated event:
      → Node: Event (description, causal role, affected entities)
      → Edge: CausedBy (event → previous events)
      → Edge: OccurredIn (event → session)

For each annotated Q&A:
  → Node: Question (text, embedding)
  → Node: Answer (text, embedding)
  → Edge: AnsweredBy (question → messages/summaries containing answer)
```

**Result:** A causal graph of conversations where:
- Temporal ordering is explicit (FollowsInTime edges)
- Semantic relationships are captured (embeddings)
- Causal chains are encoded (CausedBy edges)
- Q&A ground truth is linked

---

### 2. Test Scenarios

#### Scenario A: Simple Message Retrieval
**Task:** "Find messages from Alice about cooking"

**Metric:** 
- Precision: % of retrieved messages actually about cooking
- Recall: % of ground-truth cooking messages retrieved
- Latency: Query response time

**Query Type:** Vector similarity + temporal constraints

#### Scenario B: Question Answering (Core LoCoMo Task)
**Task:** Given a question + conversation history, retrieve the answer

**Example:**
```
Question: "What restaurant did Alice go to in session 3?"
Ground Truth Evidence: [message_id_42, message_id_59]
Database: GrapheneDB (only has messages, summaries, no explicit QA pairs)
```

**Retrieval Strategy:**
1. Embed the question
2. Search for semantically similar messages
3. Traverse causal chains to find connected context
4. Rank by relevance + temporal proximity

**Metric:**
- Recall@1, @5, @10: Did correct answer appear in top-K?
- MRR (Mean Reciprocal Rank): How high was correct answer?
- Latency: Time to find answer from 3-7 hour conversation

#### Scenario C: Event Summarization (Causal Reasoning)
**Task:** "Summarize events where Alice was the protagonist"

**Ground Truth:** Annotated event summaries

**Retrieval Strategy:**
1. Find events where Alice is main actor
2. Traverse CausedBy edges to find root causes
3. Follow effects forward to find impacts
4. Generate summary of causal chain

**Metric:**
- ROUGE score against ground-truth summary
- Causal chain accuracy: Correct order + causality?
- Completeness: % of important events included

#### Scenario D: Temporal Reasoning
**Task:** "What happened between session 2 and session 5?"

**Retrieval Strategy:**
1. Filter messages by timestamp range
2. Extract key events
3. Build causal chains within time window

**Metric:**
- Temporal accuracy: Correct ordering?
- Event coverage: How many important events retrieved?

---

### 3. Performance Metrics

#### Accuracy Metrics
| Metric | Scenario | Target |
|--------|----------|--------|
| Recall@5 (Q&A) | B | >= 80% (beat GPT-3.5 retrieval?) |
| MRR (Q&A) | B | >= 0.85 |
| ROUGE-1 (Summarization) | C | >= 0.60 |
| Event Order Accuracy | C,D | >= 95% |

#### Latency Metrics
| Operation | Target | Notes |
|-----------|--------|-------|
| Ingest 1 message | < 1 ms | Per-message latency |
| Query (similarity) | < 100 ms | Over full 7-hour conversation |
| Query (causal) | < 200 ms | With multi-hop traversal |
| Retrieve answer (Q&A) | < 500 ms | End-to-end for one question |

#### Throughput
| Operation | Target |
|-----------|--------|
| Ingest rate | >= 1000 msg/sec |
| Query throughput | >= 100 queries/sec |

#### Resource Usage
| Metric | Target | Notes |
|--------|--------|-------|
| Memory per conversation | <= 500 MB | For 7-hour conversation (~50k messages) |
| Disk per conversation | <= 100 MB | Compressed WAL + index |
| Index rebuild time | <= 30 sec | After new data ingestion |

---

### 4. Baseline Comparisons

#### Baseline 1: Vector-Only Search (FAISS)
- Embed all messages
- Use FAISS for similarity search
- No causal reasoning

**Expected Result:** Good recall on simple retrieval, poor on causal/event tasks

#### Baseline 2: BM25 (Full-Text Search)
- Index all messages with BM25
- Keyword search

**Expected Result:** Exact match for keywords, misses semantic similarity

#### Baseline 3: LLM Context Window (GPT-4)
- Load entire conversation into context
- Let LLM retrieve answers

**Expected Result:** Perfect accuracy (it can read everything), but expensive + slow

#### Baseline 4: LoCoMo Paper Results
- Published retrieval results from ACL 2024 paper
- Various RAG approaches

**Expected Result:** Understand where GrapheneDB fits

---

## Implementation Plan

### Phase 1: Data Preparation (Week 1)
```
1. Download LoCoMo dataset (locomo10.json)
2. Generate embeddings for all messages
   - Use OpenAI, HuggingFace, or local embedding model
3. Build ingestion script
   - Parse JSON → Node/Edge structures
   - Assign IDs, timestamps, roles
4. Load into GrapheneDB
   - Target: All 10 conversations (~500k messages)
```

### Phase 2: Query Implementation (Week 2)
```
1. Implement Q&A retrieval queries
2. Implement event summarization queries
3. Implement temporal range queries
4. Benchmark each scenario
```

### Phase 3: Analysis & Reporting (Week 3)
```
1. Compute accuracy metrics
2. Compare to baselines
3. Write findings report
4. Create visualizations
```

---

## Expected Outcomes

**Best Case:**
- GrapheneDB achieves >= 80% Recall@5 on Q&A
- Latency < 500ms per query
- Outperforms vector-only baseline on causal tasks
- Clearly demonstrates explainability advantage

**Realistic Case:**
- GrapheneDB achieves 70-75% Recall@5
- Latency 200-300ms on simple queries
- Equivalent to vector search on recall, better on reasoning
- Shows promise for specialized use case

**Learning Case:**
- GrapheneDB discovers limitations
- Identifies optimizations needed
- Provides roadmap for Phase 2

---

## Code Structure

```
benchmarks/locomo/
├── README.md                          # This benchmark guide
├── download_locomo.py                 # Fetch dataset
├── ingest_locomo_to_graphenedb.py    # Convert → DB
├── queries/
│   ├── qa_retrieval.py               # Simple Q&A
│   ├── event_summarization.py        # Causal chains
│   └── temporal_reasoning.py          # Time-based
├── baselines/
│   ├── faiss_vector_search.py        # Vector baseline
│   ├── bm25_search.py                # FTS baseline
│   └── gpt4_context_window.py        # LLM baseline
├── benchmark.py                       # Main harness
├── metrics.py                         # Accuracy/perf calculations
└── report.md                          # Results & analysis
```

---

## Success Criteria for Phase 1 Pilot

| Criteria | Pass | Partial | Fail |
|----------|------|---------|------|
| **Ingest 500k messages** | < 10 min | 10-30 min | > 30 min |
| **Recall@5 (Q&A)** | >= 75% | 60-75% | < 60% |
| **Query latency** | < 300ms | 300-1000ms | > 1000ms |
| **Memory usage** | <= 2 GB | 2-5 GB | > 5 GB |
| **Causal reasoning** | Beats vector baseline | Equivalent | Worse |
| **Explainability** | Clear reasoning shown | Partial | None |

---

## Questions to Answer

1. **Does GrapheneDB scale to LoCoMo?**
   - Can it handle 500k+ messages from 10 conversations?
   - What's the ingestion rate?

2. **How does recall compare to vector search?**
   - Better? Worse? Comparable?
   - Where does it win/lose?

3. **Is causal reasoning useful?**
   - Do CausedBy edges improve event summarization?
   - Do temporal chains help with reasoning?

4. **What's the real-world performance?**
   - Latency vs. throughput tradeoffs
   - Memory footprint for production use

5. **How explainable is retrieval?**
   - Can users understand why a message was returned?
   - Is the reasoning trust-worthy?

---

## Timeline

- **Week 1:** Data prep + ingestion
- **Week 2:** Query implementation + baseline setup
- **Week 3:** Benchmarking + analysis
- **Week 4:** Report + Phase 1 pilot results

---

## Next Steps

1. Confirm benchmark scope with user
2. Set up Phase 1 benchmark environment
3. Run preliminary ingest on small sample
4. Identify any blocking issues early
5. Proceed to full benchmark
