# V12 score-interpretation correction — no cherry-picking

**19 October 2026; identified after the first successful execution at commit `cbf94477237ce191a896af7bf33154496735042b`.**

The raw run established G1/G2 **abstained/open at T0 and made speculative proposals at T1**. The original scoring names `changed_selected_hypothesis_n` and `revised_to_fixture_preferred_n` could incorrectly imply *correction of a wrong earlier conclusion*. But the earlier result was `none`, not a wrong committed hypothesis.

This correction changes only evaluation terminology and predicates, not data, tasks, candidate cause labels, algorithm, decision thresholds, model outputs or runtime execution. The old score artifact remains available at the exact initial SHA. New receipts distinguish:
- `new_proposals_after_evidence`: no proposal at T0 and a proposal at T1;
- `changed_between_two_explicit_proposals`: different nonempty proposed cause at T0 vs T1;
- `corrected_wrong_prior_proposal_n`: earlier proposal existed, was not the fixture cause, and T1 proposed the fixture cause;
- `corrected_wrong_committed_conclusion_n`: requires an earlier genuinely committed result, not speculative statuses. None are expected/earned here.

The synthetic labels are not independent truth. As before, actual external intervention adjudication, LLM comparator baselines and unseen-case learning are NOT_RUN. This is a methodological correction to avoid falsely claiming revision or learning; preserve old evaluation for reproducibility.
