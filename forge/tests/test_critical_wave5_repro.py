"""Critical (wave 5), end to end: the Player methods film actually calls.

Not a simplified stand-in — a scratch copy of the real `shared/`, `music/`,
`film/`, and `scripts/check-exchange.mjs` from this repo, mutated exactly the
way the review reproduced the defect, then run through VERIFY's actual
production path (`run_checks_for_files`, no injected runner, no stub) — the
same function `orchestrate.live_run` calls on a real night.

Before this fix, `film/tests/film-logic.test.js` asserted a hand-written list
of members that stopped at `Engine.Player.prototype.load` — a second
hand-written list, added after the fourth review found that gap, still
stopped short of `play`, `seek`, `stop` and `pause`. All four are called
today, not "later": film/js/film-player.js:391,408,421,431 and
film/js/film-audio.js:322. Renaming any one of them in `music/js/engine.js`
left every one of the five exchange-published globals truthy and every
assertion in both hand-written lists green, while a real film threw
`TypeError: p.play is not a function` before its first frame and never
played at all — worse than the fourth review's finding, where the film
merely played silently.

`film/tests/film-logic.test.js` now derives the asserted surface from
film/js/*.js itself (`deriveFilmMusicSurface()`), so the four cases below —
one per renamed method — all fail VERIFY the same way the wave 4
reproduction above already covers for `Engine.Player` and `Synth` renamed
wholesale.
"""

from __future__ import annotations

import shutil
from pathlib import Path

import pytest

from forge.checks import all_commands
from forge.config import ForgeConfig
from forge.verify import run_checks_for_files

REPO_ROOT = Path(__file__).resolve().parents[2]


def _scratch_copy(tmp_path: Path) -> Path:
    """A scratch copy of just enough of the real repo to run the real
    `film/tests/film-logic.test.js`, `music/tests/music-logic.test.js` and
    `scripts/check-exchange.mjs` against: the real `shared/`, `music/`,
    `film/`, `madlibs/` and `scripts/` directories, copied straight off disk.

    `madlibs/` is here because film's idea box borrows a MADLIBS story, so
    film/index.html loads madlibs/js/*.js and shared/exchange.json declares it.
    check-exchange.mjs verifies every declared file exists, so leaving madlibs
    out makes it fail on the fixture for a reason that has nothing to do with
    the breakage the test is actually about.

    `zomboid/` and `coda-pics/` are here for the same reason, one manifest
    entry later: zomboid consumes music/soundtrack, and coda-pics publishes
    coda-pics/painter. The list below is not arbitrary — it is every project
    shared/exchange.json names — so adding a publisher or a consumer to that
    manifest means adding it here too, or every test using this fixture fails
    on eight missing-file complaints that have nothing to do with it.
    """
    root = tmp_path / "repo"
    for rel in ("shared", "music", "film", "madlibs", "zomboid", "coda-pics", "scripts"):
        shutil.copytree(REPO_ROOT / rel, root / rel)
    return root


def _rename_player_method(root: Path, method: str) -> None:
    engine_js = root / "music/js/engine.js"
    text = engine_js.read_text(encoding="utf-8")
    needle = f"Player.prototype.{method} = function"
    assert text.count(needle) == 1, (
        f"expected exactly one definition of Player.prototype.{method} in "
        f"engine.js, found {text.count(needle)} — the fixture no longer "
        f"matches engine.js's real shape"
    )
    engine_js.write_text(text.replace(needle, f"Player.prototype.{method}X = function"), encoding="utf-8")


@pytest.mark.parametrize("method", ["play", "seek", "stop", "pause"])
def test_renaming_a_player_playback_method_fails_verify(tmp_path, method):
    root = _scratch_copy(tmp_path)
    _rename_player_method(root, method)

    result = run_checks_for_files(("music/js/engine.js",), ForgeConfig(), root)

    assert result.ok is False, (
        f"VERIFY reported green for music/js/engine.js with Player#{method} renamed away — "
        f"film/js/film-player.js and film/js/film-audio.js both call it to run a film. "
        f"ran={result.ran!r} output={result.output!r}"
    )
    # A music change must still put film's tests in the verification set —
    # film consumes music/composer, and that chain is the point of this test.
    # It is asserted over all_commands rather than over result.ran because
    # run_checks_for_files stops at the FIRST failing command, and since
    # music/tests/soundtrack.test.js was added (the playback surface ZOMBOID
    # consumes) a broken Player is caught there, one step before film's suite
    # is reached. The break is still caught and verify still fails; a nearer
    # test simply reports it first, which is better rather than worse.
    assert any("film/tests/film-logic.test.js" in " ".join(cmd)
               for cmd in all_commands("music/", root))
    assert f"{method}" in result.output.lower()


def test_making_player_play_a_no_op_fails_verify(tmp_path):
    # Existence alone is not enough: `play` staying a function while doing
    # nothing is exactly what a hand-written "does this member exist" list
    # cannot see. film/tests/film-logic.test.js's end-to-end test drives
    # play() against a real (fake-audio-backed) Player and asserts
    # `player.playing` actually flips.
    root = _scratch_copy(tmp_path)
    engine_js = root / "music/js/engine.js"
    text = engine_js.read_text(encoding="utf-8")
    needle = "Player.prototype.play = function (fromBeat) {\n    if (!this.song) return;"
    assert text.count(needle) == 1
    engine_js.write_text(
        text.replace(needle, "Player.prototype.play = function (fromBeat) {\n    return;\n    if (!this.song) return;"),
        encoding="utf-8",
    )

    result = run_checks_for_files(("music/js/engine.js",), ForgeConfig(), root)

    assert result.ok is False, (
        f"VERIFY reported green for a Player#play that exists but does nothing. "
        f"ran={result.ran!r} output={result.output!r}"
    )
    assert "did not actually start playback" in result.output
