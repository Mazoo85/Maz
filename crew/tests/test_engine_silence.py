"""A dead engine must not read as a finished run.

The Forge handed Crew a job, the SDK's underlying process exited 255, and Crew
printed "Crew finished" and exited 0. The Forge recorded a successful night
that had simply changed nothing — a total failure, filed as a quiet success.

`receive_response()` yielding nothing is the signature: any real phase yields
at least a terminal result message.
"""

from __future__ import annotations

import asyncio

import pytest
from conftest import FakeClient, FakeAssistant, FakeResult

from crew.config import CrewConfig
from crew.orchestrator import EngineSilentError, run_task


class SilentClient(FakeClient):
    """An engine that accepts the prompt and never answers."""

    async def receive_response(self):
        return
        yield  # unreachable; makes this an async generator


def _run(client):
    return asyncio.run(
        run_task(
            "add a test",
            CrewConfig(),
            confirm=lambda *a, **k: True,
            client_factory=lambda c, r: client,
        )
    )


def test_a_silent_engine_raises_rather_than_finishing():
    with pytest.raises(EngineSilentError) as exc:
        _run(SilentClient(lambda p: ""))
    # The message has to be actionable: a person reading it should know where
    # to look, not just that something went wrong.
    assert "did not run" in str(exc.value)
    assert "API key" in str(exc.value)


def test_a_phase_that_answers_briefly_is_still_a_real_phase():
    """The guard must key on silence, not on brevity — an engine that answers
    with little or no text has still run, and must not be failed for it."""

    class TerseClient(FakeClient):
        async def receive_response(self):
            yield FakeResult(self.session_id, None)

    state = _run(TerseClient(lambda p: ""))
    assert state.phase == "done"
