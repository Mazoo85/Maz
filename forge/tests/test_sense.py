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
