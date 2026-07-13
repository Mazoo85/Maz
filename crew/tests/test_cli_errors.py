"""The CLI turns run failures into clean, resumable exits — not tracebacks."""

import pytest
import typer

from crew import cli, orchestrator
from crew.config import CrewConfig


def _patch_run(monkeypatch, exc):
    def boom(*a, **k):
        raise exc

    monkeypatch.setattr(orchestrator, "run_task_sync", boom)


def test_generic_error_exits_1(monkeypatch):
    _patch_run(monkeypatch, RuntimeError("kaboom"))
    with pytest.raises(typer.Exit) as ei:
        cli._drive("t", CrewConfig(), confirm=lambda q: True)
    assert ei.value.exit_code == 1


def test_missing_sdk_exits_1(monkeypatch):
    _patch_run(monkeypatch, ModuleNotFoundError("No module named 'claude_agent_sdk'"))
    with pytest.raises(typer.Exit) as ei:
        cli._drive("t", CrewConfig(), confirm=lambda q: True)
    assert ei.value.exit_code == 1


def test_keyboard_interrupt_exits_130(monkeypatch):
    _patch_run(monkeypatch, KeyboardInterrupt())
    with pytest.raises(typer.Exit) as ei:
        cli._drive("t", CrewConfig(), confirm=lambda q: True)
    assert ei.value.exit_code == 130


def test_success_does_not_raise(monkeypatch):
    monkeypatch.setattr(orchestrator, "run_task_sync", lambda *a, **k: None)
    cli._drive("t", CrewConfig(), confirm=lambda q: True)  # must not raise
