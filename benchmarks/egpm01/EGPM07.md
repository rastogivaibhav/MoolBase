# EGPM07 development protocol and EGPM06 manual claim review

EGPM06 [run 45905423044](https://github.com/rastogivaibhav/MoolBase/actions/runs/45905423044) 8/8 jobs and staged tests passed. All eight artifacts downloaded and inspected (15158291625, 15158975330, 15157414765, 15159335244, 15158446577, 15157539602, 15158119705, 15159095021). 21 admitted edges across 40 graph arms: 16 support, 5 refute. Eight unique (claim, exact source text, verdict) combinations, not 21 independent facts. Manual source review (not blinded expert precision):
| Claim | Count | NLI | Reviewer |
|---|---:|---|---|
| SMECO headquarters Hughesville |4|support|directly supported|
| The Red Tree authored by Shaun Tan |1|support|directly supported|
| Empire Sports Network owned by Adelphia |5|support|directly supported|
| Almost Made Ya performed by Ali & Gipp |1|support|directly supported|
| Almost Made Ya performed by LeToya Luckett |3|support|featured artist; participation plausible, primary performer ambiguous|
| Almost Made Ya performed by LeToya Luckett |3|refute|source says featuring her, no explicit contradiction|
| Almost Made Ya performed by Murphy Lee |2|refute|source says features on a different track, does not explicitly rule out participation; unresolved|
| European Movement Germany member of Bernd Hüttemann |2|support|incorrect role / co-occurrence|
11/21 clearly supported, 3/21 ambiguous, 7/21 unsupported as labelled. This is an engineering diagnostic, not a generalisable precision estimate. Raw QA EM: MeMo 2/8, oracle text 4/8, all graph arms 0/8. Oracle graph 5 admitted edges and no complete path. Gold paragraphs sometimes under-entail question relation; do not relax standards.

EGPM07: same eight development questions, same 120-row corpus, exact prior GGUF digests checked and independent NLI revision checked. Candidate extraction from stored windows (up to 3), one candidate-conditioned two-link proof per candidate, rather than a single blind plan. Different candidates may use different relation plans. Whole short document windows preserve titles and neighboring sentences; exact offsets and hashes validated against loaded corpus. Both NLI (.8 unchanged) and separate executive critic must agree; critic also checks full question constraints. This critic shares model weights with executive, so not a second independent model. No alias joins unless exact normalization or source-grounded NLI + critic identity. Verify prefix and query missing links with bounded expansion (10 initial, up to 6 additional documents); preserve prior proof attempts and competing answers. Native graph/Python parity checked for each proof; matched text, verifier-only and native epistemic controls; separate oracle after ordinary predictions. Track model call counts, output caps, prompt chars and wall time, not exact matched tokens; this is an adapted development experiment. Only two-hop; do not run held-out until generalized to 3/4 hop, full corpus and frozen comparable budgets. Real accepted path and provenance audit needed before G3/G4. No threshold changes or gold leakage.

Limitations: candidate list can miss correct answer, critic may be wrong, relation extraction remains generative, separate graph per candidate (not shared durable knowledge graph), explicit identity limited to one window. No claim of superiority or generalisation. 
