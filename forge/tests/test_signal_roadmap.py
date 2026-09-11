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
    # A malformed line (only whitespace after the checkbox) should be skipped,
    # but valid items before and after should be collected.
    result = parse("## Phase 0\n- [ ] first task\n- [ ]  \n- [ ] second task\n")
    tasks = [c.task for c in result]
    assert tasks == ["first task", "second task"]


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


# ---------------------------------------------------------------------------
# Naming files in a roadmap item — the human's steering wheel.
# ---------------------------------------------------------------------------

PATHS_SAMPLE = """## Phase 3 — Arcade
- [ ] Add a volume slider to `music/player.js`
- [ ] Rework `scraper/fetch.py` and `scraper/parse.py` together
- [ ] Dependency management via `FetchContent`, `find_package(Vulkan)`
- [ ] Tidy up `docs/../forge/config.py`
- [ ] Rewrite `/etc/passwd`
- [ ] Document the `music/player.js` volume slider in `music/player.js`
"""


def _real(*existing):
    """A predicate standing in for 'this path is a real file in the repo'."""
    return lambda p: p in set(existing)


def test_a_backticked_real_file_becomes_the_candidates_path():
    by_task = {c.task: c for c in parse(PATHS_SAMPLE, exists=_real("music/player.js"))}
    c = by_task["Add a volume slider to music/player.js"]
    assert c.paths == ("music/player.js",)


def test_several_named_files_are_all_carried():
    cands = parse(
        PATHS_SAMPLE, exists=_real("scraper/fetch.py", "scraper/parse.py")
    )
    c = next(c for c in cands if c.task.startswith("Rework"))
    assert c.paths == ("scraper/fetch.py", "scraper/parse.py")


def test_backticks_that_are_not_files_are_not_paths():
    # `FetchContent` and `find_package(Vulkan)` are prose, not files. Nothing
    # exists, so nothing is claimed — the item stays pathless and DECIDE skips it.
    cands = parse(PATHS_SAMPLE, exists=_real())
    assert all(c.paths == () for c in cands)


def test_a_name_that_does_not_exist_is_never_claimed():
    # The predicate says only music/player.js is real, so the scraper item —
    # whose backticks are well-formed paths — must still come back empty.
    cands = parse(PATHS_SAMPLE, exists=_real("music/player.js"))
    c = next(c for c in cands if c.task.startswith("Rework"))
    assert c.paths == ()


def test_traversal_and_absolute_paths_are_refused_even_if_they_resolve():
    # A predicate that says "yes" to everything is the hostile case: extraction
    # itself must reject `..` and absolute paths, rather than leaning on the
    # zone check downstream to catch them.
    cands = parse(PATHS_SAMPLE, exists=lambda p: True)
    by_task = {c.task: c for c in cands}
    assert by_task["Tidy up docs/../forge/config.py"].paths == ()
    assert by_task["Rewrite /etc/passwd"].paths == ()


def test_a_file_named_twice_is_carried_once():
    c = next(
        c
        for c in parse(PATHS_SAMPLE, exists=_real("music/player.js"))
        if c.task.startswith("Document")
    )
    assert c.paths == ("music/player.js",)


def test_paths_are_empty_when_no_predicate_is_given():
    # parse() with no way to check the repo must not guess. Every existing
    # caller that passes only text keeps the old, pathless behaviour.
    assert all(c.paths == () for c in parse(PATHS_SAMPLE))


def test_collect_reads_paths_from_the_real_tree(tmp_path):
    docs = tmp_path / "docs"
    docs.mkdir()
    (tmp_path / "music").mkdir()
    (tmp_path / "music" / "player.js").write_text("// player\n", encoding="utf-8")
    (docs / "ROADMAP.md").write_text(
        "## Phase 3\n"
        "- [ ] Add a volume slider to `music/player.js`\n"
        "- [ ] Add a volume slider to `music/missing.js`\n",
        encoding="utf-8",
    )
    by_task = {c.task: c for c in collect(tmp_path)}
    assert by_task["Add a volume slider to music/player.js"].paths == ("music/player.js",)
    assert by_task["Add a volume slider to music/missing.js"].paths == ()


def test_collect_does_not_claim_a_directory_as_a_file(tmp_path):
    # `music/` exists but is not a file. Handing a directory to the leash as
    # though it were an edit target would be a lie about what the work touches.
    docs = tmp_path / "docs"
    docs.mkdir()
    (tmp_path / "music").mkdir()
    (docs / "ROADMAP.md").write_text(
        "## Phase 3\n- [ ] Tidy up `music`\n", encoding="utf-8"
    )
    assert collect(tmp_path)[0].paths == ()


def test_items_inside_a_code_fence_are_examples_not_work():
    # A roadmap that documents its own conventions will contain a worked
    # example. Without fence awareness that example becomes a real candidate —
    # the same way illustrative TODO: fixtures in docs/superpowers were once
    # picked up as genuine work by a live run.
    text = (
        "## Phase 3\n"
        "- [ ] real work\n"
        "```\n"
        "- [ ] example work\n"
        "```\n"
        "- [ ] more real work\n"
    )
    assert [c.task for c in parse(text)] == ["real work", "more real work"]


def test_a_fence_with_a_language_tag_still_closes():
    text = "## Phase 3\n```markdown\n- [ ] example\n```\n- [ ] real\n"
    assert [c.task for c in parse(text)] == ["real"]


def test_an_unclosed_fence_swallows_the_rest_rather_than_guessing():
    # Better to lose candidates than to invent them: an unterminated fence is
    # a malformed document, and treating its contents as work is the failure
    # that actually costs something.
    text = "## Phase 3\n- [ ] real\n```\n- [ ] example\n"
    assert [c.task for c in parse(text)] == ["real"]
