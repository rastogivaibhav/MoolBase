#!/usr/bin/env python3
"""Focused correctness tests for the experimental storage benchmark."""

from __future__ import annotations

import importlib.util
import math
import sys
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "storage_reduction_benchmark.py"
SPEC = importlib.util.spec_from_file_location("storage_reduction_benchmark", SCRIPT)
assert SPEC and SPEC.loader
benchmark = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = benchmark
SPEC.loader.exec_module(benchmark)


class StorageReductionBenchmarkTests(unittest.TestCase):
    def vectors(self) -> list[list[float]]:
        return [
            [benchmark.f32(0.0), benchmark.f32(0.5), benchmark.f32(-0.5), benchmark.f32(1.0)],
            [benchmark.f32(0.00001), benchmark.f32(0.50001), benchmark.f32(-0.49999), benchmark.f32(1000.0)],
            [benchmark.f32(-1000.0), benchmark.f32(0.25), benchmark.f32(-0.25), benchmark.f32(2.0)],
        ]

    def test_float32_is_exact(self) -> None:
        encoded = benchmark.encode_float32(self.vectors())
        decoded = benchmark.decode_vector_payload(encoded.name, encoded.payload)
        self.assertEqual(decoded, self.vectors())

    def test_all_vector_codecs_decode_deterministically(self) -> None:
        factories = [
            benchmark.encode_float16,
            lambda vectors: benchmark.uniform_encoding(vectors, 16),
            lambda vectors: benchmark.uniform_encoding(vectors, 8),
            lambda vectors: benchmark.uniform_encoding(vectors, 4),
            lambda vectors: benchmark.block_int8_encoding(vectors, 2),
            lambda vectors: benchmark.predictive_int8_encoding(vectors, 2),
        ]
        for factory in factories:
            with self.subTest(factory=factory):
                encoded = factory(self.vectors())
                decoded = benchmark.decode_vector_payload(encoded.name, encoded.payload)
                self.assertEqual(decoded, encoded.reconstructed)
                self.assertTrue(all(math.isfinite(value) for row in decoded for value in row))

    def test_predictive_codec_uses_exact_outlier_exceptions(self) -> None:
        vectors = [
            [benchmark.f32(index * 0.001 + component * 0.0001) for component in range(4)]
            for index in range(128)
        ]
        vectors[63][3] = benchmark.f32(1000.0)
        vectors[64][3] = benchmark.f32(-1000.0)
        encoded = benchmark.predictive_int8_encoding(vectors, 128)
        self.assertGreater(encoded.exception_rate, 0.0)
        decoded = benchmark.decode_vector_payload(encoded.name, encoded.payload)
        self.assertEqual(decoded[63][3], vectors[63][3])
        self.assertEqual(decoded[64][3], vectors[64][3])

    def test_corrupt_v2_frame_is_rejected(self) -> None:
        payload = b"DATA_NODE\t0\t1\t18446744073709551615\t0\t0\t0\t0\t0\t61\t0,0,0,0\tnone\t0\t"
        frame = (
            str(len(payload)).encode("ascii")
            + b"|"
            + str(benchmark.fnv1a(payload) ^ 1).encode("ascii")
            + b"|"
            + payload
            + b"\n"
        )
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "graphene.data").write_bytes(frame)
            (root / "graphene.wal").write_bytes(b"")
            (root / "MANIFEST").write_text("dimension=4\n", encoding="ascii")
            with self.assertRaisesRegex(ValueError, "frame verification failed"):
                benchmark.parse_checkpoint(root)

    def test_section_checksum_detects_mutation(self) -> None:
        encoded = benchmark.section(b"payload")
        length, checksum = benchmark.struct.unpack_from("<QI", encoded, 0)
        body = bytearray(encoded[12:])
        body[0] ^= 0x01
        self.assertEqual(length, len(body))
        self.assertNotEqual(checksum, benchmark.zlib.crc32(body))


if __name__ == "__main__":
    unittest.main()
