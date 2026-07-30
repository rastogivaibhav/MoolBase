#!/usr/bin/env python3
"""Measure GrapheneDB storage anatomy and candidate v3 encodings.

This is an experimental measurement harness, not a production codec.  It reads
the real v2 framed checkpoint, verifies every frame, constructs actual candidate
byte streams, decodes their vector payloads, and evaluates query-level
distortion.  The production implementation must remain in the C++ core and
requires an explicit durable-format migration decision.
"""

from __future__ import annotations

import argparse
import bz2
import dataclasses
import gzip
import heapq
import json
import lzma
import math
import os
import random
import statistics
import struct
import time
import tracemalloc
import zlib
from pathlib import Path
from typing import Callable, Iterable, Sequence


FNV_OFFSET = 1469598103934665603
FNV_PRIME = 1099511628211
INF_VERSION = (1 << 64) - 1
CODEC_VERSION = 1
BLOCK_NODES = 256


@dataclasses.dataclass
class NodeRecord:
    node_id: int
    created: int
    deleted: int
    signature: int
    incident: int
    flags: int
    content: bytes
    vector: list[float]
    lattice: tuple[int, int, int] | None
    defect: int
    metadata: list[tuple[bytes, bytes]]


@dataclasses.dataclass
class EdgeRecord:
    edge_id: int
    source: int
    target: int
    origin: int
    role: int
    confidence_fixed6: int
    created: int
    deleted: int
    bond_type: int
    defect_type: int
    layer_coupling: int
    strength_fixed6: int
    metadata: list[tuple[bytes, bytes]]


@dataclasses.dataclass
class ParsedStore:
    nodes: list[NodeRecord]
    edges: list[EdgeRecord]
    dimension: int
    baseline_blob: bytes
    anatomy: dict[str, int]


@dataclasses.dataclass
class VectorEncoding:
    name: str
    payload: bytes
    reconstructed: list[list[float]]
    notes: str
    update_amplification_values: int
    exception_rate: float


def fnv1a(data: bytes) -> int:
    value = FNV_OFFSET
    for byte in data:
        value ^= byte
        value = (value * FNV_PRIME) & INF_VERSION
    return value


def f32(value: float) -> float:
    return struct.unpack("<f", struct.pack("<f", value))[0]


def encode_varint(value: int) -> bytes:
    if value < 0:
        raise ValueError("unsigned varint cannot encode a negative value")
    out = bytearray()
    while value >= 0x80:
        out.append((value & 0x7F) | 0x80)
        value >>= 7
    out.append(value)
    return bytes(out)


def zigzag(value: int) -> int:
    return (value << 1) ^ (value >> 63)


def encode_svarint(value: int) -> bytes:
    return encode_varint(zigzag(value))


def decode_metadata(field: bytes) -> list[tuple[bytes, bytes]]:
    if not field:
        return []
    result: list[tuple[bytes, bytes]] = []
    for item in field.split(b";"):
        key, sep, value = item.partition(b"=")
        if not sep:
            raise ValueError("malformed metadata")
        result.append((bytes.fromhex(key.decode("ascii")), bytes.fromhex(value.decode("ascii"))))
    return result


def parse_lattice(field: bytes) -> tuple[int, int, int] | None:
    if field in (b"", b"none"):
        return None
    parts = field.split(b",")
    if len(parts) != 3:
        raise ValueError("malformed lattice coordinate")
    return tuple(int(part) for part in parts)  # type: ignore[return-value]


