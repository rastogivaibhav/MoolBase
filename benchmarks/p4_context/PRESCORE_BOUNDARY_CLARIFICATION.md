# P4 Pre-Score Boundary Clarification — No Outcomes Evaluated
Date: 2026-10-18. Experiment `MOOL-P4-CONTEXT-001`.

The frozen protocol described "all 2,922 native contextual weather predictions" although the lagged contextual baseline has no previous-day meteorological input on each city's very first date (2012-01-01). The existing P3 Python contextual baseline likewise begins from the **second date** per city, skipping its first date without training its logistic parameters. Thus cross-implementation parity means all **2,920 eligible contextual predictions**, exactly matching the existing P3 lagged-feature control; P1 Beta records may cover all 2,922 rows. Both 2015 holdouts are completely covered (730 predictions).

This is a **pre-score feasibility correction**, not a change of model, split, feature or claim thresholds. No P4 evaluation outputs had been inspected when this clarification was committed; the exact chronological implementation later should preserve this explanation. The original frozen preregistration is unchanged. If the C++/Python weather parity fails, the experiment is invalid, not a score-bearing negative finding.
