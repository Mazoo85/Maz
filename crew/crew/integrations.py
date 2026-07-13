"""Optional external integrations for the crew — currently GitHub Actions CI.

When enabled, the tester agent gets access to a GitHub MCP server so it can read
the project's real CI logs and fold them into its verdict (a red CI = FAIL), on
top of running tests locally. Everything here is opt-in and off by default.

The decision logic is kept pure (given a config + an env mapping) so it can be
unit-tested offline, without the SDK, a token, or network.
"""

from __future__ import annotations

import os
from typing import Mapping

from .config import CrewConfig

# Wildcard grant for the GitHub MCP tools. A wildcard avoids hard-coding tool
# names that vary by server version; session-level allowed_tools accepts this
# pattern. (Whether per-agent AgentDefinition.tools accepts it is verified live;
# if not, list explicit names here — nothing else changes.)
GITHUB_TESTER_TOOLS = ["mcp__github__*"]

# The npx reference server. Documented as swappable for another GitHub MCP server.
_GITHUB_MCP_COMMAND = "npx"
_GITHUB_MCP_ARGS = ["-y", "@modelcontextprotocol/server-github"]

TESTER_CI_ADDENDUM = """

This project uses GitHub Actions CI, and you have GitHub tools available. In
addition to running the tests locally:
- Find the latest workflow run for the current branch and read the logs of any
  failing jobs.
- Quote the key CI failure(s) alongside your local results.
- Treat a red CI as a failure: if CI is failing, the verdict is VERDICT: FAIL
  even when local tests pass (say why). Only report VERDICT: PASS when both local
  tests and CI are green.
"""


def _env(env: Mapping[str, str] | None) -> Mapping[str, str]:
    return os.environ if env is None else env


def github_token(env: Mapping[str, str] | None = None) -> str | None:
    """Return the first available GitHub token, or None."""
    e = _env(env)
    for key in ("GITHUB_TOKEN", "GITHUB_PERSONAL_ACCESS_TOKEN"):
        value = e.get(key)
        if value:
            return value
    return None


def github_ci_enabled(config: CrewConfig, env: Mapping[str, str] | None = None) -> bool:
    """CI integration is active only when opted in AND a token is present."""
    return bool(getattr(config, "github_ci", False)) and github_token(env) is not None


def github_mcp_servers(env: Mapping[str, str] | None = None) -> dict:
    """The SDK ``mcp_servers`` spec for the GitHub server (assumes a token exists)."""
    token = github_token(env) or ""
    return {
        "github": {
            "command": _GITHUB_MCP_COMMAND,
            "args": list(_GITHUB_MCP_ARGS),
            "env": {"GITHUB_PERSONAL_ACCESS_TOKEN": token},
        }
    }


def augment_tester_tools(
    base_tools: list[str], config: CrewConfig, env: Mapping[str, str] | None = None
) -> list[str]:
    """Add the GitHub tools to the tester's tool list when CI is enabled."""
    if not github_ci_enabled(config, env):
        return list(base_tools)
    return list(base_tools) + [t for t in GITHUB_TESTER_TOOLS if t not in base_tools]


def augment_tester_prompt(
    base_prompt: str, config: CrewConfig, env: Mapping[str, str] | None = None
) -> str:
    """Append the CI addendum to the tester's prompt when CI is enabled."""
    if not github_ci_enabled(config, env):
        return base_prompt
    return base_prompt.rstrip() + "\n" + TESTER_CI_ADDENDUM
