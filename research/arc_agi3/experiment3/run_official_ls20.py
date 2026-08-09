from __future__ import annotations

import base64
import gzip
import hashlib
import importlib.util
import json
import logging
import sys
import types
from pathlib import Path

import arc_agi
from arcengine import FrameData, GameAction, GameState

ROOT = Path(__file__).resolve().parent
PAYLOAD_PARTS = sorted(ROOT.glob("agent_payload.part*"))
OUT = ROOT / "official_ls20_result.json"
EXPECTED_AGENT_SHA256 = "91e4d245a43245bed64ac58bc9dd542864233adc828397940d00f2bb560857e4"


def install_agent_base_stub() -> None:
    """Expose only the Agent surface MyAgent needs, while using real ARC classes."""
    agents_pkg = types.ModuleType("agents")
    agent_mod = types.ModuleType("agents.agent")

    class Agent:
        MAX_ACTIONS = 80

        def __init__(self, *args, **kwargs):
            self.game_id = kwargs.get("game_id", "ls20")
            self.action_counter = 0

        @property
        def name(self):
            return f"{self.game_id}.{self.__class__.__name__.lower()}"

    agent_mod.Agent = Agent
    agents_pkg.agent = agent_mod
    sys.modules["agents"] = agents_pkg
    sys.modules["agents.agent"] = agent_mod


def load_my_agent():
    install_agent_base_stub()
    if not PAYLOAD_PARTS:
        raise RuntimeError("no Experiment-3 agent payload parts found")
    payload = "".join(part.read_text().strip() for part in PAYLOAD_PARTS)
    src = gzip.decompress(base64.b64decode(payload))
    observed_sha = hashlib.sha256(src).hexdigest()
    if observed_sha != EXPECTED_AGENT_SHA256:
        raise RuntimeError(
            f"Experiment-3 agent SHA mismatch: expected={EXPECTED_AGENT_SHA256} observed={observed_sha}"
        )
    agent_path = ROOT / "_materialized_my_agent.py"
    agent_path.write_bytes(src)
    spec = importlib.util.spec_from_file_location("experiment3_my_agent", agent_path)
    if spec is None or spec.loader is None:
        raise RuntimeError("cannot load Experiment 3 MyAgent")
    mod = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = mod
    spec.loader.exec_module(mod)
    return mod.MyAgent, observed_sha


def convert(raw) -> FrameData:
    if raw is None:
        raise RuntimeError("ARC environment returned None")
    return FrameData(
        game_id=raw.game_id,
        frame=[arr.tolist() if hasattr(arr, "tolist") else arr for arr in raw.frame],
        state=raw.state,
        levels_completed=raw.levels_completed,
        win_levels=raw.win_levels,
        guid=getattr(raw, "guid", ""),
        full_reset=getattr(raw, "full_reset", False),
        available_actions=raw.available_actions,
    )


def action_data(action: GameAction) -> dict:
    try:
        return action.action_data.model_dump()
    except Exception:
        return {}


def available_ids(frame: FrameData) -> list[int]:
    out = []
    for value in list(frame.available_actions or []):
        if isinstance(value, int):
            out.append(value)
        elif hasattr(value, "value") and isinstance(value.value, int):
            out.append(int(value.value))
        elif hasattr(value, "id"):
            out.append(int(value.id))
    return out


def main() -> None:
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
    MyAgent, agent_sha = load_my_agent()

    # NORMAL downloads the official public LS20 source and executes it through
    # LocalEnvironmentWrapper. No LS20 semantic labels are supplied to MyAgent.
    arc = arc_agi.Arcade()
    env = arc.make("ls20", save_recording=True, include_frame_data=True)
    if env is None:
        raise RuntimeError("arc.make('ls20') failed")

    raw = env.observation_space
    if raw is None:
        raw = env.reset()
    latest = convert(raw)
    game_id = latest.game_id or env.info.game_id
    agent = MyAgent(game_id=game_id)
    frames: list[FrameData] = [latest]
    trace: list[dict] = []

    max_actions = int(getattr(agent, "MAX_ACTIONS", 80))
    for turn in range(max_actions):
        latest = frames[-1]
        if agent.is_done(frames, latest):
            break

        action = agent.choose_action(frames, latest)
        reasoning = getattr(action, "reasoning", None)
        if reasoning is not None and not isinstance(reasoning, dict):
            reasoning = {"text": str(reasoning)}
        raw = env.step(action, data=action_data(action), reasoning=reasoning)
        nxt = convert(raw)
        frames.append(nxt)
        agent.action_counter = turn + 1

        kernel = getattr(agent, "kernel", None)
        top_goal = None
        control = None
        critic_regime = None
        if kernel is not None:
            try:
                goals = kernel.top_goal_hypotheses(limit=1)
                if goals:
                    g = goals[0]
                    top_goal = {
                        "family": getattr(g, "family", None),
                        "statement": getattr(g, "statement", None),
                        "support": getattr(g, "support", None),
                        "contradiction": getattr(g, "contradiction", None),
                        "confidence": getattr(g, "confidence", None),
                        "origin": getattr(g, "origin", None),
                    }
            except Exception:
                pass
            try:
                status = kernel.control_status()
                control = status if isinstance(status, str) else getattr(status, "value", repr(status))
            except Exception:
                pass
            try:
                critic = getattr(kernel, "last_critic", None)
                critic_regime = getattr(getattr(critic, "regime", None), "value", None)
            except Exception:
                pass

        rec = {
            "turn": turn + 1,
            "action": action.name,
            "state": nxt.state.name,
            "levels_completed": nxt.levels_completed,
            "available_actions": available_ids(nxt),
            "control_status": control,
            "critic_regime": critic_regime,
            "top_goal": top_goal,
        }
        trace.append(rec)
        print(json.dumps(rec, sort_keys=True))

        if nxt.state is GameState.WIN:
            break

    score = None
    try:
        card = arc.get_scorecard()
        if card is not None:
            if hasattr(card, "model_dump"):
                score = card.model_dump(mode="json")
            elif hasattr(card, "dict"):
                score = card.dict()
            else:
                score = str(card)
    except Exception as exc:
        score = {"error": repr(exc)}

    kernel = getattr(agent, "kernel", None)
    summary = {
        "environment": "official ARC-AGI-3 local LS20",
        "agent_sha256": agent_sha,
        "game_id": game_id,
        "actions": len(trace),
        "levels_completed": frames[-1].levels_completed,
        "state": frames[-1].state.name,
        "win_levels": frames[-1].win_levels,
        "max_actions": max_actions,
        "scorecard": score,
        "no_silent_goal_promotion": None,
        "trace": trace,
    }
    if kernel is not None:
        try:
            goals = kernel.top_goal_hypotheses(limit=1000)
            summary["no_silent_goal_promotion"] = all(
                getattr(g, "origin", None) == "hypothetical" for g in goals
            )
        except Exception:
            pass

    OUT.write_text(json.dumps(summary, indent=2, sort_keys=True, default=str))
    print(
        "FINAL_RESULT="
        + json.dumps(
            {
                k: summary[k]
                for k in [
                    "actions",
                    "levels_completed",
                    "state",
                    "win_levels",
                    "no_silent_goal_promotion",
                    "agent_sha256",
                ]
            },
            sort_keys=True,
        )
    )


if __name__ == "__main__":
    main()
