"""The per-phase run summary formatter."""

from crew.summary import format_run_summary


def test_lists_phases_and_total():
    out = format_run_summary(
        [("1/4  PLAN", 0.01), ("2/4  CODE", 0.02)],
        total_cost=0.03,
    )
    assert "Run summary:" in out
    assert "1/4  PLAN" in out and "$0.0100" in out
    assert "2/4  CODE" in out and "$0.0200" in out
    assert "Total" in out and "$0.0300" in out


def test_missing_costs_render_dash():
    out = format_run_summary([("1/4  PLAN", None)], total_cost=0.0)
    assert "1/4  PLAN" in out
    assert "—" in out  # no cost for the phase and no total


def test_empty_records_still_has_total_row():
    out = format_run_summary([], total_cost=0.0)
    assert out.splitlines()[0] == "Run summary:"
    assert "Total" in out
