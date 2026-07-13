"""Drive the full workflow offline with a fake client — no SDK, no API key.

Covers phase order, the plan checkpoint gate, and the bounded repair loop.
"""

import asyncio

from crew.config import CrewConfig
from crew.orchestrator import run_task

from conftest import FakeClient, classify


def responder_for(tester_text: str):
    def responder(prompt: str) -> str:
        if classify(prompt) == "test":
            return tester_text
        return "done."

    return responder


def run(confirm, tester_text="VERDICT: PASS", config=None, session_id="sess-1"):
    """Run a task against a FakeClient and return (state, client)."""
    client = FakeClient(responder_for(tester_text), session_id=session_id)
    cfg = config or CrewConfig()
    state = asyncio.run(
        run_task(
            "demo task",
            cfg,
            confirm=confirm,
            client_factory=lambda c, r: client,
        )
    )
    return state, client


def tags(client):
    return [classify(p) for p in client.prompts]


def test_phase_order(monkeypatch, tmp_path):
    monkeypatch.chdir(tmp_path)
    state, client = run(confirm=lambda q: True)
    seq = [t for t in tags(client) if t in ("plan", "code", "review", "test")]
    assert seq == ["plan", "code", "review", "test"]
    assert state.session_id == "sess-1"
    assert state.phase == "done"


def test_declining_plan_stops_before_coding(monkeypatch, tmp_path):
    monkeypatch.chdir(tmp_path)
    _, client = run(confirm=lambda q: False)
    assert tags(client) == ["plan"]  # nothing after the plan checkpoint


def test_repair_loop_hits_bound(monkeypatch, tmp_path):
    monkeypatch.chdir(tmp_path)
    cfg = CrewConfig(max_fix_rounds=3)
    _, client = run(confirm=lambda q: True, tester_text="VERDICT: FAIL", config=cfg)
    t = tags(client)
    assert t.count("test") == 3  # tested each round up to the bound
    assert t.count("fix") == 2   # fixed after rounds 1 and 2, not after the last


def test_passing_tests_break_immediately(monkeypatch, tmp_path):
    monkeypatch.chdir(tmp_path)
    _, client = run(confirm=lambda q: True, tester_text="12 passed\nVERDICT: PASS")
    t = tags(client)
    assert t.count("test") == 1
    assert t.count("fix") == 0
