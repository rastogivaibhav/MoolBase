from __future__ import annotations

import base64
import gzip
import importlib.util
import json
import logging
import sys
import types
from pathlib import Path

import arc_agi
from arcengine import FrameData, GameAction, GameState

ROOT = Path(__file__).resolve().parent
PAYLOAD = ROOT / "agent_payload.b64"
OUT = ROOT / "official_ls20_result.json"


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
    src = gzip.decompress(base64.b64decode(PAYLOAD.read_text().strip()))
    agent_path = ROOT / "_materialized_my_agent.py"
    agent_path.write_bytes(src)
    spec = importlib.util.spec_from_file_location("experiment3_my_agent", agent_path)
    if spec is None or spec.loader is None:
        raise RuntimeError("cannot load Experiment 3 MyAgent")
    mod = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = mod
    spec.loader.exec_module(mod)
    return mod.MyAgent


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


def main() -> None:
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
    MyAgent = load_my_agent()

    # NORMAL downloads the official public LS20 source, then executes it locally.
    arc = arc_agi.Arcade()
    env = arc.make("ls20", save_recording=True, include_frame_data=True)
    if env is None:
        raise RuntimeError("arc.make('ls20') failed")

    raw = env.observation_space
    if raw is None:
        raw = env.reset()
    latest = convert(raw)
    game_id = latest.game_id or getattr(env.info, "game_id", "ls20")
    agent = MyAgent(game_id=game_id)
    frames: list[FrameData] = [latest]
    trace: list[dict] = []

    max_actions = int(getattr(agent, "MAX_ACTIONS", 80))
    for turn in range(max_actions + 1):
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

        rec = {
            "turn": turn + 1,
            "action": action.name,
            "state": nxt.state.name,
            "levels_completed": nxt.levels_completed,
            "available_actions": [int(a) for a in (nxt.available_actions or [])],
            "control_status": control,
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
            summary["no_silent_goal_promotion"] = all(getattr(g, "origin", None) == "hypothetical" for g in goals)
        except Exception:
            pass

    OUT.write_text(json.dumps(summary, indent=2, sort_keys=True, default=str))
    print("FINAL_RESULT=" + json.dumps({k: summary[k] for k in ["actions", "levels_completed", "state", "win_levels", "no_silent_goal_promotion"]}, sort_keys=True))


if __name__ == "__main__":
    main()