def parse_checkpoint(db_dir: Path) -> ParsedStore:
    data_path = db_dir / "graphene.data"
    manifest_path = db_dir / "MANIFEST"
    wal_path = db_dir / "graphene.wal"
    data = data_path.read_bytes()
    manifest = manifest_path.read_bytes() if manifest_path.exists() else b""
    wal = wal_path.read_bytes() if wal_path.exists() else b""
    if wal:
        raise ValueError("benchmark requires a compacted store with an empty WAL")

    anatomy = {
        "framing": 0,
        "node_content_hex": 0,
        "node_vectors_text": 0,
        "node_coordinates_text": 0,
        "node_metadata_hex": 0,
        "node_fixed_and_delimiters": 0,
        "edge_topology_text": 0,
        "edge_weights_text": 0,
        "edge_metadata_hex": 0,
        "edge_fixed_and_delimiters": 0,
        "manifest": len(manifest),
        "wal": len(wal),
        "decoded_content": 0,
        "decoded_metadata": 0,
        "semantic_vector_float32": 0,
    }
    nodes: list[NodeRecord] = []
    edges: list[EdgeRecord] = []

    offset = 0
    for raw_line in data.splitlines(keepends=True):
        line = raw_line[:-2] if raw_line.endswith(b"\r\n") else raw_line[:-1]
        first = line.find(b"|")
        second = line.find(b"|", first + 1)
        if first < 0 or second < 0:
            raise ValueError(f"malformed frame at offset {offset}")
        expected_len = int(line[:first])
        expected_checksum = int(line[first + 1 : second])
        payload = line[second + 1 :]
        if len(payload) != expected_len or fnv1a(payload) != expected_checksum:
            raise ValueError(f"frame verification failed at offset {offset}")
        anatomy["framing"] += len(raw_line) - len(payload)
        fields = payload.split(b"\t")
        op = fields[0]

        if op == b"DATA_NODE":
            if len(fields) != 14:
                raise ValueError(f"unexpected node field count: {len(fields)}")
            vector = [f32(float(item)) for item in fields[10].split(b",")] if fields[10] else []
            metadata = decode_metadata(fields[13])
            flags = (int(fields[6]) & 1) | ((int(fields[7]) & 1) << 1) | ((int(fields[8]) & 1) << 2)
            nodes.append(
                NodeRecord(
                    node_id=int(fields[1]),
                    created=int(fields[2]),
                    deleted=int(fields[3]),
                    signature=int(fields[4]),
                    incident=int(fields[5]),
                    flags=flags,
                    content=bytes.fromhex(fields[9].decode("ascii")),
                    vector=vector,
                    lattice=parse_lattice(fields[11]),
                    defect=int(fields[12]),
                    metadata=metadata,
                )
            )
            anatomy["node_content_hex"] += len(fields[9])
            anatomy["node_vectors_text"] += len(fields[10])
            anatomy["node_coordinates_text"] += len(fields[11])
            anatomy["node_metadata_hex"] += len(fields[13])
            anatomy["node_fixed_and_delimiters"] += (
                len(payload) - len(fields[9]) - len(fields[10]) - len(fields[11]) - len(fields[13])
            )
            anatomy["decoded_content"] += len(nodes[-1].content)
            anatomy["decoded_metadata"] += sum(len(k) + len(v) for k, v in metadata)
            anatomy["semantic_vector_float32"] += len(vector) * 4
        elif op == b"DATA_EDGE":
            if len(fields) != 14:
                raise ValueError(f"unexpected edge field count: {len(fields)}")
            metadata = decode_metadata(fields[13])
            edges.append(
                EdgeRecord(
                    edge_id=int(fields[1]),
                    source=int(fields[2]),
                    target=int(fields[3]),
                    origin=int(fields[4]),
                    role=int(fields[5]),
                    confidence_fixed6=round(float(fields[6]) * 1_000_000),
                    created=int(fields[7]),
                    deleted=int(fields[8]),
                    bond_type=int(fields[9]),
                    defect_type=int(fields[10]),
                    layer_coupling=int(fields[11]),
                    strength_fixed6=round(float(fields[12]) * 1_000_000),
                    metadata=metadata,
                )
            )
            anatomy["edge_topology_text"] += len(fields[2]) + len(fields[3])
            anatomy["edge_weights_text"] += len(fields[6]) + len(fields[12])
            anatomy["edge_metadata_hex"] += len(fields[13])
            anatomy["edge_fixed_and_delimiters"] += (
                len(payload)
                - len(fields[2])
                - len(fields[3])
                - len(fields[6])
                - len(fields[12])
                - len(fields[13])
            )
            anatomy["decoded_metadata"] += sum(len(k) + len(v) for k, v in metadata)
        else:
            raise ValueError(f"unexpected checkpoint record {op!r}")
        offset += len(raw_line)

    dimension = len(nodes[0].vector) if nodes else 0
    if any(len(node.vector) != dimension for node in nodes):
        raise ValueError("mixed vector dimensions")
    anatomy["graphene_data"] = len(data)
    anatomy["total_persisted"] = len(data) + len(manifest) + len(wal)
    anatomy["classified_persisted"] = sum(
        value
        for key, value in anatomy.items()
        if key
        in {
            "framing",
            "node_content_hex",
            "node_vectors_text",
            "node_coordinates_text",
            "node_metadata_hex",
            "node_fixed_and_delimiters",
            "edge_topology_text",
            "edge_weights_text",
            "edge_metadata_hex",
            "edge_fixed_and_delimiters",
            "manifest",
            "wal",
        }
    )
    return ParsedStore(nodes, edges, dimension, data + manifest + wal, anatomy)


def build_dictionary(store: ParsedStore) -> tuple[list[bytes], dict[bytes, int]]:
    values = {item for node in store.nodes for pair in node.metadata for item in pair}
    values.update(item for edge in store.edges for pair in edge.metadata for item in pair)
    ordered = sorted(values)
    return ordered, {value: index for index, value in enumerate(ordered)}


def section(payload: bytes) -> bytes:
    return struct.pack("<QI", len(payload), zlib.crc32(payload)) + payload


