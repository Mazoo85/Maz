"""Important 2, end to end: the reproduction from the review, reproduced.

Not a simplified stand-in — a scratch copy of the *real* `shared/`,
`music/js/`, `film/`, and `scripts/check-exchange.mjs` from this repo, with
a real `docs/demo.html` (an ordinary roadmap task) added on top containing
two undeclared cross-project `<script>` references. Runs the real
`node scripts/check-exchange.mjs` through VERIFY's actual production path
(`run_checks_for_files`, no injected runner) — no fake, no stub.

Before the fix: `all_commands` only appended `EXCHANGE_CHECK_CMD` for a
zone with a project, so `docs/` never ran it. VERIFY returned
`ok=True, ran=()` and the ledger would have recorded `checks: green` on a
change that `node scripts/check-exchange.mjs` fails outright.
"""

from __future__ import annotations

from pathlib import Path

from conftest import scratch_repo_copy
from forge.config import ForgeConfig
from forge.verify import run_checks_for_files

REPO_ROOT = Path(__file__).resolve().parents[2]

DEMO_HTML = (
    "<!doctype html><html><head></head><body>\n"
    '<script src="../music/js/theory.js"></script>\n'
    '<script src="../music/js/composer.js"></script>\n'
    "</body></html>\n"
)


def _scratch_copy(tmp_path: Path) -> Path:
    """A scratch copy of just enough of the real repo to run the real
    check-exchange.mjs against, copied straight off disk rather than
    reconstructed by hand, so this exercises the real declaration and the real
    consumer page.

    "Just enough" is whatever `shared/exchange.json` refers to (see
    `tests/conftest.py`) — the checker verifies every declared file exists, so
    anything the manifest names and the fixture omits shows up as a missing-file
    complaint mixed into the output this test reads.
    """
    return scratch_repo_copy(tmp_path)


def test_docs_only_demo_html_with_undeclared_script_tags_fails_verify(tmp_path):
    root = _scratch_copy(tmp_path)
    (root / "docs").mkdir()
    demo = root / "docs" / "demo.html"
    demo.write_text(DEMO_HTML, encoding="utf-8")

    try:
        config = ForgeConfig()
        result = run_checks_for_files(("docs/demo.html",), config, root)

        assert result.ok is False, (
            "VERIFY reported green for a docs/-only change that leaves two "
            f"undeclared cross-project <script> tags. ran={result.ran!r} "
            f"output={result.output!r}"
        )
        assert "node scripts/check-exchange.mjs" in result.ran
        assert "demo.html" in result.output
    finally:
        # Do not leave the scratch file anywhere it could be mistaken for
        # part of the real repo — tmp_path is discarded by pytest anyway,
        # but the demo file itself is removed explicitly for clarity.
        if demo.exists():
            demo.unlink()
