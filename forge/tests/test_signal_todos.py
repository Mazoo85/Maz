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