def structural_sections(store: ParsedStore) -> tuple[bytes, bytes, bytes, bytes, bytes]:
    dictionary, dictionary_ids = build_dictionary(store)
    dictionary_payload = bytearray()
    dictionary_payload += encode_varint(len(dictionary))
    for value in dictionary:
        dictionary_payload += encode_varint(len(value)) + value

    node_payload = bytearray()
    node_index = bytearray()
    previous_id = 0
    previous_created = 0
    for index, node in enumerate(store.nodes):
        if index % BLOCK_NODES == 0:
            node_index += struct.pack("<IQ", node.node_id, len(node_payload))
        node_payload += encode_varint(node.node_id - previous_id)
        node_payload += encode_svarint(node.created - previous_created)
        node_payload += encode_varint(0 if node.deleted == INF_VERSION else node.deleted + 1)
        node_payload += encode_varint(node.signature)
        node_payload += encode_varint(node.incident)
        node_payload += bytes((node.flags, node.defect))
        if node.lattice is None:
            node_payload += b"\x00"
        else:
            node_payload += b"\x01"
            for coordinate in node.lattice:
                node_payload += encode_svarint(coordinate)
        node_payload += encode_varint(len(node.content)) + node.content
        node_payload += encode_varint(len(node.metadata))
        for key, value in node.metadata:
            node_payload += encode_varint(dictionary_ids[key])
            node_payload += encode_varint(dictionary_ids[value])
        previous_id = node.node_id
        previous_created = node.created

    edge_payload = bytearray()
    edge_sparse_index = bytearray()
    previous_id = 0
    previous_created = 0
    for index, edge in enumerate(store.edges):
        if index % BLOCK_NODES == 0:
            edge_sparse_index += struct.pack("<IQ", edge.edge_id, len(edge_payload))
        edge_payload += encode_varint(edge.edge_id - previous_id)
        edge_payload += encode_varint(edge.source)
        edge_payload += encode_varint(edge.target)
        edge_payload += bytes((edge.origin, edge.role, edge.bond_type, edge.defect_type, edge.layer_coupling))
        edge_payload += encode_varint(edge.confidence_fixed6)
        edge_payload += encode_varint(edge.strength_fixed6)
        edge_payload += encode_svarint(edge.created - previous_created)
        edge_payload += encode_varint(0 if edge.deleted == INF_VERSION else edge.deleted + 1)
        edge_payload += encode_varint(len(edge.metadata))
        for key, value in edge.metadata:
            edge_payload += encode_varint(dictionary_ids[key])
            edge_payload += encode_varint(dictionary_ids[value])
        previous_id = edge.edge_id
        previous_created = edge.created

    # Production-oriented persistent indexes. The current v2 store rebuilds
    # these in memory, but omitting them would make the candidate comparison
    # unrealistically favourable.
    outgoing: dict[int, list[int]] = {}
    incoming: dict[int, list[int]] = {}
    for edge in store.edges:
        outgoing.setdefault(edge.source, []).append(edge.edge_id)
        incoming.setdefault(edge.target, []).append(edge.edge_id)
    adjacency_index = bytearray()
    indexed_nodes = sorted(set(outgoing) | set(incoming))
    adjacency_index += encode_varint(len(indexed_nodes))
    previous_node = 0
    for node_id in indexed_nodes:
        adjacency_index += encode_varint(node_id - previous_node)
        for edge_ids in (sorted(outgoing.get(node_id, [])), sorted(incoming.get(node_id, []))):
            adjacency_index += encode_varint(len(edge_ids))
            previous_edge = 0
            for edge_id in edge_ids:
                adjacency_index += encode_varint(edge_id - previous_edge)
                previous_edge = edge_id
        previous_node = node_id

    postings: dict[tuple[int, int, int], list[int]] = {}
    for node in store.nodes:
        for key, value in node.metadata:
            postings.setdefault((0, dictionary_ids[key], dictionary_ids[value]), []).append(node.node_id)
    for edge in store.edges:
        for key, value in edge.metadata:
            postings.setdefault((1, dictionary_ids[key], dictionary_ids[value]), []).append(edge.edge_id)
    metadata_index = bytearray()
    metadata_index += encode_varint(len(postings))
    for (scope, key_id, value_id), identifiers in sorted(postings.items()):
        metadata_index += bytes((scope,))
        metadata_index += encode_varint(key_id)
        metadata_index += encode_varint(value_id)
        metadata_index += encode_varint(len(identifiers))
        previous = 0
        for identifier in sorted(identifiers):
            metadata_index += encode_varint(identifier - previous)
            previous = identifier

    coordinates = sorted(
        (node.lattice[2], node.lattice[0], node.lattice[1], node.node_id)
        for node in store.nodes
        if node.lattice is not None
    )
    coordinate_index = bytearray()
    coordinate_index += encode_varint(len(coordinates))
    previous_layer = previous_q = previous_r = previous_node = 0
    for layer, q, r, node_id in coordinates:
        coordinate_index += encode_svarint(layer - previous_layer)
        coordinate_index += encode_svarint(q - previous_q)
        coordinate_index += encode_svarint(r - previous_r)
        coordinate_index += encode_svarint(node_id - previous_node)
        previous_layer, previous_q, previous_r, previous_node = layer, q, r, node_id

    index_bundle = (
        section(bytes(node_index))
        + section(bytes(edge_sparse_index))
        + section(bytes(adjacency_index))
        + section(bytes(metadata_index))
        + section(bytes(coordinate_index))
    )
    return bytes(dictionary_payload), bytes(node_payload), bytes(edge_payload), index_bundle, bytes(node_index)


