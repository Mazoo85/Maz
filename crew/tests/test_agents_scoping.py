"""Tool scoping is the safety guarantee: only the coder may edit files."""

from crew.agents import READ_ONLY, ROLES_BY_NAME
from crew.config import CrewConfig

EDIT_TOOLS = {"Edit", "Write"}


def test_planner_and_reviewer_are_read_only():
    assert ROLES_BY_NAME["planner"].tools == READ_ONLY
    assert ROLES_BY_NAME["reviewer"].tools == READ_ONLY
    for role in ("planner", "reviewer"):
        assert not EDIT_TOOLS & set(ROLES_BY_NAME[role].tools)


def test_only_coder_can_edit():
    editors = [name for name, r in ROLES_BY_NAME.items() if EDIT_TOOLS & set(r.tools)]
    assert editors == ["coder"]


def test_tester_cannot_edit_but_can_run():
    tester = ROLES_BY_NAME["tester"]
    assert not EDIT_TOOLS & set(tester.tools)
    assert "Bash" in tester.tools


def test_models_resolve_from_config():
    cfg = CrewConfig()
    assert ROLES_BY_NAME["reviewer"].model(cfg) == cfg.reviewer_model
    assert ROLES_BY_NAME["tester"].model(cfg) == cfg.tester_model
