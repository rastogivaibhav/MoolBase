# Foundation Attempt Failures

1. `G0` is incomplete because the tested source came from a dirty worktree,
   not a clean checkout of a committed immutable revision.
2. The first command ran before a start timestamp and complete manifest were
   captured. This bundle therefore records the attempt but is not a
   preregistration-quality clean rerun.
3. `G2` is incomplete because `bench/deepmind/generate_causal_suite.*` and its
   scorer do not exist. The diagnostic run covered 12 cases instead of the
   required 5,000 graphs and 20,000 queries.
4. No result from the 12-case diagnostic ablation has been promoted into a
   formal `D0` acceptance metric.