def flatten(vectors: Sequence[Sequence[float]]) -> list[float]:
    return [value for vector in vectors for value in vector]


def reshape(values: Sequence[float], dimension: int) -> list[list[float]]:
    return [list(values[index : index + dimension]) for index in range(0, len(values), dimension)]


def encode_float32(vectors: list[list[float]]) -> VectorEncoding:
    values = flatten(vectors)
    payload = struct.pack("<II", len(vectors), len(vectors[0]) if vectors else 0)
    payload += struct.pack(f"<{len(values)}f", *values)
    reconstructed = reshape(list(struct.unpack(f"<{len(values)}f", payload[8:])), len(vectors[0]))
    return VectorEncoding("binary_f32", payload, reconstructed, "Exact for GrapheneDB float32 vectors.", 1, 0.0)


def encode_float16(vectors: list[list[float]]) -> VectorEncoding:
    values = flatten(vectors)
    body = b"".join(struct.pack("<e", value) for value in values)
    reconstructed = reshape(
        [struct.unpack("<e", body[index : index + 2])[0] for index in range(0, len(body), 2)],
        len(vectors[0]),
    )
    payload = struct.pack("<II", len(vectors), len(vectors[0])) + body
    return VectorEncoding(
        "binary_f16",
        payload,
        reconstructed,
        "IEEE binary16 vectors; topology, content, metadata, and edge weights remain exact.",
        1,
        0.0,
    )


def pack_unsigned_codes(codes: Sequence[int], bits: int) -> bytes:
    if bits == 16:
        return struct.pack(f"<{len(codes)}H", *codes)
    if bits == 8:
        return bytes(codes)
    if bits == 4:
        out = bytearray()
        for index in range(0, len(codes), 2):
            low = codes[index]
            high = codes[index + 1] if index + 1 < len(codes) else 0
            out.append(low | (high << 4))
        return bytes(out)
    raise ValueError(f"unsupported bit width {bits}")


def uniform_encoding(vectors: list[list[float]], bits: int) -> VectorEncoding:
    values = flatten(vectors)
    minimum = min(values)
    maximum = max(values)
    levels = (1 << bits) - 1
    scale = (maximum - minimum) / levels if maximum != minimum else 1.0
    codes = [max(0, min(levels, round((value - minimum) / scale))) for value in values]
    body = pack_unsigned_codes(codes, bits)
    stored_minimum, stored_scale = struct.unpack("<ff", struct.pack("<ff", minimum, scale))
    reconstructed = [f32(stored_minimum + stored_scale * code) for code in codes]
    payload = struct.pack("<IIB3xff", len(vectors), len(vectors[0]), bits, stored_minimum, stored_scale) + body
    return VectorEncoding(
        f"global_int{bits}",
        payload,
        reshape(reconstructed, len(vectors[0])),
        f"Global asymmetric {bits}-bit uniform vector quantization.",
        len(values),
        0.0,
    )


def block_int8_encoding(vectors: list[list[float]], block_nodes: int = BLOCK_NODES) -> VectorEncoding:
    dimension = len(vectors[0])
    payload = bytearray(struct.pack("<III", len(vectors), dimension, block_nodes))
    reconstructed: list[list[float]] = []
    for start in range(0, len(vectors), block_nodes):
        block = vectors[start : start + block_nodes]
        values = flatten(block)
        minimum = min(values)
        maximum = max(values)
        scale = (maximum - minimum) / 255 if maximum != minimum else 1.0
        minimum, scale = struct.unpack("<ff", struct.pack("<ff", minimum, scale))
        codes = [max(0, min(255, round((value - minimum) / scale))) for value in values]
        payload += struct.pack("<Iff", len(block), minimum, scale)
        payload += bytes(codes)
        reconstructed.extend(reshape([f32(minimum + scale * code) for code in codes], dimension))
    return VectorEncoding(
        "block_int8",
        bytes(payload),
        reconstructed,
        f"Per-{block_nodes}-node asymmetric Int8 blocks.",
        block_nodes * dimension,
        0.0,
    )


def percentile(values: Sequence[float], fraction: float) -> float:
    ordered = sorted(values)
    if not ordered:
        return 0.0
    return ordered[min(len(ordered) - 1, round(fraction * (len(ordered) - 1)))]


