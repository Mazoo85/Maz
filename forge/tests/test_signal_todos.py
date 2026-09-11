"""Scanning tracked source for TODO / FIXME / HACK markers."""

from pathlib import Path

from forge.signals.todos import collect, scan_text


def test_finds_todo_with_line_number():
    text = "let a = 1;\n// TODO: clamp the camera to the map bounds\nlet b = 2;\n"
    cands = scan_text("js/game.js", text, recent=True)
    assert len(cands) == 1
    assert cands[0].source == "todo:js/game.js:2"
    assert "clamp the camera" in cands[0].task
    assert cands[0].paths == ("js/game.js",)


def test_recognises_fixme_and_hack():
    text = "# FIXME: retry on 429\n# HACK: sleep to dodge the race\n"
    assert len(scan_text("scraper/scraper/http.py", text, recent=False)) == 2


def test_recent_flag_is_carried_in_detail():
    hot = scan_text("music/js/a.js", "// TODO: x\n", recent=True)[0]
    cold = scan_text("music/js/a.js", "// TODO: x\n", recent=False)[0]
    assert hot.detail == "recent"
    assert cold.detail == ""


def test_ignores_the_word_todo_in_prose():
    # A bare mention of the marker word — even capitalised, matching \bTODO\b —
    # is not a marker unless it is followed by a colon/dash and then text. A
    # lowercase "todo" would be excluded by case alone and prove nothing about
    # that rule, so this uses the uppercase spelling with no separator.
    text = "The TODO list is long.\n"
    assert scan_text("docs/x.md", text, recent=True) == []


def test_marker_with_no_text_is_skipped():
    assert scan_text("js/a.js", "// TODO:\n", recent=True) == []


def test_collect_uses_injected_runner(tmp_path):
    (tmp_path / "js").mkdir()
    (tmp_path / "js" / "game.js").write_text("// TODO: fix the thing\n")
    (tmp_path / "notes.bin").write_bytes(b"\x00\x01TODO: binary\n")

    def runner(args):
        if args[:2] == ["ls-files", "-z"]:
            return "js/game.js\x00notes.bin\x00"
        if args[0] == "log":
            return "js/game.js\n"
        return ""

    cands = collect(tmp_path, runner=runner)
    assert len(cands) == 1
    assert cands[0].paths == ("js/game.js",)
    assert cands[0].detail == "recent"  # git log listed it


def test_collect_survives_a_failing_runner(tmp_path):
    def runner(args):
        raise OSError("git not found")

    assert collect(tmp_path, runner=runner) == []


def test_closing_comment_with_empty_body_is_dropped():
    # rstrip("*/") strips a trailing run of '*' and '/' characters, not the
    # two-character suffix "*/". A closing-comment line like /* TODO: */ or
    # // TODO: */ reduces to an empty body after rstrip("*/"), and the
    # if not body: continue guard prevents emitting a contentless candidate.
    # This regression test pins that guard — without it, such lines emit
    # candidates with no task text, a garbage shape.
    assert scan_text("file.c", "/* TODO: */", recent=True) == []
    assert scan_text("file.js", "// TODO: */", recent=True) == []

    # A marker whose text legitimately ends in * or / must still be kept.
    # The trailing * is a cosmetic strip by rstrip, not a sign of an empty body.
    cands = scan_text("file.py", "# TODO: handle a/b*\n", recent=True)
    assert len(cands) == 1
    assert "handle a/b" in cands[0].task


def _collect_over(tmp_path: Path, files: dict[str, str]):
    """Write `files` under tmp_path and run collect() with a matching fake runner."""
    for rel, content in files.items():
        p = tmp_path / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(content)

    def runner(args):
        if args[:2] == ["ls-files", "-z"]:
            return "".join(f"{p}\x00" for p in files)
        if args and args[0] == "log":
            return ""
        return ""

    return collect(tmp_path, runner=runner)


def test_collect_skips_forge_github_docs_superpowers_and_claude(tmp_path):
    files = {
        "forge/anything.py": "# TODO: fix the scanner\n",
        ".github/workflows/x.yml": "# TODO: pin the action version\n",
        "docs/superpowers/plans/p.md": "TODO: illustrative example only\n",
        ".claude/skills/s.md": "TODO: illustrative skill fixture\n",
        "music/js/a.js": "// TODO: real outstanding work\n",
        "docs/ARCHITECTURE.md": "TODO: document the real architecture\n",
    }
    cands = _collect_over(tmp_path, files)
    paths = {c.paths[0] for c in cands}
    assert paths == {"music/js/a.js", "docs/ARCHITECTURE.md"}


def test_skip_dirs_are_segment_aware_not_substring(tmp_path):
    files = {
        "sub/build/x.js": "// TODO: nested build dir must be skipped\n",
        "pkg/node_modules/y.js": "// TODO: nested node_modules must be skipped\n",
        "builds/x.js": "// TODO: builds is not build, keep it\n",
        "node_modules_old/y.js": "// TODO: not node_modules, keep it\n",
    }
    cands = _collect_over(tmp_path, files)
    paths = {c.paths[0] for c in cands}
    assert paths == {"builds/x.js", "node_modules_old/y.js"}
