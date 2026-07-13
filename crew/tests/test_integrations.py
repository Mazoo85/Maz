"""Pure decision logic for the opt-in GitHub CI integration."""

from dataclasses import replace

from crew.agents import ROLES_BY_NAME
from crew.config import CrewConfig
from crew.integrations import (
    GITHUB_TESTER_TOOLS,
    augment_tester_prompt,
    augment_tester_tools,
    github_ci_enabled,
    github_mcp_servers,
    github_token,
)

OFF = CrewConfig()
ON = replace(CrewConfig(), github_ci=True)
TOK = {"GITHUB_TOKEN": "ghp_x"}


def test_token_lookup_order():
    assert github_token({"GITHUB_TOKEN": "a"}) == "a"
    assert github_token({"GITHUB_PERSONAL_ACCESS_TOKEN": "b"}) == "b"
    assert github_token({}) is None


def test_enabled_requires_flag_and_token():
    assert github_ci_enabled(OFF, TOK) is False          # flag off
    assert github_ci_enabled(ON, {}) is False            # no token
    assert github_ci_enabled(ON, TOK) is True            # both


def test_mcp_server_shape_injects_token():
    servers = github_mcp_servers(TOK)
    gh = servers["github"]
    assert gh["command"] == "npx"
    assert "@modelcontextprotocol/server-github" in gh["args"]
    assert gh["env"]["GITHUB_PERSONAL_ACCESS_TOKEN"] == "ghp_x"


def test_tools_unchanged_when_disabled():
    base = ROLES_BY_NAME["tester"].tools
    assert augment_tester_tools(base, OFF, TOK) == base
    assert augment_tester_tools(base, ON, {}) == base  # no token -> disabled


def test_tools_get_github_when_enabled():
    base = ROLES_BY_NAME["tester"].tools
    out = augment_tester_tools(base, ON, TOK)
    assert out[: len(base)] == base
    for t in GITHUB_TESTER_TOOLS:
        assert t in out
    # Base tools (real edit-guardrail set) are preserved, none dropped.
    assert set(base).issubset(set(out))


def test_prompt_addendum_only_when_enabled():
    base = ROLES_BY_NAME["tester"].prompt
    assert augment_tester_prompt(base, OFF, TOK) == base
    enabled = augment_tester_prompt(base, ON, TOK)
    assert enabled.startswith(base.rstrip())
    assert "GitHub Actions CI" in enabled
    assert "VERDICT: FAIL" in enabled
