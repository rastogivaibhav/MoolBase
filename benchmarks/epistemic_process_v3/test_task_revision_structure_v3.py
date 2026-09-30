#!/usr/bin/env python3
from __future__ import annotations
import json
from pathlib import Path
from validate_tasks_v3_candidate import active_support_families, all_events

ROOT=Path(__file__).resolve().parent
doc=json.loads((ROOT/"tasks_v3_candidate.json").read_text())
revision=resolution=0
for ep in doc["episodes"]:
    o=ep["oracle"]
    events=sorted(all_events(ep),key=lambda e:e["step"])
    if o["revision_required"]:
        revision+=1
        r=o["expected_revision"]; assert r
        before=[e for e in events if e["step"]<r["trigger_step"] and e["kind"]=="support" and e["bears_on"]==r["from"] and e["independent"]]
        assert len({e["canonical_family_id"] for e in before})>=2,ep["id"]
        trigger=[e for e in events if e["step"]==r["trigger_step"] and e["kind"]=="refute" and e["bears_on"]==r["from"] and e["material"]]
        assert trigger,ep["id"]
        replacement=[e for e in events if e["kind"]=="support" and e["bears_on"]==r["to"] and e["independent"]]
        assert replacement and min(e["step"] for e in replacement)>r["trigger_step"],ep["id"]
        earned=[e for e in replacement if e["step"]<=r["replacement_earned_step"]]
        assert len({e["canonical_family_id"] for e in earned})>=2,ep["id"]
        assert r["replacement_earned_step"]-r["trigger_step"]<=3,ep["id"]
    if o["resolution_eligible"]:
        resolution+=1
        target=o["expected_terminal_committed"]
        assert target in {"H1","H2"},ep["id"]
        assert len(active_support_families(ep,target))>=2,ep["id"]
        assert not any(e["kind"]=="refute" and e["bears_on"]==target for e in events),ep["id"]
assert revision==80,revision
assert resolution>=80,resolution
print(f"cycle3_revision_structure=passed revision_required={revision} resolution_eligible={resolution}")
