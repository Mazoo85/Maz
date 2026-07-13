"""`crew agents` output is derived from ROLES, so it can't drift from reality."""

from typer.testing import CliRunner

from crew.agents import ROLES
from crew.cli import app

runner = CliRunner()


def test_agents_lists_every_role_and_its_tools():
    result = runner.invoke(app, ["agents"])
    assert result.exit_code == 0
    out = result.stdout
    for role in ROLES:
        assert role.name in out
        # Every tool the role is actually granted shows up in the listing.
        for tool in role.tools:
            assert tool in out