def predictive_int8_encoding(vectors: list[list[float]], block_nodes: int = BLOCK_NODES) -> VectorEncoding:
    dimension = len(vectors[0])
    payload = bytearray(struct.pack("<III", len(vectors), dimension, block_nodes))
    reconstructed: list[list[float]] = []
    total_exceptions = 0
    total_values = len(vectors) * dimension

    for start in range(0, len(vectors), block_nodes):
        block = vectors[start : start + block_nodes]
        approximate_residuals: list[float] = []
        previous = [0.0] * dimension
        for vector in block:
            approximate_residuals.extend(value - previous[index] for index, value in enumerate(vector))
            previous = vector
        threshold = percentile([abs(value) for value in approximate_residuals], 0.99)
        scale = max(threshold / 127.0, 1e-30)
        scale = struct.unpack("<f", struct.pack("<f", scale))[0]
        codes = bytearray()
        exceptions: list[float] = []
        block_reconstructed: list[list[float]] = []
        previous = [0.0] * dimension
        for vector in block:
            decoded_vector: list[float] = []
            for component, value in enumerate(vector):
                residual = value - previous[component]
                code = round(residual / scale)
                if code < -127 or code > 127:
                    codes += struct.pack("<b", -128)
                    exceptions.append(value)
                    decoded = value
                else:
                    codes += struct.pack("<b", code)
                    decoded = f32(previous[component] + scale * code)
                decoded_vector.append(decoded)
            block_reconstructed.append(decoded_vector)
            previous = decoded_vector
        payload += struct.pack("<IfI", len(block), scale, len(exceptions))
        payload += codes
        payload += struct.pack(f"<{len(exceptions)}f", *exceptions)
        reconstructed.extend(block_reconstructed)
        total_exceptions += len(exceptions)

    return VectorEncoding(
        "predictive_residual_int8",
        bytes(payload),
        reconstructed,
        f"Previous-decoded-cell predictor, 99th-percentile residual scale, exact float32 exceptions.",
        1,
        total_exceptions / max(1, total_values),
    )


def decode_unsigned_codes(body: bytes, count: int, bits: int) -> list[int]:
    if bits == 16:
        return list(struct.unpack(f"<{count}H", body[: count * 2]))
    if bits == 8:
        return list(body[:count])
    if bits == 4:
        result: list[int] = []
        for byte in body:
            result.append(byte & 0x0F)
            if len(result) < count:
                result.append(byte >> 4)
        return result
    raise ValueError(f"unsupported bit width {bits}")


def decode_vector_payload(name: str, payload: bytes) -> list[list[float]]:
    if name == "binary_f32":
        count, dimension = struct.unpack_from("<II", payload, 0)
        values = list(struct.unpack_from(f"<{count * dimension}f", payload, 8))
        return reshape(values, dimension)
    if name == "binary_f16":
        count, dimension = struct.unpack_from("<II", payload, 0)
        values = [
            struct.unpack_from("<e", payload, 8 + index * 2)[0]
            for index in range(count * dimension)
        ]
        return reshape(values, dimension)
    if name.startswith("global_int"):
        count, dimension, bits, minimum, scale = struct.unpack_from("<IIB3xff", payload, 0)
        header_size = struct.calcsize("<IIB3xff")
        codes = decode_unsigned_codes(payload[header_size:], count * dimension, bits)
        return reshape([f32(minimum + scale * code) for code in codes], dimension)
    if name == "block_int8":
        count, dimension, block_nodes = struct.unpack_from("<III", payload, 0)
        del block_nodes
        offset = 12
        output: list[list[float]] = []
        while len(output) < count:
            block_count, minimum, scale = struct.unpack_from("<Iff", payload, offset)
            offset += 12
            code_count = block_count * dimension
            codes = payload[offset : offset + code_count]
            offset += code_count
            output.extend(reshape([f32(minimum + scale * code) for code in codes], dimension))
        return output
    if name == "predictive_residual_int8":
        count, dimension, block_nodes = struct.unpack_from("<III", payload, 0)
        del block_nodes
        offset = 12
        output: list[list[float]] = []
        while len(output) < count:
            block_count, scale, exception_count = struct.unpack_from("<IfI", payload, offset)
            offset += 12
            code_count = block_count * dimension
            codes = struct.unpack_from(f"<{code_count}b", payload, offset)
            offset += code_count
            exceptions = list(struct.unpack_from(f"<{exception_count}f", payload, offset))
            offset += exception_count * 4
            exception_index = 0
            previous = [0.0] * dimension
            for row in range(block_count):
                decoded_vector: list[float] = []
                for component in range(dimension):
                    code = codes[row * dimension + component]
                    if code == -128:
                        decoded = exceptions[exception_index]
                        exception_index += 1
                    else:
                        decoded = f32(previous[component] + scale * code)
                    decoded_vector.append(decoded)
                output.append(decoded_vector)
                previous = decoded_vector
        return output
    raise ValueError(f"no decoder for {name}")


def assemble_candidate(
    store: ParsedStore,
    structural: tuple[bytes, bytes, bytes, bytes, bytes],
    vector_encoding: VectorEncoding,
) -> bytes:
    dictionary_payload, node_payload, edge_payload, index_bundle, _ = structural
    header = struct.pack(
        "<8sHHIIII",
        b"GDBSEG3\0",
        CODEC_VERSION,
        0x0102,
        len(store.nodes),
        len(store.edges),
        store.dimension,
        BLOCK_NODES,
    )
    return (
        header
        + section(dictionary_payload)
        + section(node_payload)
        + section(edge_payload)
        + section(vector_encoding.payload)
        + section(index_bundle)
    )


