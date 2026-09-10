"""SENSE gathers every signal and survives any one of them failing."""

import json

from forge.config import ForgeConfig
from forge.models import Candidate
from forge.sense import candidate_from_dict, candidate_to_dict, read_pulse, sense, write_pulse


def _ok(name):
    return lambda root: [Candidate(task=f"{name} task", source=f"{name}:1", kind=name)]


def _boom(root):
    raise RuntimeError("signal exploded")


def test_gathers_from_every_collector(tmp_path):
    pulse = sense(tmp_path, ForgeConfig(),
                  collectors={"roadmap": _ok("roadmap"), "todo": _ok("todo")})
    assert len(pulse["candidates"]) == 2
    assert pulse["sources"]["roadmap"]["ok"] is True
    assert pulse["sources"]["roadmap"]["count"] == 1


def test_one_broken_collector_does_not_stop_the_night(tmp_path):
    pulse = sense(tmp_path, ForgeConfig(),
                  collectors={"roadmap": _ok("roadmap"), "ci": _boom})
    assert len(pulse["candidates"]) == 1
    assert pulse["sources"]["ci"]["ok"] is False
    assert "exploded" in pulse["sources"]["ci"]["error"]


def test_pulse_has_a_timestamp(tmp_path):
    pulse = sense(tmp_path, ForgeConfig(), collectors={})
    assert pulse["generated_at"].endswith("Z")


def test_write_and_read_round_trip(tmp_path):
    cfg = ForgeConfig()
    pulse = sense(tmp_path, cfg, collectors={"roadmap": _ok("roadmap")})
    path = write_pulse(pulse, tmp_path, cfg)
    assert path.exists()
    assert read_pulse(tmp_path, cfg)["candidates"][0]["task"] == "roadmap task"


def test_read_pulse_returns_empty_when_missing(tmp_path):
    assert read_pulse(tmp_path, ForgeConfig()) == {}


def test_candidate_dict_round_trip():
    c = Candidate(task="t", source="s", kind="todo", paths=("a.js",),
                  current_milestone=True, tests_nearby=True, detail="d")
    assert candidate_from_dict(candidate_to_dict(c)) == c


def test_candidate_dict_is_json_serialisable():
    c = Candidate(task="t", source="s", kind="todo", paths=("a.js",))
    assert json.loads(json.dumps(candidate_to_dict(c)))["paths"] == ["a.js"]


# Guards for misshapen collector returns (isinstance check) and read_pulse (dict check)


def _returns_dict(root):
    """Collector that returns a dict instead of list of Candidates."""
    return {"wrong": "shape"}


def _returns_strings(root):
    """Collector that returns a list of strings."""
    return ["not", "candidates"]


def _returns_dicts(root):
    """Collector that returns a list of dicts instead of Candidates."""
    return [{"task": "bad", "source": "wrong"}, {"task": "also bad"}]


def _returns_mixed(root):
    """Collector that returns a mixed list with Candidate and dict."""
    return [
        Candidate(task="good", source="mixed:1", kind="test"),
        {"not": "a candidate"}
    ]


def test_collector_returning_dict_fails_alone(tmp_path):
    """A collector returning a dict fails alone; healthy collector still contributes."""
    pulse = sense(tmp_path, ForgeConfig(),
                  collectors={"bad": _returns_dict, "good": _ok("good")})
    # Bad collector failed with error, zero count
    assert pulse["sources"]["bad"]["ok"] is False
    assert pulse["sources"]["bad"]["error"]  # non-empty
    assert pulse["sources"]["bad"]["count"] == 0
    # Good collector still contributes
    assert pulse["sources"]["good"]["ok"] is True
    assert pulse["sources"]["good"]["count"] == 1
    assert len(pulse["candidates"]) == 1


def test_collector_returning_strings_fails_alone(tmp_path):
    """A collector returning a list of strings fails alone."""
    pulse = sense(tmp_path, ForgeConfig(),
                  collectors={"bad": _returns_strings, "good": _ok("good")})
    # Bad collector failed
    assert pulse["sources"]["bad"]["ok"] is False
    assert pulse["sources"]["bad"]["error"]
    assert pulse["sources"]["bad"]["count"] == 0
    # Good collector still contributes
    assert pulse["sources"]["good"]["ok"] is True
    assert pulse["sources"]["good"]["count"] == 1
    assert len(pulse["candidates"]) == 1


def test_collector_returning_dicts_fails_alone(tmp_path):
    """A collector returning a list of dicts (not Candidates) fails alone."""
    pulse = sense(tmp_path, ForgeConfig(),
                  collectors={"bad": _returns_dicts, "good": _ok("good")})
    # Bad collector failed
    assert pulse["sources"]["bad"]["ok"] is False
    assert pulse["sources"]["bad"]["error"]
    assert pulse["sources"]["bad"]["count"] == 0
    # Good collector still contributes
    assert pulse["sources"]["good"]["ok"] is True
    assert pulse["sources"]["good"]["count"] == 1
    assert len(pulse["candidates"]) == 1


def test_collector_returning_mixed_list_fails_alone(tmp_path):
    """A collector returning mixed Candidate and dict fails at the dict."""
    pulse = sense(tmp_path, ForgeConfig(),
                  collectors={"bad": _returns_mixed, "good": _ok("good")})
    # Bad collector failed when it hit the non-Candidate item
    assert pulse["sources"]["bad"]["ok"] is False
    assert pulse["sources"]["bad"]["error"]
    assert pulse["sources"]["bad"]["count"] == 0
    # Good collector still contributes
    assert pulse["sources"]["good"]["ok"] is True
    assert pulse["sources"]["good"]["count"] == 1
    assert len(pulse["candidates"]) == 1


def test_read_pulse_with_json_list_returns_empty(tmp_path):
    """A pulse file with a JSON list (valid JSON, not dict) returns {}."""
    cfg = ForgeConfig()
    path = cfg.state_dir(tmp_path) / "pulse.json"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps([1, 2, 3]), encoding="utf-8")

    result = read_pulse(tmp_path, cfg)
    assert result == {}


def test_read_pulse_with_json_number_returns_empty(tmp_path):
    """A pulse file with a bare JSON number returns {}."""
    cfg = ForgeConfig()
    path = cfg.state_dir(tmp_path) / "pulse.json"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(42), encoding="utf-8")

    result = read_pulse(tmp_path, cfg)
    assert result == {}


def test_read_pulse_with_malformed_json_returns_empty(tmp_path):
    """A pulse file with malformed JSON returns {}."""
    cfg = ForgeConfig()
    path = cfg.state_dir(tmp_path) / "pulse.json"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("not valid json at all {][", encoding="utf-8")

    result = read_pulse(tmp_path, cfg)
    assert result == {}
