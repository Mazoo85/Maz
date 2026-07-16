"""The package exposes a version, and the CLI reports it."""

from typer.testing import CliRunner

from scraper import __version__
from scraper.cli import app


def test_package_version():
    assert __version__ == "0.1.0"


def test_cli_version_flag():
    result = CliRunner().invoke(app, ["--version"])
    assert result.exit_code == 0
    assert "scrape" in result.stdout
    assert __version__ in result.stdout
