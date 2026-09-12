"""Parsing docs/ROADMAP.md into candidates."""

from forge.signals.roadmap import MAX_PATH_CHARS, collect, parse

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


# ---------------------------------------------------------------------------
# Fences, the hostile cases. A parity toggle is not a fence matcher: anything
# this parser calls a fence but CommonMark does not (or the reverse) flips the
# state for the rest of the document, and the roadmap's own illustration
# becomes tonight's work.
# ---------------------------------------------------------------------------


def test_an_inline_code_span_at_line_start_is_not_a_fence():
    # A backtick fence's info string may not contain a backtick, so CommonMark
    # reads this as a paragraph. Reading it as an opening fence swallows every
    # real item after it — or, one stray line earlier, un-swallows an example.
    text = "## Phase 3\n```forge sense``` refreshes the pulse.\n- [ ] real work\n"
    assert [c.task for c in parse(text)] == ["real work"]


def test_a_longer_fence_may_contain_a_shorter_one():
    # The construct required to document the "fences are ignored" rule at all.
    text = (
        "## Phase 3\n"
        "````markdown\n"
        "```markdown\n"
        "- [ ] example\n"
        "```\n"
        "````\n"
        "- [ ] real work\n"
    )
    assert [c.task for c in parse(text)] == ["real work"]


def test_a_tilde_fence_is_not_closed_by_a_backtick_fence():
    text = "## Phase 3\n~~~\n- [ ] example\n```\n- [ ] still example\n~~~\n- [ ] real work\n"
    assert [c.task for c in parse(text)] == ["real work"]


def test_a_backtick_fence_is_not_closed_by_a_tilde_fence():
    text = "## Phase 3\n```\n- [ ] example\n~~~\n- [ ] still example\n```\n- [ ] real work\n"
    assert [c.task for c in parse(text)] == ["real work"]


def test_a_closing_fence_may_not_carry_an_info_string():
    # ```markdown inside a block is content, not the close.
    text = "## Phase 3\n```\n- [ ] example\n```markdown\n- [ ] still example\n```\n- [ ] real\n"
    assert [c.task for c in parse(text)] == ["real"]


def test_a_shorter_run_does_not_close_a_longer_fence():
    text = "## Phase 3\n````\n- [ ] example\n```\n- [ ] still example\n````\n- [ ] real\n"
    assert [c.task for c in parse(text)] == ["real"]


def test_tilde_fences_are_honoured_like_backtick_fences():
    # The `~~~` alternation must be load-bearing, not decoration.
    assert [c.task for c in parse("## Phase 3\n~~~\n- [ ] example\n~~~\n- [ ] real\n")] == [
        "real"
    ]


def test_the_roadmaps_own_illustration_survives_a_stray_fence_like_line():
    # The regression this whole section exists for, in miniature: an ordinary
    # prose line must not be able to reach inside a fenced example.
    text = (
        "## Phase 3\n"
        "```forge sense``` refreshes the pulse nightly.\n"
        "```markdown\n"
        "- [ ] Fix the broken recipe link in `scraper/README.md`\n"
        "```\n"
    )
    assert parse(text, exists=lambda p: True) == []


# ---------------------------------------------------------------------------
# Shape guards named in looks_like_path's contract, each pinned by a test.
# ---------------------------------------------------------------------------


def test_a_backslash_path_is_refused_even_if_it_resolves():
    text = "## Phase 3\n- [ ] Tidy `docs\\\\notes.md`\n"
    assert parse(text, exists=lambda p: True)[0].paths == ()


def test_a_token_containing_whitespace_is_refused():
    text = "## Phase 3\n- [ ] Tidy `docs/my notes.md`\n"
    assert parse(text, exists=lambda p: True)[0].paths == ()


def test_an_absurdly_long_token_is_refused():
    long = "docs/" + ("a" * MAX_PATH_CHARS) + ".md"
    text = f"## Phase 3\n- [ ] Tidy `{long}`\n"
    assert parse(text, exists=lambda p: True)[0].paths == ()


def test_a_symlink_pointing_out_of_the_tree_is_not_claimable(tmp_path):
    # A safe-zone name can be a symlink to anywhere. zones.py refuses to
    # resolve paths on purpose, so if extraction judges the written name
    # rather than the resolved target, Crew edits the target through the link
    # and the leash only notices afterwards.
    outside = tmp_path / "outside"
    outside.mkdir()
    (outside / "secret.py").write_text("x = 1\n", encoding="utf-8")
    tree = tmp_path / "tree"
    (tree / "docs").mkdir(parents=True)
    (tree / "music").mkdir()
    try:
        (tree / "music" / "player.js").symlink_to(outside / "secret.py")
    except (OSError, NotImplementedError):  # pragma: no cover - platform dependent
        import pytest

        pytest.skip("symlinks unavailable on this platform")
    (tree / "docs" / "ROADMAP.md").write_text(
        "## Phase 3\n- [ ] Tidy `music/player.js`\n", encoding="utf-8"
    )
    assert collect(tree)[0].paths == ()


