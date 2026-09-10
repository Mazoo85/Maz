"""Parsing docs/ROADMAP.md into candidates."""

from forge.signals.roadmap import collect, parse

SAMPLE = """# Maz Engine — Roadmap

> **Current milestone: M0 — walking skeleton.**

## Phase 0 — Foundation & tooling
- [x] CMake project, C++20, out-of-source build
- [ ] clang-tidy config + CI lint gate
- [~] CI matrix (Linux/Windows/macOS) — **Linux CI landed**
      Windows/macOS still to add.

## Phase 7 — Audio
- [ ] Mixer, buses, ducking
"""


def test_only_unchecked_items_become_candidates():
    cands = parse(SAMPLE)
    tasks = [c.task for c in cands]
    assert "clang-tidy config + CI lint gate" in tasks
    assert "Mixer, buses, ducking" in tasks
    assert not any("CMake project" in t for t in tasks)


def test_in_progress_items_are_candidates_too():
    tasks = [c.task for c in parse(SAMPLE)]
    assert any(t.startswith("CI matrix") for t in tasks)


def test_source_names_the_phase():
    by_task = {c.task: c for c in parse(SAMPLE)}
    assert by_task["clang-tidy config + CI lint gate"].source == "roadmap:phase-0"
    assert by_task["Mixer, buses, ducking"].source == "roadmap:phase-7"


def test_current_milestone_flag_follows_in_progress_phase():
    by_task = {c.task: c for c in parse(SAMPLE)}
    # Phase 0 carries a [~] marker, so it is the phase in progress.
    assert by_task["clang-tidy config + CI lint gate"].current_milestone is True
    assert by_task["Mixer, buses, ducking"].current_milestone is False


def test_markdown_emphasis_is_stripped_from_task_text():
    by_task = {c.task: c for c in parse(SAMPLE)}
    ci = next(c for c in by_task.values() if c.task.startswith("CI matrix"))
    assert "**" not in ci.task
    assert "`" not in ci.task


def test_kind_is_roadmap_and_paths_are_empty():
    c = parse(SAMPLE)[0]
    assert c.kind == "roadmap"
    # The roadmap names work, not files. DECIDE will skip anything it can't
    # place in a safe zone, which is the correct conservative default.
    assert c.paths == ()


def test_empty_document_yields_nothing():
    assert parse("") == []


def test_malformed_lines_are_skipped_not_fatal():
    assert parse("- [ ]\n- [ ] real item\n") == parse("- [ ] real item\n")


def test_collect_never_raises_on_invalid_utf8(tmp_path):
    # UnicodeDecodeError is a ValueError, not an OSError — a bare
    # `except OSError` around read_text would let it escape and break the
    # "collectors never raise" contract SENSE depends on.
    docs = tmp_path / "docs"
    docs.mkdir()
    (docs / "ROADMAP.md").write_bytes(b"## Phase 0 - x\n- [ ] \xff\xfe broken bytes\n")
    assert collect(tmp_path) == []


def test_collect_missing_file_yields_nothing(tmp_path):
    assert collect(tmp_path) == []
