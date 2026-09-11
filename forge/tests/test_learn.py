"""LEARN: quarantine, roadmap ticks, memory notes."""

from forge.config import ForgeConfig
from forge.learn import memory_note, quarantine, tick_roadmap

ROADMAP = """## Phase 0 — Foundation & tooling
- [x] CMake project
- [ ] clang-tidy config + CI lint gate
- [ ] Semantic-version header, `CHANGELOG.md`
"""

# A real shape from docs/ROADMAP.md: an item whose bold emphasis opens on the
# matched line but closes on an indented continuation line the single-line
# item regex never sees. The parser's `_clean` leaves that lone "**" in
# place (it only collapses *matched* pairs) — so the task text SENSE actually
# stored still contains it. A second, different item earlier in the file
# happens to read identically once that "**" is (wrongly) stripped away.
ROADMAP_DANGLING_EMPHASIS = """## Phase 0 — Foundation
- [ ] Render pipeline refactor — partial rewrite
- [ ] Render pipeline refactor — **partial rewrite
      landed** later
"""


def test_quarantine_appends_a_readable_line(tmp_path):
    path = quarantine("abc123", "Fix the thing", "todo:a.js:1", tmp_path, ForgeConfig())
    text = path.read_text()
    assert "Fix the thing" in text
    assert "todo:a.js:1" in text


def test_quarantine_is_idempotent_per_key(tmp_path):
    cfg = ForgeConfig()
    quarantine("abc123", "Fix the thing", "todo:a.js:1", tmp_path, cfg)
    path = quarantine("abc123", "Fix the thing", "todo:a.js:1", tmp_path, cfg)
    assert path.read_text().count("abc123") == 1


def test_quarantine_never_raises_when_the_path_cannot_be_written(tmp_path):
    """quarantine is one of LEARN's three optional side effects: a failure to
    write stuck.md must not raise (see orchestrate.live_run, which depends on
    that to keep writing its ledger line). Blocking `forge/` with a plain
    file simulates the write failing for any OSError reason (full disk,
    permissions, a stray file where a directory belongs).
    """
    (tmp_path / "forge").write_text("not a directory", encoding="utf-8")
    path = quarantine("abc123", "Fix the thing", "todo:a.js:1", tmp_path, ForgeConfig())
    assert path == tmp_path / "forge" / "stuck.md"
    assert not path.exists()


def test_tick_roadmap_flips_an_exact_match(tmp_path):
    docs = tmp_path / "docs"
    docs.mkdir()
    (docs / "ROADMAP.md").write_text(ROADMAP)
    assert tick_roadmap("clang-tidy config + CI lint gate", tmp_path) is True
    assert "- [x] clang-tidy config + CI lint gate" in (docs / "ROADMAP.md").read_text()


def test_tick_roadmap_ignores_a_non_match(tmp_path):
    docs = tmp_path / "docs"
    docs.mkdir()
    (docs / "ROADMAP.md").write_text(ROADMAP)
    assert tick_roadmap("something else entirely", tmp_path) is False
    assert (docs / "ROADMAP.md").read_text() == ROADMAP


def test_tick_roadmap_handles_backticked_items(tmp_path):
    docs = tmp_path / "docs"
    docs.mkdir()
    (docs / "ROADMAP.md").write_text(ROADMAP)
    # The parser strips backticks, so LEARN must match the stripped form back.
    assert tick_roadmap("Semantic-version header, CHANGELOG.md", tmp_path) is True


def test_tick_roadmap_missing_file_is_not_fatal(tmp_path):
    assert tick_roadmap("anything", tmp_path) is False


def test_tick_roadmap_matches_the_real_task_not_a_stripped_lookalike(tmp_path):
    """A hand-rolled `.replace("**", "")` stripper removes a marker `_clean`
    would leave in place (see ROADMAP_DANGLING_EMPHASIS above), so it
    normalises two genuinely different tasks down to the same text and ticks
    the wrong (earlier) line. Matching must re-derive "cleaned" with the
    parser's own `_clean`, which never loses that distinguishing "**".
    """
    docs = tmp_path / "docs"
    docs.mkdir()
    (docs / "ROADMAP.md").write_text(ROADMAP_DANGLING_EMPHASIS)
    task = "Render pipeline refactor — **partial rewrite"  # as SENSE actually stored it
    assert tick_roadmap(task, tmp_path) is True
    lines = (docs / "ROADMAP.md").read_text().splitlines()
    assert lines[1] == "- [ ] Render pipeline refactor — partial rewrite"
    assert lines[2] == "- [x] Render pipeline refactor — **partial rewrite"


def test_memory_note_appends_an_entity_line(tmp_path):
    claude = tmp_path / ".claude"
    claude.mkdir()
    (claude / "codebase-memory.json").write_text("")
    entry = {"run_id": "2026-09-11", "chose": "Write the docs", "outcome": "pr_opened", "pr": 7}
    assert memory_note(entry, tmp_path) is True
    text = (claude / "codebase-memory.json").read_text()
    assert '"type": "entity"' in text or '"type":"entity"' in text
    assert "Write the docs" in text


def test_memory_note_missing_file_is_not_fatal(tmp_path):
    assert memory_note({"run_id": "x", "chose": "y", "outcome": "no_task"}, tmp_path) is False
