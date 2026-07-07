# Codex Notes

Codex agents should use `AGENTS.md` as the primary repository instruction file.

Short version: GrapheneDB is an embedded C++20 causal/lattice-memory database for AI memory and explainable retrieval. Preserve the embedded library plus CLI boundary, keep durable format changes documented and tested, and align release claims with `reports/GA_STATUS_REPORT.md`.

Immediate next step: open a PR from `codex/rc5-developer-preview` to `master`, describe it as the RC5 public developer-preview candidate, link the preserved evidence in `reports/GA_STATUS_REPORT.md`, `reports/preview-hardware/20260707-141656/PREVIEW_HARDWARE_SUMMARY.md`, `reports/ga-readiness/20260707-142146/GA_READINESS_SUMMARY.md`, and `reports/ga-evidence/20260707-142233.zip`, then fix only CI/reviewer issues needed to merge. Do not start enterprise-GA work in that PR.
