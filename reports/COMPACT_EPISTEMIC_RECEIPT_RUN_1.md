# Compact epistemic receipt validation — Run 1

## Purpose

Provide a durable, content-addressed answer artifact so callers do not need to persist the complete FiberBundle and every recursive-cycle workspace.

## Receipt contents

- selected target and governed status;
- final bundle hash and snapshot version;
- selected FiberPath IDs;
- source, evidence-family and derivation lineage;
- verifier versions;
- independent-evidence-family count;
- contradiction mass and completeness;
- evidence-edge references;
- Lyapunov energy and semantic-verification state;
- residual uncertainty;
- deterministic content hash.

The receipt contains references and summaries, not copies of source documents, graph indexes or complete bundles.

## Executed validation

The standalone contract was compiled with C++20 and `-Wall -Wextra -Wpedantic -Werror`, then executed successfully:

```text
compact_epistemic_receipt_contract_passed=true
```

It verified deterministic hashing, canonical sorting/deduplication, selected-path lineage retention and hash change when material uncertainty changes.

Source SHA-256 values from the executed local test:

```text
src/epistemic_receipt.cpp  5e36a46e3b0ac552b1dc78c5600b4d1762c6065a8fe5bb01f7476e25090f2a3a
tests/test_epistemic_receipt.cpp  8fe9a224a28d71abbc8e4242a0e1cabf0e2700f27d29198ba4ac2adc754c5ad9
```

## Claim boundary

This proves deterministic receipt construction for structured runtime output. It does not prove semantic truth or define an organisation's retention policy. Full bundles may still be retained explicitly for audit, debugging or research.
