"""Interpret a tester agent's free-text report into a pass/fail verdict.

The naive approach — "does the word 'fail' appear?" — misreads common phrasings
like "no failures" or "0 errors" as failures. So we prefer an explicit machine
marker the tester is prompted to emit, and only fall back to counting when it's
absent.
"""

from __future__ import annotations

import re

# Explicit marker the tester is asked to end its report with. Kept deliberately
# lenient because agents wrap it in Markdown and add trailing prose in practice —
# e.g. "**Verdict: PASS** — full suite green". We match "VERDICT:" followed by
# optional emphasis/space and PASS/FAIL anywhere on the line, and take the last one.
_MARKER = re.compile(r"VERDICT:\s*[*_`]{0,2}\s*(PASS|FAIL)\b", re.IGNORECASE)

# "<n> failed" / "<n> errors" style counts, e.g. pytest's "2 failed, 5 passed".
_FAILED_COUNT = re.compile(r"\b(\d+)\s+(?:failed|failures?|errors?)\b", re.IGNORECASE)
_PASSED_COUNT = re.compile(r"\b(\d+)\s+passed\b", re.IGNORECASE)

# Whole-run success phrases when there are no numeric counts to lean on.
_ALL_PASS = re.compile(
    r"\ball (?:tests|checks) pass(?:ed)?\b|\b(?:tests|build) (?:are |is )?green\b"
    r"|\bno (?:failures?|errors?)\b|\bpassing\b",
    re.IGNORECASE,
)


def interpret_test_result(text: str) -> bool | None:
    """Return True (passed), False (failed), or None (can't tell) for a report.

    Precedence:
    1. The last explicit ``VERDICT: PASS|FAIL`` marker, if present.
    2. Numeric counts: any non-zero failed/error count -> False; else a positive
       passed count -> True.
    3. Whole-run success phrases -> True.
    4. Otherwise None, so the caller can treat it as "not yet green" and ask.
    """
    if not text:
        return None

    # 1. Explicit marker wins. Take the LAST one (the tester's final word).
    markers = _MARKER.findall(text)
    if markers:
        return markers[-1].upper() == "PASS"

    # 2. Numeric counts. A non-zero failure/error count is a failure, even if
    #    some tests also passed.
    failed = [int(n) for n in _FAILED_COUNT.findall(text)]
    if failed:
        if any(n > 0 for n in failed):
            return False
        # Only "0 failed" style counts seen -> that's a pass.
        return True
    if any(int(n) > 0 for n in _PASSED_COUNT.findall(text)):
        return True

    # 3. Plain-English success with no counts.
    if _ALL_PASS.search(text):
        return True

    # 4. Genuinely ambiguous.
    return None