def test_a_symlink_staying_inside_the_tree_is_still_claimable(tmp_path):
    # The containment check must not turn into "no symlinks at all" — a link
    # to a real file in the repo names a real file in the repo.
    tree = tmp_path / "tree"
    (tree / "docs").mkdir(parents=True)
    (tree / "music").mkdir()
    (tree / "music" / "real.js").write_text("// real\n", encoding="utf-8")
    try:
        (tree / "music" / "player.js").symlink_to(tree / "music" / "real.js")
    except (OSError, NotImplementedError):  # pragma: no cover - platform dependent
        import pytest

        pytest.skip("symlinks unavailable on this platform")
    (tree / "docs" / "ROADMAP.md").write_text(
        "## Phase 3\n- [ ] Tidy `music/player.js`\n", encoding="utf-8"
    )
    assert collect(tree)[0].paths == ("music/player.js",)


def test_a_predicate_that_explodes_does_not_break_the_night():
    # parse() promises it never raises. A predicate is caller-supplied code;
    # catching only OSError would let anything else escape and cost SENSE
    # every roadmap candidate, not just the one bad token.
    def boom(_path):
        raise RuntimeError("predicate exploded")

    cands = parse("## Phase 3\n- [ ] Tidy `docs/x.md`\n", exists=boom)
    assert [c.task for c in cands] == ["Tidy docs/x.md"]
    assert cands[0].paths == ()


# ---------------------------------------------------------------------------
# docs/FORGE-JOBS.md — the human's intake, read separately from the roadmap.
#
# It lived in the roadmap as a "Phase 14" until the repo's two trunks were
# unified: the merge kept the engine's roadmap and the intake section vanished,
# taking the one job in it and leaving FORGE.md pointing at a heading that no
# longer existed. A file nobody else edits cannot be lost that way.
# ---------------------------------------------------------------------------

from forge.signals.roadmap import JOBS_PATH, collect_jobs  # noqa: E402


def _jobs(root, text):
    (root / "docs").mkdir(parents=True, exist_ok=True)
    (root / JOBS_PATH).write_text(text, encoding="utf-8")
    return root


def test_a_job_needs_no_phase_heading(tmp_path):
    # The whole point of the separate file: it is a flat list, not a plan.
    (tmp_path / "music").mkdir()
    (tmp_path / "music" / "player.js").write_text("//\n", encoding="utf-8")
    _jobs(tmp_path, "- [ ] Add a volume slider to `music/player.js`\n")
    got = collect_jobs(tmp_path)
    assert [c.task for c in got] == ["Add a volume slider to music/player.js"]
    assert got[0].paths == ("music/player.js",)


def test_a_job_is_sourced_as_jobs_not_as_a_roadmap_phase(tmp_path):
    # The ledger has to show where a night's work was asked for.
    _jobs(tmp_path, "- [ ] Tidy something\n")
    assert collect_jobs(tmp_path)[0].source == "jobs"


def test_a_job_counts_as_the_current_milestone(tmp_path):
    # A job written by hand is the most deliberate signal there is, and scores
    # as current work rather than as something inferred from a backlog.
    _jobs(tmp_path, "- [ ] Tidy something\n")
    assert collect_jobs(tmp_path)[0].current_milestone is True


def test_a_ticked_job_is_not_picked_up(tmp_path):
    _jobs(tmp_path, "- [x] Already done\n- [ ] Still wanted\n")
    assert [c.task for c in collect_jobs(tmp_path)] == ["Still wanted"]


def test_an_example_in_a_fence_is_not_a_job(tmp_path):
    # The jobs file documents its own conventions, so it necessarily contains
    # worked examples. They must stay illustrations.
    _jobs(tmp_path, "```markdown\n- [ ] example\n```\n- [ ] real work\n")
    assert [c.task for c in collect_jobs(tmp_path)] == ["real work"]


def test_a_missing_jobs_file_is_not_an_error(tmp_path):
    assert collect_jobs(tmp_path) == []


def test_the_roadmap_still_ignores_items_above_the_first_phase(tmp_path):
    # The roadmap keeps its old behaviour: a checkbox before any heading is
    # prose. Only the jobs file treats such a line as work.
    docs = tmp_path / "docs"
    docs.mkdir(parents=True)
    (docs / "ROADMAP.md").write_text(
        "- [ ] loose line, not a task\n## Phase 3\n- [ ] real task\n", encoding="utf-8"
    )
    assert [c.task for c in collect(tmp_path)] == ["real task"]


def test_the_two_files_do_not_read_each_other(tmp_path):
    # Each collector reads exactly one file; a job in the roadmap is not a job,
    # and a roadmap phase in the jobs file does not become a roadmap source.
    docs = tmp_path / "docs"
    docs.mkdir(parents=True)
    (docs / "ROADMAP.md").write_text("## Phase 3\n- [ ] roadmap item\n", encoding="utf-8")
    (docs / "FORGE-JOBS.md").write_text("- [ ] job item\n", encoding="utf-8")
    assert [c.task for c in collect(tmp_path)] == ["roadmap item"]
    assert [c.task for c in collect_jobs(tmp_path)] == ["job item"]
