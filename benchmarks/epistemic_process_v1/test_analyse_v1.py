#!/usr/bin/env python3
import json
import tempfile
import unittest
from pathlib import Path

import analyse_v1


class AnalyseTests(unittest.TestCase):
    def test_layer_deltas_preserve_raw_metric_direction(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            for index, config in enumerate(analyse_v1.CONFIGS):
                raw = {
                    "score_bearing": True,
                    "episodes": [
                        {
                            "episode_id": "EP1",
                            "execution_seconds": 1.0 + index,
                            "runtime_receipts": [
                                {
                                    "visited_states": index,
                                    "expansion_rounds": index,
                                    "evidence_edge_count": index + 1,
                                }
                            ],
                        }
                    ],
                }
                receipt = {
                    "episode_id": "EP1",
                    "terminal_status": "resolved",
                    "terminal_hypothesis": "H1",
                    "transitions": [],
                    "challenges": [],
                }
                aggregate = {
                    "eligible_episodes": 1,
                    "excluded_episodes": 0,
                    "failure_classes": {},
                    "evidence_use_rate": {"rate": 0.25 * index},
                    "refutation_response_rate": {"rate": 0.25 * index},
                    "revision_inertia_steps": {"mean": float(3 - index)},
                    "false_convergence_rate": {"rate": 0.0},
                    "independent_evidence_convergence": {
                        "earned_resolution_rate": 0.25 * index
                    },
                    "selective_coverage": {
                        "coverage_rate": 1.0,
                        "abstention_rate": 0.0,
                        "covered_accuracy": 0.25 * index,
                        "overall_terminal_accuracy": 0.25 * index,
                    },
                    "receipt_completeness": {"rate": 1.0},
                }
                evaluation = {
                    "score_bearing": True,
                    "aggregate": aggregate,
                    "episodes": [
                        {
                            "episode_id": "EP1",
                            "selective_coverage": {
                                "terminal_correct": index > 0,
                                "covered": True,
                            },
                            "refutation_response": {"responded": index > 0},
                            "revision_inertia_steps": 3 - index,
                            "false_convergence": {
                                "false_convergence": False
                            },
                            "independent_evidence_convergence": {
                                "terminal_independent_family_count": index
                            },
                            "receipt_completeness": {
                                "completeness_rate": 1.0
                            },
                        }
                    ],
                }
                (root / f"raw-{config}.json").write_text(json.dumps(raw))
                (root / f"receipts-{config}.json").write_text(
                    json.dumps([receipt])
                )
                (root / f"scores-{config}.json").write_text(
                    json.dumps(evaluation)
                )

            report = analyse_v1.build_report(root)
            self.assertEqual(
                report["layer_effects"]["B0->G0"]["metrics"][
                    "evidence_use_rate"
                ],
                0.25,
            )
            self.assertEqual(
                report["layer_effects"]["G1->G2"]["cost"][
                    "execution_seconds_total"
                ],
                1.0,
            )
            self.assertEqual(
                report["claim_interpretation"], "not_performed_by_script"
            )


if __name__ == "__main__":
    unittest.main()
