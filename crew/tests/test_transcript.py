"""Transcript formatting and writing."""

from crew.transcript import format_transcript, write_transcript


def test_format_includes_task_and_phases():
    out = format_transcript(
        "add a healthcheck",
        [("1/4  PLAN", "the plan text"), ("2/4  CODE", "changed foo.py")],
    )
    assert out.startswith("# Crew run")
    assert "**Task:** add a healthcheck" in out
    assert "## 1/4  PLAN" in out and "the plan text" in out
    assert "## 2/4  CODE" in out and "changed foo.py" in out


def test_empty_phase_text_marked():
    out = format_transcript("t", [("1/4  PLAN", "   ")])
    assert "_(no output)_" in out


def test_write_creates_file(tmp_path):
    runs = tmp_path / "runs"
    path = write_transcript(runs, "sess-1", "# hi\n")
    assert path == runs / "sess-1.md"
    assert path.read_text() == "# hi\n"


def test_write_sanitizes_name(tmp_path):
    path = write_transcript(tmp_path, "a/b:c*d", "x")
    assert path.name == "a_b_c_d.md"


def test_write_falls_back_on_empty_name(tmp_path):
    path = write_transcript(tmp_path, "", "x")
    assert path.name == "run.md"


def test_list_runs_empty(tmp_path):
    from crew.transcript import list_runs

    assert list_runs(tmp_path / "nope") == []


def test_list_runs_parses_tasks(tmp_path):
    from crew.transcript import list_runs

    runs = tmp_path / "runs"
    write_transcript(runs, "a", format_transcript("first task", [("1/4  PLAN", "x")]))
    write_transcript(runs, "b", format_transcript("second task", [("1/4  PLAN", "y")]))

    entries = list_runs(runs)
    tasks = {task for _, task in entries}
    assert tasks == {"first task", "second task"}
    assert all(p.suffix == ".md" for p, _ in entries)


def test_parse_task_unknown_when_missing(tmp_path):
    from crew.transcript import parse_task

    p = tmp_path / "x.md"
    p.write_text("# Crew run\n\nno task line here\n")
    assert parse_task(p) == "(unknown)"
