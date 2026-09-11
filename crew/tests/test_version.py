"""`crew --version` prints the package version and exits cleanly."""

from typer.testing import CliRunner

from crew import __version__
from crew.cli import app

runner = CliRunner()


def test_version_flag():
    result = runner.invoke(app, ["--version"])
    assert result.exit_code == 0
    assert __version__ in result.stdout
    assert "crew" in result.stdout


def test_help_still_lists_commands():
    result = runner.invoke(app, ["--help"])
    assert result.exit_code == 0
    for cmd in ("do", "resume", "status", "runs", "show", "config", "init", "agents"):
        assert cmd in result.stdout
