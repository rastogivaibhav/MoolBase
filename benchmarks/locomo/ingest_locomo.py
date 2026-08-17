#!/usr/bin/env python3
"""
Ingest LoCoMo dataset into GrapheneDB for benchmark testing.

Converts conversations into a causal graph:
- Messages → Nodes (with embeddings)
- Temporal ordering → Edges (FollowsInTime)
- Q&A pairs → Nodes + Links (Question, Answer)
"""

import json
import sys
import subprocess
import hashlib
import time
from pathlib import Path
from typing import List, Dict, Tuple
import numpy as np

class LoCoMoIngestor:
    def __init__(self, locomo_json: str, db_path: str, cli_path: str, dim: int = 768):
        self.locomo_json = locomo_json
        self.db_path = db_path
        self.cli_path = cli_path
        self.dim = dim
        self.node_id = 0
        self.edge_id = 0

    def load_data(self) -> List[Dict]:
        """Load LoCoMo dataset."""
        with open(self.locomo_json, encoding='utf-8') as f:
            return json.load(f)

    def hash_to_vector(self, text: str) -> str:
        """Convert text to a vector by hashing."""
        # Simple deterministic embedding: hash text and generate vector
        h = hashlib.sha256(text.encode()).digest()
        # Use hash bytes to seed random vector generation
        np.random.seed(int.from_bytes(h[:4], 'big'))
        vec = np.random.randn(self.dim).astype(np.float32)
        # Normalize
        vec = vec / (np.linalg.norm(vec) + 1e-8)
        # Scale to [0.1, 0.9] range for clarity
        vec = 0.1 + (vec - vec.min()) / (vec.max() - vec.min() + 1e-8) * 0.8
        return ','.join(str(round(float(v), 4)) for v in vec)

    def run_cli(self, args: List[str]) -> Tuple[int, str]:
        """Run CLI command."""
        # Ensure paths are absolute and converted properly
        cli_exe = str(Path(self.cli_path).absolute())
        db_file = str(Path(self.db_path).absolute())

        # Replace db path in args
        args = [db_file if arg == self.db_path else arg for arg in args]

        cmd = [cli_exe] + args
        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=30,
                shell=False
            )
            return result.returncode, result.stdout + result.stderr
        except subprocess.TimeoutExpired:
            return -1, "TIMEOUT"
        except Exception as e:
            return -1, str(e)

    def init_db(self):
        """Initialize database."""
        print(f"[INIT] Creating database at {self.db_path}...")
        rc, out = self.run_cli(['init', self.db_path, str(self.dim)])
        if rc != 0:
            print(f"ERROR: {out}")
            sys.exit(1)
        print(f"[OK] Database initialized")

    def ingest_conversation(self, conv_id: int, conv: Dict):
        """Ingest one conversation."""
        print(f"\n[CONV {conv_id}] Ingesting conversation '{conv['sample_id']}'...")

        sample_id = conv['sample_id']
        conversation = conv['conversation']
        speaker_a = conversation.get('speaker_a', 'Speaker A')
        speaker_b = conversation.get('speaker_b', 'Speaker B')

        # Get sessions
        sessions = sorted([
            (k, v) for k, v in conversation.items()
            if k.startswith('session_') and isinstance(v, list)
        ], key=lambda x: x[0])

        print(f"[CONV {conv_id}] Sessions: {len(sessions)}, Speakers: {speaker_a}, {speaker_b}")

        prev_message_id = None
        total_messages = 0

        for session_num, (session_key, messages) in enumerate(sessions):
            if not messages:
                continue

            # Create session node
            session_node_id = self.node_id
            self.node_id += 1
            session_content = f"Session {session_num} of {sample_id}"
            session_sig = hash(session_content) & 0xFFFFFFFF
            session_vec = self.hash_to_vector(session_content)

            rc, out = self.run_cli([
                'put-node', self.db_path, str(self.dim),
                session_content, session_vec, str(session_sig), 'root'
            ])

            if rc != 0:
                print(f"ERROR ingesting session node: {out}")
                continue

            # Ingest messages
            for msg_num, msg in enumerate(messages):
                speaker = msg.get('speaker', '?')
                text = msg.get('text', '')[:200]  # Truncate for clarity
                dia_id = msg.get('dia_id', f"{session_num}_{msg_num}")

                # Create message node
                msg_node_id = self.node_id
                self.node_id += 1

                # Use text content as signature base
                msg_sig = hash(f"{sample_id}_{dia_id}_{text}") & 0xFFFFFFFF
                msg_vec = self.hash_to_vector(text)

                # Determine role
                role = 'root' if msg_num == 0 else 'symptom' if msg_num < len(messages) // 2 else 'impact'

                rc, out = self.run_cli([
                    'put-node', self.db_path, str(self.dim),
                    f"{speaker}: {text}", msg_vec, str(msg_sig), role
                ])

                if rc != 0:
                    print(f"ERROR at msg {msg_num}: {out}")
                    continue

                # Link to previous message (temporal ordering)
                if prev_message_id is not None:
                    rc, out = self.run_cli([
                        'put-edge', self.db_path, str(self.dim),
                        str(prev_message_id), str(msg_node_id), 'causal'
                    ])

                # Link to session
                rc, out = self.run_cli([
                    'put-edge', self.db_path, str(self.dim),
                    str(session_node_id), str(msg_node_id), 'supports'
                ])

                prev_message_id = msg_node_id
                total_messages += 1

                if total_messages % 100 == 0:
                    print(f"  ... {total_messages} messages ingested")

        print(f"[CONV {conv_id}] Ingested {total_messages} messages")

        # Ingest Q&A pairs
        if 'qa' in conv:
            qa_count = 0
            for qa in conv['qa']:
                q_text = qa.get('question', '')[:100]
                a_text = qa.get('answer', '')[:100]

                if not q_text or not a_text:
                    continue

                # Question node
                q_id = self.node_id
                self.node_id += 1
                q_sig = hash(q_text) & 0xFFFFFFFF
                q_vec = self.hash_to_vector(q_text)

                rc, out = self.run_cli([
                    'put-node', self.db_path, str(self.dim),
                    f"Q: {q_text}", q_vec, str(q_sig), 'root'
                ])

                # Answer node
                a_id = self.node_id
                self.node_id += 1
                a_sig = hash(a_text) & 0xFFFFFFFF
                a_vec = self.hash_to_vector(a_text)

                rc, out = self.run_cli([
                    'put-node', self.db_path, str(self.dim),
                    f"A: {a_text}", a_vec, str(a_sig), 'impact'
                ])

                # Link Q→A
                rc, out = self.run_cli([
                    'put-edge', self.db_path, str(self.dim),
                    str(q_id), str(a_id), 'causal'
                ])

                qa_count += 1

            print(f"[CONV {conv_id}] Ingested {qa_count} Q&A pairs")

    def run(self):
        """Run full ingestion."""
        print("="*60)
        print("LoCoMo to GrapheneDB Ingestion")
        print("="*60)

        self.init_db()

        data = self.load_data()
        print(f"Loaded {len(data)} conversations, {sum(sum(1 for k,v in c['conversation'].items() if k.startswith('session_') and isinstance(v, list)) for c in data)} total messages\n")

        start_time = time.time()

        for conv_id, conv in enumerate(data):
            self.ingest_conversation(conv_id, conv)

        elapsed = time.time() - start_time

        print("\n" + "="*60)
        print(f"Ingestion complete in {elapsed:.1f}s")
        print(f"Total nodes: {self.node_id}")
        print(f"Total edges: {self.edge_id}")
        print("="*60)

        # Validate
        print("\n[VALIDATE] Checking database integrity...")
        rc, out = self.run_cli(['validate', self.db_path, str(self.dim)])
        if rc == 0:
            print("[OK] Database validation passed")
        else:
            print(f"[WARN] Validation issues: {out}")

        # Inspect
        print("\n[INSPECT] Database statistics:")
        rc, out = self.run_cli(['inspect', self.db_path, str(self.dim)])
        for line in out.split('\n'):
            if any(x in line for x in ['nodes_visible', 'edges_visible', 'dimension', 'wal_bytes']):
                print(f"  {line}")


if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser(description='Ingest LoCoMo into GrapheneDB')
    parser.add_argument('--locomo', default='./benchmarks/locomo-dataset/data/locomo10.json',
                       help='Path to locomo10.json')
    parser.add_argument('--db', default='./benchmarks/locomo/locomo.db',
                       help='Output database path')
    parser.add_argument('--cli', default='./build/graphenedb_cli.exe',
                       help='Path to GrapheneDB CLI')
    parser.add_argument('--dim', type=int, default=768,
                       help='Vector dimension')

    args = parser.parse_args()

    ingestor = LoCoMoIngestor(args.locomo, args.db, args.cli, args.dim)
    ingestor.run()
