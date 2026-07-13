"""_extract_text / _extract_cost pull data out of duck-typed SDK messages."""

from crew.orchestrator import _extract_cost, _extract_text

from conftest import FakeAssistant, FakeBlock, FakeResult


class StrContent:
    def __init__(self, s):
        self.content = s


class NoTextBlock:
    """A block with no .text (e.g. a tool-use block)."""


def test_list_of_text_blocks():
    msg = FakeAssistant("hello ")
    msg.content.append(FakeBlock("world"))
    assert _extract_text(msg) == "hello world"


def test_string_content():
    assert _extract_text(StrContent("plain")) == "plain"


def test_none_content():
    assert _extract_text(FakeResult("sess")) == ""


def test_missing_content_attr():
    assert _extract_text(object()) == ""


def test_non_text_blocks_ignored():
    class Msg:
        content = [NoTextBlock(), FakeBlock("kept")]

    assert _extract_text(Msg()) == "kept"


def test_extract_cost_present():
    assert _extract_cost(FakeResult("s", cost=0.0123)) == 0.0123


def test_extract_cost_absent():
    assert _extract_cost(FakeResult("s")) is None
    assert _extract_cost(FakeAssistant("hi")) is None
