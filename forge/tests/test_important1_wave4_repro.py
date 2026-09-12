"""Important 1 (wave 4), end to end: the two reproductions from the review.

Not a simplified stand-in — a scratch copy of the real `shared/`, `music/`,
`film/`, and `scripts/check-exchange.mjs` from this repo, mutated exactly the
way the review reproduced the defect, then run through VERIFY's actual
production path (`run_checks_for_files`, no injected runner, no stub) —
the same function `orchestrate.live_run` calls on a real night.

Before this fix, `film/tests/film-logic.test.js` only asserted the five
`shared/exchange.json`-published globals (`Theory`, `Genres`, `Synth`,
`Composer`, `Engine`) were *truthy* after loading — never that the specific
members `film/js/*.js` actually calls on them still existed with the right
shape. Both mutations below leave every one of those five globals truthy,
so `checks: green` would have been recorded, and a draft PR opened, over a
change that leaves `film`'s own audio silently broken:

  * Renaming `music/js/engine.js`'s `Engine.Player` export to
    `Engine.Sequencer` leaves `Engine` itself a truthy object.
    `film/js/film-audio.js:250`'s `new Play.Player({...})` then throws
    `TypeError: Play.Player is not a constructor` — caught by
    `startScore()`'s own `try/catch`, so the film plays with no score and
    no visible error.

  * Gutting `music/js/synth.js` to `window.Synth = {}` leaves `Synth`
    truthy. `music/js/engine.js`'s mixer graph (`Synth.softClipCurve`,
    `Synth.reverbImpulse`, `Synth.vinylBuffer`) and its scheduler
    (`Synth.playDrum`, `Synth.playNote`) all call into functions that no
    longer exist, once the film actually tries to play a note.
"""

from __future__ import annotations

import shutil
from pathlib import Path

import pytest

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
    """
    root = tmp_path / "repo"
    for rel in ("shared", "music", "film", "madlibs", "scripts"):
        shutil.copytree(REPO_ROOT / rel, root / rel)
    return root


@pytest.mark.parametrize(
    "mutate, files, needle",
    [
        pytest.param(
            lambda root: (root / "music/js/engine.js").write_text(
                (root / "music/js/engine.js").read_text(encoding="utf-8")
                .replace("    Player: Player,", "    Sequencer: Player,"),
                encoding="utf-8",
            ),
            ("music/js/engine.js",),
            "Engine.Player is not a constructor",
            id="engine-player-renamed-to-sequencer",
        ),
        pytest.param(
            lambda root: (root / "music/js/synth.js").write_text(
                "window.Synth = {};\n", encoding="utf-8"
            ),
            ("music/js/synth.js",),
            "Synth.softClipCurve is not a function",
            id="synth-gutted-to-empty-object",
        ),
    ],
)
def test_a_parseable_published_file_change_that_breaks_film_fails_verify(
    tmp_path, mutate, files, needle
):
    root = _scratch_copy(tmp_path)
    mutate(root)

    config = ForgeConfig()
    result = run_checks_for_files(files, config, root)

    assert result.ok is False, (
        "VERIFY reported green for a change that leaves an exchange-published "
        f"global truthy but one of its members renamed or missing. "
        f"ran={result.ran!r} output={result.output!r}"
    )
    assert "node film/tests/film-logic.test.js" in result.ran
    assert needle in result.output
