"""Shared test helpers: a fake SDK client that drives the orchestrator offline."""

from __future__ import annotations


class FakeBlock:
    def __init__(self, text: str) -> None:
        self.text = text


class FakeAssistant:
    """Mimics an SDK assistant message: a ``content`` list of text blocks."""

    def __init__(self, text: str) -> None:
        self.content = [FakeBlock(text)]


class FakeResult:
    """Mimics the terminal result message that carries the session id and cost."""

    def __init__(self, session_id: str, cost: float | None = None) -> None:
        self.content = None
        self.session_id = session_id
        if cost is not None:
            self.total_cost_usd = cost


class FakeClient:
    """Async-context-manager stand-in for ``ClaudeSDKClient``.

    Records every prompt it receives and replies with scripted text produced by
    ``responder(prompt)``, followed by a result message carrying ``session_id``
    (and ``cost`` per turn, when set).
    """

    def __init__(self, responder, session_id: str = "sess-1", cost: float | None = None) -> None:
        self.responder = responder
        self.session_id = session_id
        self.cost = cost
        self.prompts: list[str] = []
        self._pending = ""

    async def __aenter__(self) -> "FakeClient":
        return self

    async def __aexit__(self, *exc) -> bool:
        return False

    async def query(self, prompt: str) -> None:
        self.prompts.append(prompt)
        self._pending = self.responder(prompt)

    async def receive_response(self):
        yield FakeAssistant(self._pending)
        yield FakeResult(self.session_id, self.cost)


def classify(prompt: str) -> str:
    """Map an orchestrator phase prompt to a short role tag."""
    p = prompt.lower()
    if "planner agent" in p:
        return "plan"
    if "reviewer agent" in p:
        return "review"
    if "tester agent" in p:
        return "test"
    if "coder agent to apply" in p:
        return "applyfix"
    if "coder agent to fix" in p:
        return "fix"
    if "coder agent" in p:
        return "code"
    return "other"
