"""The verdict parser is the fix for the old brittle substring check."""

import pytest

from crew.verdict import interpret_test_result as f


@pytest.mark.parametrize(
    "text, expected",
    [
        # Explicit marker is authoritative.
        ("12 passed, 0 failed\nVERDICT: PASS", True),
        ("VERDICT: FAIL", False),
        ("verdict: pass", True),  # case-insensitive
        # Marker wins even when the prose looks the other way...
        ("2 failed, 3 errors\n\nVERDICT: PASS", True),
        # ...and the LAST marker is the tester's final word.
        ("VERDICT: FAIL\nwait, re-ran\nVERDICT: PASS", True),
        # These are exactly what broke the old heuristic:
        ("no failures found", True),
        ("Ran 8 tests, 0 errors", True),
        ("0 failed", True),
        # Real failures.
        ("2 failed, 5 passed", False),
        ("1 error during collection", False),
        # Plain-English success without counts.
        ("all tests passed", True),
        ("build is green", True),
        # Genuinely ambiguous / empty -> unknown.
        ("hmm, something happened but I'm not sure", None),
        ("", None),
        (None, None),
    ],
)
def test_interpret(text, expected):
    assert f(text) is expected
