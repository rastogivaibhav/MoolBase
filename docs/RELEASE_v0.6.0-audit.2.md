# MoolBase 0.6.0-audit.2 — evaluation-build notes

Published 2026-10-04. This is the corrected developer evaluation build distributed by the Evidence Lab, not a new official release tag or GA release. The [original alpha.2 release notes](RELEASE_v0.6.0-alpha.2.md) describe the historical release; its assets are unchanged.

## What changed

A retired evidence path could displace an active path from the same source family and suppress eligible support or opposition. [PR #111](https://github.com/rastogivaibhav/MoolBase/pull/111) fixes representative selection while retaining every path for audit. Sixteen regression cases cover lifecycle state, evidence role and insertion order. There is no storage-format change.

The customer adapter reports configured runtime bounds, verifier identity and prior bundle reference. Applications must separately persist receipts and prior reasoning state. The memory example now shows that retiring one record leaves other active copies in place; stale copies require explicit retirement.

CI uses the existing preregistered V2 perturbation manifest with its matching runner. Historical campaign checks replay their original source separately from current-engine checks. Frozen manifests and historical release artifacts remain unchanged.

## Source and validation

- Merged source: [`0f19727b80007e875dea62dca7ab50e281d008e6`](https://github.com/rastogivaibhav/MoolBase/commit/0f19727b80007e875dea62dca7ab50e281d008e6).
- Artifact build and tested PR head: `b01c22334a08e1ad26dee0cff21a37b04ef22c82`; its source tree is identical to the merge.
- All 15 workflows passed on that exact PR head, including Linux, macOS and Windows checks. Post-merge validation passed 56 native CTest groups and five V2 perturbations.
- Actual native/WASM parity covered 31 states across three fixtures; seven WASM adversarial groups and 15 worker contract checks passed.

The [download section](https://moolbase.rasvai.com/index.html#developer) provides DB, examples and source ZIPs with SHA-256 checksums, per-file manifests and SPDX inventories. [Engine provenance](https://moolbase.rasvai.com/engine-provenance.json) records source and binary digests. See the [installation guide](DEVELOPER_QUICKSTART.md) and [verification boundaries](https://moolbase.rasvai.com/VALIDATION.html).

## Evaluation boundaries

The fixtures use synthetic typed evidence, caller-supplied source families and verification certificates. These tests establish the tested engine behavior, not natural-language truth, source authenticity, calibrated confidence or general superiority. Browser files are session-local and refresh resets them. Adapter exports are reduced receipts, not complete native receipts.

An independent validation supplied by the project owner reports successful live desktop/mobile rendering and WASM initialization. It explicitly did not complete the full browser click-through. Fresh end-to-end browser interactions, receipt downloads and the key mobile path remain pending; this build is not described as browser E2E certified.