def cosine(a: Sequence[float], b: Sequence[float]) -> float:
    dot = sum(x * y for x, y in zip(a, b))
    aa = sum(x * x for x in a)
    bb = sum(y * y for y in b)
    return dot / math.sqrt(aa * bb) if aa > 0 and bb > 0 else -1.0


def top_k(vectors: Sequence[Sequence[float]], query: Sequence[float], count: int) -> list[int]:
    return [
        index
        for _, index in heapq.nlargest(
            count,
            ((cosine(vector, query), index) for index, vector in enumerate(vectors)),
        )
    ]


def vector_metrics(original: list[list[float]], reconstructed: list[list[float]]) -> dict[str, float]:
    original_values = flatten(original)
    reconstructed_values = flatten(reconstructed)
    errors = [actual - decoded for actual, decoded in zip(original_values, reconstructed_values)]
    mse = sum(error * error for error in errors) / max(1, len(errors))
    variance = statistics.pvariance(original_values) if len(original_values) > 1 else 0.0
    relative_errors = [
        abs(error) / max(abs(actual), 1e-12)
        for actual, error in zip(original_values, errors)
    ]

    region_errors: list[float] = []
    dimensions = sorted({0, len(original[0]) // 2, len(original[0]) - 1})
    for start in range(0, len(original), 64):
        for dimension in dimensions:
            expected = sum(vector[dimension] for vector in original[start : start + 64])
            actual = sum(vector[dimension] for vector in reconstructed[start : start + 64])
            region_errors.append(abs(expected - actual) / max(abs(expected), 1e-9))

    query_indexes = sorted({0, len(original) // 7, len(original) // 3, len(original) // 2, len(original) - 1})
    topk_overlaps: list[float] = []
    before = time.perf_counter()
    original_rankings = [top_k(original, original[index], 10) for index in query_indexes]
    original_query_ms = (time.perf_counter() - before) * 1000
    before = time.perf_counter()
    reconstructed_rankings = [top_k(reconstructed, original[index], 10) for index in query_indexes]
    reconstructed_query_ms = (time.perf_counter() - before) * 1000
    for expected, actual in zip(original_rankings, reconstructed_rankings):
        topk_overlaps.append(len(set(expected) & set(actual)) / 10)

    sampled = original_values[:: max(1, len(original_values) // 100_000)]
    sampled_reconstructed = reconstructed_values[:: max(1, len(reconstructed_values) // 100_000)]
    thresholds = [percentile(sampled, value) for value in (0.1, 0.5, 0.9)]
    f1_values: list[float] = []
    false_positives = 0
    false_negatives = 0
    for threshold in thresholds:
        tp = fp = fn = 0
        for expected, actual in zip(sampled, sampled_reconstructed):
            expected_positive = expected >= threshold
            actual_positive = actual >= threshold
            tp += expected_positive and actual_positive
            fp += (not expected_positive) and actual_positive
            fn += expected_positive and (not actual_positive)
        precision = tp / max(1, tp + fp)
        recall = tp / max(1, tp + fn)
        f1_values.append(2 * precision * recall / max(1e-30, precision + recall))
        false_positives += fp
        false_negatives += fn

    point_normalized = mse / max(variance, 1e-30)
    aggregate_error = statistics.mean(region_errors) if region_errors else 0.0
    rank_error = 1.0 - statistics.mean(topk_overlaps)
    threshold_error = 1.0 - statistics.mean(f1_values)
    application_distortion = (
        0.10 * min(1.0, point_normalized)
        + 0.10 * min(1.0, aggregate_error)
        + 0.35 * rank_error
        + 0.20 * threshold_error
        # Edge costs and topology remain exact in every vector candidate.
        + 0.20 * 0.0
        + 0.05 * 0.0
    )
    return {
        "mse": mse,
        "normalized_mse": point_normalized,
        "max_absolute_error": max((abs(error) for error in errors), default=0.0),
        "max_relative_error": max(relative_errors, default=0.0),
        "mean_regional_sum_relative_error": aggregate_error,
        "max_regional_sum_relative_error": max(region_errors, default=0.0),
        "mean_top10_overlap": statistics.mean(topk_overlaps),
        "mean_threshold_f1": statistics.mean(f1_values),
        "threshold_false_positives": float(false_positives),
        "threshold_false_negatives": float(false_negatives),
        "path_selection_changes": 0.0,
        "topology_changes": 0.0,
        "original_query_ms": original_query_ms,
        "reconstructed_query_ms": reconstructed_query_ms,
        "application_distortion": application_distortion,
    }


def measure_vector_candidate(
    store: ParsedStore,
    structural: tuple[bytes, bytes, bytes, bytes, bytes],
    factory: Callable[[list[list[float]]], VectorEncoding],
) -> dict[str, object]:
    vectors = [node.vector for node in store.nodes]
    tracemalloc.start()
    start = time.perf_counter()
    encoding = factory(vectors)
    candidate = assemble_candidate(store, structural, encoding)
    encode_ms = (time.perf_counter() - start) * 1000
    start = time.perf_counter()
    decoded_vectors = decode_vector_payload(encoding.name, encoding.payload)
    decode_ms = (time.perf_counter() - start) * 1000
    _, peak_memory = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    if decoded_vectors != encoding.reconstructed:
        raise AssertionError(f"{encoding.name} decoder disagrees with encoder reconstruction")
    metrics = vector_metrics(vectors, decoded_vectors)
    baseline_size = store.anatomy["total_persisted"]
    return {
        "name": encoding.name,
        "bytes": len(candidate),
        "storage_ratio": len(candidate) / baseline_size,
        "encode_ms": encode_ms,
        "decode_ms": decode_ms,
        "peak_encode_memory_bytes": peak_memory,
        "update_amplification_values": encoding.update_amplification_values,
        "exception_rate": encoding.exception_rate,
        "edge_weights_exact": True,
        "topology_exact": True,
        "content_metadata_exact": True,
        "notes": encoding.notes,
        "metrics": metrics,
        "_payload": candidate,
    }


def measure_lossless(name: str, source: bytes, compressor: Callable[[bytes], bytes], decompressor: Callable[[bytes], bytes], baseline_size: int) -> dict[str, object]:
    tracemalloc.start()
    start = time.perf_counter()
    payload = compressor(source)
    encode_ms = (time.perf_counter() - start) * 1000
    start = time.perf_counter()
    restored = decompressor(payload)
    decode_ms = (time.perf_counter() - start) * 1000
    _, peak_memory = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    if restored != source:
        raise AssertionError(f"{name} did not round-trip")
    return {
        "name": name,
        "bytes": len(payload),
        "storage_ratio": len(payload) / baseline_size,
        "encode_ms": encode_ms,
        "decode_ms": decode_ms,
        "peak_encode_memory_bytes": peak_memory,
        "update_amplification_values": len(source),
        "exception_rate": 0.0,
        "edge_weights_exact": True,
        "topology_exact": True,
        "content_metadata_exact": True,
        "notes": "Lossless whole-file compression; requires full decompression and has poor in-place update behavior.",
        "metrics": {
            "mse": 0.0,
            "normalized_mse": 0.0,
            "max_absolute_error": 0.0,
            "max_relative_error": 0.0,
            "mean_regional_sum_relative_error": 0.0,
            "max_regional_sum_relative_error": 0.0,
            "mean_top10_overlap": 1.0,
            "mean_threshold_f1": 1.0,
            "threshold_false_positives": 0.0,
            "threshold_false_negatives": 0.0,
            "path_selection_changes": 0.0,
            "topology_changes": 0.0,
            "application_distortion": 0.0,
        },
    }


def synthetic_vectors(name: str, count: int, dimension: int, seed: int) -> list[list[float]]:
    rng = random.Random(seed)
    result: list[list[float]] = []
    previous = [0.0] * dimension
    for index in range(count):
        if name == "uniform":
            vector = [rng.uniform(-1, 1) for _ in range(dimension)]
        elif name == "gaussian":
            vector = [rng.gauss(0, 1) for _ in range(dimension)]
        elif name == "sparse":
            vector = [0.0 if rng.random() < 0.9 else rng.uniform(-1, 1) for _ in range(dimension)]
        elif name == "heavy_tailed":
            vector = [(1 if rng.random() < 0.5 else -1) * rng.lognormvariate(0, 1.4) for _ in range(dimension)]
        elif name == "spatially_smooth":
            vector = [0.97 * previous[item] + rng.gauss(0, 0.03) for item in range(dimension)]
        elif name == "spatially_discontinuous":
            region = (index // 64) % 4
            vector = [region * 3.0 + rng.gauss(0, 0.1) for _ in range(dimension)]
        elif name == "outlier_heavy":
            vector = [rng.gauss(0, 0.1) if rng.random() < 0.99 else rng.choice((-100.0, 100.0)) for _ in range(dimension)]
        elif name == "temporal_drift":
            mean = 0.0 if index < count // 2 else 4.0
            sigma = 0.5 if index < count // 2 else 1.5
            vector = [rng.gauss(mean, sigma) for _ in range(dimension)]
        elif name == "adversarial":
            vector = [
                (0.5 + (1e-5 if (index + item) % 2 else -1e-5))
                if item % 3
                else (1000.0 if index % 127 == 0 else -1000.0 if index % 131 == 0 else 0.0)
                for item in range(dimension)
            ]
        else:
            raise ValueError(name)
        vector = [f32(value) for value in vector]
        result.append(vector)
        previous = vector
    return result


def synthetic_study() -> dict[str, object]:
    scenarios = [
        "uniform",
        "gaussian",
        "sparse",
        "heavy_tailed",
        "spatially_smooth",
        "spatially_discontinuous",
        "outlier_heavy",
        "temporal_drift",
        "adversarial",
    ]
    factories: list[Callable[[list[list[float]]], VectorEncoding]] = [
        encode_float16,
        lambda vectors: uniform_encoding(vectors, 16),
        lambda vectors: uniform_encoding(vectors, 8),
        lambda vectors: uniform_encoding(vectors, 4),
        block_int8_encoding,
        predictive_int8_encoding,
    ]
    output: dict[str, object] = {}
    for scenario_index, scenario in enumerate(scenarios):
        vectors = synthetic_vectors(scenario, 512, 32, 1701 + scenario_index)
        baseline = len(vectors) * len(vectors[0]) * 4
        rows = []
        for factory in factories:
            encoding = factory(vectors)
            metrics = vector_metrics(vectors, encoding.reconstructed)
            rows.append(
                {
                    "name": encoding.name,
                    "vector_storage_ratio": len(encoding.payload) / baseline,
                    "exception_rate": encoding.exception_rate,
                    "normalized_mse": metrics["normalized_mse"],
                    "mean_top10_overlap": metrics["mean_top10_overlap"],
                    "mean_threshold_f1": metrics["mean_threshold_f1"],
                    "application_distortion": metrics["application_distortion"],
                }
            )
        output[scenario] = rows
    return output


def strip_private_payloads(results: list[dict[str, object]]) -> list[dict[str, object]]:
    return [{key: value for key, value in row.items() if not key.startswith("_")} for row in results]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--db", type=Path, required=True, help="Compacted GrapheneDB directory")
    parser.add_argument("--output", type=Path, required=True, help="JSON report path")
    parser.add_argument("--skip-synthetic", action="store_true")
    args = parser.parse_args()

    store = parse_checkpoint(args.db)
    structural = structural_sections(store)
    factories: list[Callable[[list[list[float]]], VectorEncoding]] = [
        encode_float32,
        encode_float16,
        lambda vectors: uniform_encoding(vectors, 16),
        lambda vectors: uniform_encoding(vectors, 8),
        lambda vectors: uniform_encoding(vectors, 4),
        block_int8_encoding,
        predictive_int8_encoding,
    ]
    results = [measure_vector_candidate(store, structural, factory) for factory in factories]
    exact_payload = next(row["_payload"] for row in results if row["name"] == "binary_f32")
    baseline_size = store.anatomy["total_persisted"]
    results.extend(
        [
            measure_lossless("gzip_v2", store.baseline_blob, lambda data: gzip.compress(data, compresslevel=6), gzip.decompress, baseline_size),
            measure_lossless("zlib_v2", store.baseline_blob, lambda data: zlib.compress(data, level=6), zlib.decompress, baseline_size),
            measure_lossless("bz2_v2", store.baseline_blob, lambda data: bz2.compress(data, compresslevel=9), bz2.decompress, baseline_size),
            measure_lossless("lzma_v2", store.baseline_blob, lambda data: lzma.compress(data, preset=6), lzma.decompress, baseline_size),
            measure_lossless("zlib_binary_f32", exact_payload, lambda data: zlib.compress(data, level=6), zlib.decompress, baseline_size),
        ]
    )
    payload = {
        "schema": "graphenedb-storage-reduction-study-v1",
        "db": str(args.db),
        "nodes": len(store.nodes),
        "edges": len(store.edges),
        "dimension": store.dimension,
        "anatomy": store.anatomy,
        "exact_binary_structural_sections": {
            "metadata_dictionary_bytes": len(structural[0]),
            "node_records_without_vectors_bytes": len(structural[1]),
            "edge_records_bytes": len(structural[2]),
            "persistent_index_bundle_bytes": len(structural[3]),
            "sparse_node_index_bytes": len(structural[4]),
        },
        "distortion_weights": {
            "normalized_point_mse": 0.10,
            "regional_aggregate_error": 0.10,
            "top10_rank_error": 0.35,
            "threshold_error": 0.20,
            "path_error": 0.20,
            "topology_error": 0.05,
        },
        "candidates": strip_private_payloads(results),
        "synthetic_scenarios": {} if args.skip_synthetic else synthetic_study(),
        "unsupported_optional_codecs": ["zstd", "lz4", "snappy", "bitshuffle"],
        "notes": [
            "Candidate byte streams include versioned headers, per-section lengths and CRC32 checksums, a metadata dictionary, exact content, exact edge endpoints, exact fixed-six-decimal edge weights, and a sparse node-offset index.",
            "The Python harness is experimental. A production durable-format implementation must be C++20, endian-defined, fuzzed, migration-tested, and separately approved.",
            "Resident production memory and OS allocation are not inferred from Python object sizes; peak harness allocation is reported only as experimental process evidence.",
        ],
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    print(json.dumps({"ok": True, "output": str(args.output), "candidates": len(results)}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
