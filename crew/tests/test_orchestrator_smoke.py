"""Drive the full workflow offline with a fake client — no SDK, no API key.

Covers phase order, the plan checkpoint gate, and the bounded repair loop.
"""

import asyncio

import pytest

from crew.config import CrewConfig
from crew.orchestrator import run_task

from conftest import FakeClient, classify


def responder_for(tester_text: str):
    def responder(prompt: str) -> str:
        if classify(prompt) == "test":
            return tester_text
        return "done."

    return responder


def run(confirm, tester_text="VERDICT: PASS", config=None, session_id="sess-1", cost=None):
    """Run a task against a FakeClient and return (state, client)."""
    client = FakeClient(responder_for(tester_text), session_id=session_id, cost=cost)
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


def test_cost_accumulates_across_phases(monkeypatch, tmp_path):
    monkeypatch.chdir(tmp_path)
    # Each turn's result reports the same per-turn cost; the task total is the
    # sum over every phase that ran.
    state, client = run(confirm=lambda q: True, cost=0.01)
    assert state.total_cost_usd == pytest.approx(0.01 * len(client.prompts))
    assert len(client.prompts) >= 4  # at least plan, code, review, test


def test_no_cost_reported_stays_zero(monkeypatch, tmp_path):
    monkeypatch.chdir(tmp_path)
    state, _ = run(confirm=lambda q: True, cost=None)
    assert state.total_cost_usd == 0.0


def test_dry_run_plans_only(monkeypatch, tmp_path):
    monkeypatch.chdir(tmp_path)

    def no_confirm(q):
        raise AssertionError("confirm must not be called during a dry run")

    client = FakeClient(responder_for("VERDICT: PASS"))
    state = asyncio.run(
        run_task(
            "demo task",
            CrewConfig(),
            confirm=no_confirm,
            dry_run=True,
            client_factory=lambda c, r: client,
        )
    )
    assert [classify(p) for p in client.prompts] == ["plan"]  # only the planner ran
    assert state.phase == "plan"


def test_run_prints_phase_summary(monkeypatch, tmp_path, capsys):
    monkeypatch.chdir(tmp_path)
    run(confirm=lambda q: True, cost=0.01)
    out = capsys.readouterr().out
    assert "Run summary:" in out
    assert "PLAN" in out and "TEST" in out
    assert "Total" in out


def test_state_persisted_when_a_phase_errors(monkeypatch, tmp_path):
    """A mid-run failure must leave resumable state from the completed phases."""
    monkeypatch.chdir(tmp_path)

    def responder(prompt):
        if classify(prompt) == "code":
            raise RuntimeError("boom mid-run")
        return "done."

    client = FakeClient(responder)
    with pytest.raises(RuntimeError):
        asyncio.run(
            run_task(
                "demo task",
                CrewConfig(),
                confirm=lambda q: True,
                client_factory=lambda c, r: client,
            )
        )

    from crew import session as session_mod

    saved = session_mod.load(CrewConfig())
    assert saved.phase == "plan"        # last phase that completed before the error
    assert saved.session_id == "sess-1"  # captured, so `crew resume` can continue
