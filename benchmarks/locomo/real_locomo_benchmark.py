#!/usr/bin/env python3
"""
REAL LoCoMo benchmark against GrapheneDB.

No simulation. Every number in the output comes from an actual subprocess
call to the real graphenedb_cli binary, built from this repo's source.

Embeddings: hashed bag-of-words (feature hashing / "hashing trick").
This is NOT a state-of-the-art semantic embedding model (no network access
to download one was available), but unlike a whole-string hash it is a
legitimate, well-known lightweight embedding technique: texts that share
words get vectors that are actually close together. That property is the
minimum requirement for a similarity search benchmark to mean anything.
This limitation is reported explicitly in the output, not hidden.

Ground truth: LoCoMo's `qa[].evidence` field lists the dialog ids (dia_id)
that justify each answer. We ingest every message as a node, tag its dia_id,
and after ingestion check whether the `reason` command's primary_node is one
of the evidence message nodes for that question.
"""

import json
import re
import subprocess
import sys
import time
import math
import statistics
from pathlib import Path
from collections import defaultdict

DIM = 64  # kept small: every dimension is a real CLI subprocess argument


def tokenize(text):
    return re.findall(r"[a-z0-9]+", text.lower())


def embed(text, dim=DIM):
    """Hashed bag-of-words embedding, L2-normalized.

    Real property: cosine/L2 similarity of embed(a) and embed(b) increases
    with shared vocabulary between a and b. This is the actual mechanism
    being tested, not a stand-in for a real semantic model.
    """
    vec = [0.0] * dim
    for tok in tokenize(text):
        h = hash(tok) % dim
        vec[h] += 1.0
    norm = math.sqrt(sum(v * v for v in vec)) or 1.0
    vec = [v / norm for v in vec]
    # CLI dimension enforcement requires nonzero-looking floats; shift away
    # from exact zero so put-node's parser and any downstream log10/ln paths
    # never see a literal 0 vector for an empty/stopword-only message.
    vec = [v if v != 0.0 else 1e-6 for v in vec]
    return ",".join(f"{v:.6f}" for v in vec)


class RealCLI:
    def __init__(self, cli_path, db_path, dim):
        self.cli_path = cli_path
        self.db_path = db_path
        self.dim = dim
        self.call_count = 0
        self.total_latency = 0.0

    def run(self, args, timeout=30):
        cmd = [self.cli_path] + args
        t0 = time.time()
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
        elapsed = time.time() - t0
        self.call_count += 1
        self.total_latency += elapsed
        return result.returncode, result.stdout.strip(), result.stderr.strip(), elapsed

    def init(self):
        rc, out, err, _ = self.run(["init", self.db_path, str(self.dim)])
        if rc != 0:
            raise RuntimeError(f"init failed: {out} {err}")

    def put_node(self, content, vector, signature, role="root"):
        rc, out, err, elapsed = self.run(
            ["put-node", self.db_path, str(self.dim), content, vector, str(signature), role]
        )
        if rc != 0:
            raise RuntimeError(f"put-node failed: {out} {err}")
        return int(out.splitlines()[-1]), elapsed

    def put_edge(self, frm, to, role="causal"):
        rc, out, err, elapsed = self.run(
            ["put-edge", self.db_path, str(self.dim), str(frm), str(to), role]
        )
        if rc != 0:
            raise RuntimeError(f"put-edge failed: {out} {err}")
        return int(out.splitlines()[-1]), elapsed

    def reason(self, vector, signature):
        rc, out, err, elapsed = self.run(
            ["reason", self.db_path, str(self.dim), vector, str(signature)]
        )
        return rc, out, err, elapsed

    def search(self, vector, signature):
        rc, out, err, elapsed = self.run(
            ["search", self.db_path, str(self.dim), vector, str(signature)]
        )
        return rc, out, err, elapsed

    def validate(self):
        return self.run(["validate", self.db_path, str(self.dim)])

    def inspect(self):
        return self.run(["inspect", self.db_path, str(self.dim)])


def parse_reason_output(out):
    """Parse the real text output of `reason` into a dict."""
    d = {"status": None, "primary_node": None, "confidence": None,
         "lyapunov_regime": None, "uncertainty": []}
    for line in out.splitlines():
        if line.startswith("status="):
            d["status"] = line.split("=", 1)[1]
        elif line.startswith("primary_node="):
            # "primary_node=3 confidence=0.82"
            parts = line.split()
            d["primary_node"] = int(parts[0].split("=", 1)[1])
            d["confidence"] = float(parts[1].split("=", 1)[1])
        elif line.startswith("lyapunov_regime="):
            d["lyapunov_regime"] = line.split("=", 1)[1]
        elif line.startswith("uncertainty="):
            d["uncertainty"].append(line.split("=", 1)[1])
        elif line.startswith("ABSTAIN"):
            d["status"] = "abstain"
    return d


