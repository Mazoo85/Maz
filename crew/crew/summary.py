"""Render a per-phase run summary — pure string formatting, easy to test."""

from __future__ import annotations


def _cost(value: float | None) -> str:
    return f"${value:.4f}" if value else "—"


def format_run_summary(records: list[tuple[str, float | None]], total_cost: float = 0.0) -> str:
    """A plain-text table of each phase that ran and its cost, plus the total.

    ``records`` is a list of (phase title, cost) in the order the phases ran.
    """
    lines = ["Run summary:"]
    for title, cost in records:
        lines.append(f"  {title:<30} {_cost(cost)}")
    lines.append(f"  {'Total':<30} {_cost(total_cost)}")
    return "\n".join(lines)
