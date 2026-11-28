# EGPM-01 native retrieval diagnostic

This isolated research adapter measures BM25 and the actual MoolBase HTTP vector retrieval endpoint on the same corpus, using MuSiQue validation rows and the MeMo authors' published selected distractor manifest. Default CI examines the first 40 validation rows, not the full paper benchmark. It records matching IDs, gold passage IDs, ranked document IDs and input hashes. No answers or gold evidence flags enter the ranking function or database text. The manifest and dataset are retrieved at execution; hashes detect changes. For a paper-quality run pin immutable upstream revisions and verify all 1,000 expected IDs and distractors.

Run the command in .github/workflows/egpm01-native.yml. The native lifecycle smoke separately tests the real engine; it does not measure natural-language answer accuracy. The vector endpoint uses the server's own embedding path; it is not the complete evidence-reasoning arm C. No score should be called MeMo accuracy or hybrid accuracy.

## Model phase

Authors' MuSiQue memory checkpoint: https://huggingface.co/Glow-AI/QWEN2.5-14b_instruct-MSQ1000_step1-5_with_N_neg_batchsize256_best_epoch

Original evaluation: https://github.com/arunv3rma/MeMo/tree/main/evaluation_pipeline/structured_multi_turn

Reuse the original checkpoint and the structured evaluator, retaining its question IDs, frozen executive, judge, temperatures and runs. For arm D, verify recalled candidate claims against the same corpus, ingest externally determined support/refutation into the MoolBase runtime and retain receipts. Do not label retrieved claims verified without a semantic verifier. Arm A/C/D must share executive, evaluator and budget; include closed-book, oracle, and retrieval-plus-verifier controls. Record model hashes, per-question predictions, errors and costs. GPU and judge access are not supplied by this CPU workflow.