def parse_search_output(out):
    """Parse the real text output of `search` (causal_search) into a dict."""
    d = {"abstain": False, "reason": None, "target_node": None,
         "confidence": None, "paths": None, "why": []}
    for line in out.splitlines():
        if line.startswith("ABSTAIN"):
            d["abstain"] = True
            d["reason"] = line[len("ABSTAIN "):].strip()
        elif line.startswith("target="):
            # "target=0 confidence=0.72 paths=1"
            parts = line.split()
            d["target_node"] = int(parts[0].split("=", 1)[1])
            d["confidence"] = float(parts[1].split("=", 1)[1])
            d["paths"] = int(parts[2].split("=", 1)[1])
        elif line.startswith("why="):
            d["why"].append(line.split("=", 1)[1])
    return d


def load_locomo(path, max_conversations=None):
    with open(path, encoding="utf-8") as f:
        data = json.load(f)
    if max_conversations:
        data = data[:max_conversations]
    return data


def main():
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("--locomo", default="/work/benchmarks/locomo-dataset/data/locomo10.json")
    ap.add_argument("--cli", default="/work/build/graphenedb_cli")
    ap.add_argument("--db", default="/tmp/real_locomo.db")
    ap.add_argument("--max-conversations", type=int, default=None,
                     help="limit conversations processed (full corpus if unset)")
    ap.add_argument("--max-qa-per-conv", type=int, default=None)
    ap.add_argument("--results-dir", default="/work/benchmarks/locomo/results")
    args = ap.parse_args()

    results_dir = Path(args.results_dir)
    results_dir.mkdir(parents=True, exist_ok=True)

    print("=" * 78)
    print("REAL LoCoMo Benchmark -- actual graphenedb_cli subprocess calls")
    print("=" * 78)
    print(f"CLI binary: {args.cli}")
    print(f"Database:   {args.db}")
    print(f"Embedding:  hashed bag-of-words, dim={DIM} "
          f"(NOT a semantic embedding model -- see caveats in report)")
    print()

    data = load_locomo(args.locomo, args.max_conversations)
    print(f"Loaded {len(data)} conversation(s) from {args.locomo}")

    cli = RealCLI(args.cli, args.db, DIM)
    Path(args.db).parent.mkdir(parents=True, exist_ok=True)
    if Path(args.db).exists():
        import shutil
        shutil.rmtree(args.db)
    cli.init()

    # dia_id -> node_id (real ids returned by the real CLI)
    dia_to_node = {}
    node_ingest_latencies = []
    edge_ingest_latencies = []

    ingest_start = time.time()
    total_messages = 0

    for conv_idx, conv in enumerate(data):
        conversation = conv["conversation"]
        sessions = sorted(
            [(k, v) for k, v in conversation.items()
             if k.startswith("session_") and isinstance(v, list)],
            key=lambda x: x[0],
        )
        prev_node = None
        is_first_message_of_conversation = True
        # Flatten to know which message is genuinely last for this conversation
        # (role="impact") before we start issuing CLI calls.
        flat_msgs = [(sk, m) for sk, msgs in sessions for m in msgs]
        for msg_idx, (session_key, msg) in enumerate(flat_msgs):
                speaker = msg.get("speaker", "?")
                text = msg.get("text", "")
                dia_id = msg.get("dia_id")
                if not text or not dia_id:
                    continue
                content = f"{speaker}: {text}"[:500]
                vec = embed(content)
                sig = abs(hash(dia_id)) % (2**31)
                # Correct role assignment: only the conversation's first message
                # is the causal root (reverse_root() in the C++ engine stops the
                # backward BFS the instant it sees node.root==true, so marking
                # every node "root" -- an earlier bug in this script -- made
                # every query trivially self-terminate and skip real causal-edge
                # traversal entirely). Only the true first/last message get a
                # role; everything between is an ordinary intermediate node.
                if is_first_message_of_conversation:
                    role = "root"
                    is_first_message_of_conversation = False
                elif msg_idx == len(flat_msgs) - 1:
                    role = "impact"
                else:
                    role = "none"
                node_id, elapsed = cli.put_node(content, vec, sig, role=role)
                node_ingest_latencies.append(elapsed)
                dia_to_node[dia_id] = node_id
                total_messages += 1

                if prev_node is not None:
                    _, elapsed = cli.put_edge(prev_node, node_id, "causal")
                    edge_ingest_latencies.append(elapsed)
                prev_node = node_id

        if (conv_idx + 1) % 1 == 0:
            elapsed_so_far = time.time() - ingest_start
            print(f"  [conv {conv_idx+1}/{len(data)}] "
                  f"messages so far: {total_messages}, "
                  f"elapsed: {elapsed_so_far:.1f}s")

    ingest_elapsed = time.time() - ingest_start
    print()
    print(f"Ingestion complete: {total_messages} messages, "
          f"{len(edge_ingest_latencies)} edges, {ingest_elapsed:.1f}s")
    print(f"  Real mean put-node latency: {statistics.mean(node_ingest_latencies)*1000:.1f}ms")
    print(f"  Real mean put-edge latency: {statistics.mean(edge_ingest_latencies)*1000:.1f}ms" if edge_ingest_latencies else "  no edges")

    # Validate the database for real
    rc, out, err, _ = cli.validate()
    print(f"\nvalidate: rc={rc} output={out!r} {err!r}")

    rc, out, err, _ = cli.inspect()
    print("inspect (relevant lines):")
    for line in out.splitlines():
        if any(k in line for k in ("nodes_visible", "edges_visible", "wal_bytes", "dimension")):
            print(f"  {line}")

    # --- Real Q&A retrieval test using BOTH real commands ---
    # `search` (causal_search): single-path semantic+signature+causal retrieval.
    # `reason` (HypoKosh/Dialectic/Lyapunov governed pipeline): additionally
    # requires multiple independent corroborating evidence paths before it
    # will confidently resolve -- on a linear message chain (one path per
    # node) it is expected to abstain by design (no_silent_promotion). Both
    # are reported so neither mechanism is silently omitted or mischaracterized.
    print()
    print("=" * 78)
    print("Q&A RETRIEVAL TEST (real `search` and `reason` calls against ingested graph)")
    print("=" * 78)

    qa_records = []
    for conv_idx, conv in enumerate(data):
        qa_list = conv.get("qa", [])
        if args.max_qa_per_conv:
            qa_list = qa_list[: args.max_qa_per_conv]
        for qa in qa_list:
            question = qa.get("question", "")
            evidence = qa.get("evidence", []) or []
            evidence_nodes = {dia_to_node[e] for e in evidence if e in dia_to_node}
            if not question or not evidence_nodes:
                continue  # can't score without real ground truth we actually ingested

            qvec = embed(question)
            qsig = abs(hash(question)) % (2**31)

            rc_s, out_s, err_s, elapsed_s = cli.search(qvec, qsig)
            sparsed = parse_search_output(out_s)
            search_hit = (not sparsed["abstain"]) and sparsed["target_node"] in evidence_nodes

            rc_r, out_r, err_r, elapsed_r = cli.reason(qvec, qsig)
            rparsed = parse_reason_output(out_r)
            reason_hit = (rparsed["status"] not in (None, "evidence_required", "abstain")) and \
                         (rparsed["primary_node"] in evidence_nodes if rparsed["primary_node"] is not None else False)

            qa_records.append({
                "conversation": conv_idx,
                "question": question,
                "evidence_dia_ids": evidence,
                "evidence_nodes": sorted(evidence_nodes),
                "search_abstain": sparsed["abstain"],
                "search_abstain_reason": sparsed["reason"],
                "search_target_node": sparsed["target_node"],
                "search_confidence": sparsed["confidence"],
                "search_hit": search_hit,
                "search_latency_s": elapsed_s,
                "reason_status": rparsed["status"],
                "reason_primary_node": rparsed["primary_node"],
                "reason_confidence": rparsed["confidence"],
                "reason_lyapunov_regime": rparsed["lyapunov_regime"],
                "reason_hit": reason_hit,
                "reason_latency_s": elapsed_r,
            })

    print(f"\nScored {len(qa_records)} Q&A pairs "
          f"(pairs with unmatched/missing evidence were skipped, not counted as hits or misses)")

    if qa_records:
        # search metrics
        search_hits = sum(1 for r in qa_records if r["search_hit"])
        search_abstains = sum(1 for r in qa_records if r["search_abstain"])
        search_recall_at_1 = search_hits / len(qa_records)
        search_latencies = [r["search_latency_s"] * 1000 for r in qa_records]
        search_confidences = [r["search_confidence"] for r in qa_records if r["search_confidence"] is not None]
        search_abstain_reasons = defaultdict(int)
        for r in qa_records:
            if r["search_abstain"]:
                search_abstain_reasons[r["search_abstain_reason"]] += 1

        print(f"\n--- search (causal_search) ---")
        print(f"Real Recall@1: {search_recall_at_1:.1%}  ({search_hits}/{len(qa_records)})")
        print(f"Abstain rate:  {search_abstains/len(qa_records):.1%}  ({search_abstains}/{len(qa_records)})")
        if search_abstain_reasons:
            print("Abstain reasons:")
            for reason, count in sorted(search_abstain_reasons.items(), key=lambda x: -x[1]):
                print(f"  {reason:<20} {count:>5}")
        if search_confidences:
            print(f"Mean confidence (non-abstain): {statistics.mean(search_confidences):.3f}")
        print(f"Latency (real, ms): mean={statistics.mean(search_latencies):.1f} "
              f"p50={sorted(search_latencies)[len(search_latencies)//2]:.1f} "
              f"p95={sorted(search_latencies)[int(len(search_latencies)*0.95)]:.1f}")

        # reason metrics
        reason_hits = sum(1 for r in qa_records if r["reason_hit"])
        reason_recall_at_1 = reason_hits / len(qa_records)
        status_counts = defaultdict(int)
        for r in qa_records:
            status_counts[r["reason_status"]] += 1
        reason_latencies = [r["reason_latency_s"] * 1000 for r in qa_records]

        print(f"\n--- reason (governed HypoKosh/Dialectic/Lyapunov pipeline) ---")
        print(f"Real Recall@1: {reason_recall_at_1:.1%}  ({reason_hits}/{len(qa_records)})")
        print(f"Status distribution:")
        for status, count in sorted(status_counts.items(), key=lambda x: -x[1]):
            print(f"  {str(status):<25} {count:>5}  ({count/len(qa_records):.1%})")
        print(f"Latency (real, ms): mean={statistics.mean(reason_latencies):.1f} "
              f"p50={sorted(reason_latencies)[len(reason_latencies)//2]:.1f} "
              f"p95={sorted(reason_latencies)[int(len(reason_latencies)*0.95)]:.1f}")
        print(f"\nNote: `reason` requires multiple independent corroborating evidence\n"
              f"paths before resolving (no_silent_promotion design goal). A linear\n"
              f"message chain (this ingestion's edge structure) offers only one path\n"
              f"per node, so a high abstain/evidence_required rate here is an expected\n"
              f"consequence of the graph topology, not a retrieval failure per se.")

    else:
        print("\nNo Q&A pairs had evidence dia_ids that matched ingested messages -- "
              "no retrieval metric can be honestly reported.")

    # Persist raw results for audit
    out_path = results_dir / "real_locomo_qa_records.json"
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(qa_records, f, indent=2)
    print(f"\nRaw per-query records written to: {out_path}")

    summary = {
        "total_messages_ingested": total_messages,
        "total_edges_ingested": len(edge_ingest_latencies),
        "ingest_elapsed_s": ingest_elapsed,
        "mean_put_node_latency_ms": statistics.mean(node_ingest_latencies) * 1000 if node_ingest_latencies else None,
        "mean_put_edge_latency_ms": statistics.mean(edge_ingest_latencies) * 1000 if edge_ingest_latencies else None,
        "qa_pairs_scored": len(qa_records),
        "search_recall_at_1": (sum(1 for r in qa_records if r["search_hit"]) / len(qa_records)) if qa_records else None,
        "search_abstain_rate": (sum(1 for r in qa_records if r["search_abstain"]) / len(qa_records)) if qa_records else None,
        "reason_recall_at_1": (sum(1 for r in qa_records if r["reason_hit"]) / len(qa_records)) if qa_records else None,
        "cli_total_calls": cli.call_count,
        "cli_total_latency_s": cli.total_latency,
        "embedding_method": "hashed bag-of-words (feature hashing), NOT a semantic model",
        "caveat": "This is a real execution against the real CLI/database, no simulated "
                  "numbers. The embedding is a lightweight lexical-overlap hash, not a "
                  "trained semantic model, so recall reflects word-overlap retrieval "
                  "quality, not full semantic understanding. `reason`'s governed pipeline "
                  "requires multiple independent evidence paths to resolve; this ingestion "
                  "used a linear per-conversation causal chain (one path per node), so its "
                  "abstain rate reflects that graph topology as much as retrieval quality.",
    }
    summary_path = results_dir / "real_locomo_summary.json"
    with open(summary_path, "w", encoding="utf-8") as f:
        json.dump(summary, f, indent=2)
    print(f"Summary written to: {summary_path}")


if __name__ == "__main__":
    main()
