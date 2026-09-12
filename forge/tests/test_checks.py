"""What "green" covers: a zone's own checks, plus everything downstream."""

import json

import pytest

from forge.checks import (
    EXCHANGE_CHECK_CMD,
    PROJECT_CHECKS,
    ZONE_PROJECT,
    UnmappedConsumerError,
    UnmappedZoneError,
    all_commands,
    commands_for,
)
from forge.exchange import ExchangeError

GOOD = {
    "publishes": {
        "music/composer": {
            "project": "music",
            "summary": "Compose a song.",
            "files": ["music/js/composer.js"],
        }
    },
    "consumes": [
        {
            "project": "film",
            "id": "music/composer",
            "via": "script",
            "page": "film/index.html",
            "contract": "film/tests/film-logic.test.js",
        }
    ],
}


def _repo(root, data=GOOD):
    shared = root / "shared"
    shared.mkdir(parents=True, exist_ok=True)
    text = data if isinstance(data, str) else json.dumps(data)
    (shared / "exchange.json").write_text(text, encoding="utf-8")
    return root


def test_commands_for_is_unchanged_for_known_zones_and_takes_no_root():
    # The pure, own-zone map, for zones ZONE_PROJECT actually knows about.
    assert commands_for("music/") == (("node", "music/tests/music-logic.test.js"),)
    assert commands_for("docs/") == ()


# --- Important 1: an unmapped-but-safe zone must fail closed, not silently -
# --- verify nothing (ZONE_PROJECT.get(zone) collapsing "never classified" --
# --- into the same None as docs/'s deliberate "no project"). ---------------


def test_commands_for_raises_for_a_zone_with_no_zone_project_entry():
    # "nowhere/" and "tests/" are not in safe_zones and so are never asked
    # in production, but the fail-closed contract must not depend on that:
    # nobody has told ZONE_PROJECT what either of them is.
    assert "nowhere/" not in ZONE_PROJECT
    assert "tests/" not in ZONE_PROJECT
    with pytest.raises(UnmappedZoneError):
        commands_for("nowhere/")
    with pytest.raises(UnmappedZoneError):
        commands_for("tests/")


def test_all_commands_raises_for_a_zone_with_no_zone_project_entry(tmp_path):
    with pytest.raises(UnmappedZoneError):
        all_commands("nowhere/", _repo(tmp_path))


def test_docs_widening_rollout_advice_literally_still_fails_closed(tmp_path, monkeypatch):
    # Reproduction 1 from the review: follow docs/FORGE.md's *old* Rollout
    # advice literally — add a project to PROJECT_CHECKS and its directory
    # to safe_zones — without also adding it to ZONE_PROJECT, exactly the
    # step that advice never named. Before this fix, all_commands('zomboid/')
    # returned () and the night recorded checks: green having run nothing.
    monkeypatch.setitem(
        PROJECT_CHECKS, "zomboid", (("node", "zomboid/tests/zomboid-logic.test.js"),)
    )
    assert "zomboid/" not in ZONE_PROJECT
    with pytest.raises(UnmappedZoneError):
        all_commands("zomboid/", _repo(tmp_path))


def test_forge_json_typo_in_safe_zones_fails_closed_via_run_checks_for_files(tmp_path):
    # Reproduction 2 from the review: a plain typo in forge.json — "music"
    # instead of "music/" — used to turn music's entire check set into ()
    # while still reporting green. Exercised through the real production
    # path (run_checks_for_files + zones_for_files), not commands_for
    # directly, because that is the path a typo'd forge.json actually hits.
    from forge.config import ForgeConfig
    from forge.verify import run_checks_for_files

    _repo(tmp_path)
    (tmp_path / "music").mkdir(parents=True, exist_ok=True)
    (tmp_path / "music" / "notes.md").write_text("x\n", encoding="utf-8")

    config = ForgeConfig(safe_zones=("music",))  # typo: no trailing slash
    result = run_checks_for_files(("music/notes.md",), config, tmp_path,
                                  runner=lambda cmd, root: (0, "ok"))
    assert result.ok is False
    assert result.ran == ()
    assert "music" in result.output


def test_music_gains_films_tests_because_film_consumes_music(tmp_path):
    cmds = all_commands("music/", _repo(tmp_path))
    assert ("node", "music/tests/music-logic.test.js") in cmds
    assert ("node", "film/tests/film-logic.test.js") in cmds


def test_the_zones_own_checks_come_first(tmp_path):
    cmds = all_commands("music/", _repo(tmp_path))
    own = cmds.index(("node", "music/tests/music-logic.test.js"))
    downstream = cmds.index(("node", "film/tests/film-logic.test.js"))
    assert own < downstream


def test_a_zone_with_no_consumers_is_unchanged_besides_the_exchange_gate(tmp_path):
    # The exchange gate (EXCHANGE_CHECK_CMD) is added to every project-owning
    # zone regardless of consumers — see all_commands's own docstring — so
    # "unchanged" here means "no consumer-specific commands added", not
    # byte-for-byte identical to commands_for.
    cmds = all_commands("crew/tests/", _repo(tmp_path))
    assert set(cmds) - set(commands_for("crew/tests/")) == {EXCHANGE_CHECK_CMD}


def test_a_zone_with_no_checks_of_its_own_still_runs_the_exchange_gate(tmp_path):
    # docs/ has no project and so no own-zone commands, but Important 2
    # means it is never truly empty: the whole-repo exchange gate always
    # runs, regardless of zone.
    assert all_commands("docs/", _repo(tmp_path)) == (EXCHANGE_CHECK_CMD,)


def test_a_consumer_with_no_known_checks_fails_closed(tmp_path):
    # Minor 2 from the third review: zomboid consumes music but has no entry
    # in PROJECT_CHECKS. This used to silently contribute zero commands for
    # it (`PROJECT_CHECKS.get(consumer, ())`) — the exact shape of the
    # ZONE_PROJECT gap the previous wave closed, left open one map over.
    # Inventing a command for it would be the `tests/` mistake all over
    # again; the honest answer is to fail closed, the same way an unmapped
    # zone does, not to verify nothing while still recording checks: green.
    data = json.loads(json.dumps(GOOD))
    data["consumes"].append({
        "project": "zomboid", "id": "music/composer", "via": "script",
        "page": "zomboid/index.html", "contract": "zomboid/tests/t.js",
    })
    assert "zomboid" not in PROJECT_CHECKS
    with pytest.raises(UnmappedConsumerError):
        all_commands("music/", _repo(tmp_path, data))


def test_a_command_needed_twice_is_run_once(tmp_path):
    # `film` consumes two different things music publishes, but is still
    # only one *project* — exchange.consumers_of already dedups at that
    # level (see test_exchange.py), so this exercises that path, not the
    # dedup guard inside all_commands itself. See
    # test_two_consumers_sharing_a_check_run_it_once below for that.
    data = json.loads(json.dumps(GOOD))
    data["publishes"]["music/player"] = {
        "project": "music", "summary": "Play.", "files": ["music/js/engine.js"],
    }
    data["consumes"].append({
        "project": "film", "id": "music/player", "via": "script",
        "page": "film/index.html", "contract": "film/tests/film-logic.test.js",
    })
    cmds = all_commands("music/", _repo(tmp_path, data))
    assert cmds.count(("node", "film/tests/film-logic.test.js")) == 1


def test_two_consumers_sharing_a_check_run_it_once(tmp_path, monkeypatch):
    # This is the case the `if cmd not in out` guard inside all_commands
    # actually exists for: two *distinct* downstream projects (so
    # exchange.consumers_of's own dedup does not collapse them) that happen
    # to be verified by the exact same command. Without the guard, deleting
    # it does not fail test_a_command_needed_twice_is_run_once above (that
    # test's "duplication" is fully absorbed by consumers_of's set), so it
    # would ship a dead line believed to be load-bearing.
    monkeypatch.setitem(
        PROJECT_CHECKS, "reprise", (("node", "film/tests/film-logic.test.js"),)
    )
    data = json.loads(json.dumps(GOOD))
    data["consumes"].append({
        "project": "reprise", "id": "music/composer", "via": "script",
        "page": "reprise/index.html", "contract": "film/tests/film-logic.test.js",
    })
    cmds = all_commands("music/", _repo(tmp_path, data))
    assert cmds.count(("node", "film/tests/film-logic.test.js")) == 1


# --- Minor: the Forge must run scripts/check-exchange.mjs itself ----------


def test_all_commands_includes_the_exchange_gate_for_a_project_zone(tmp_path):
    # Reproduced: a music/ rename with both pages updated consistently
    # passes both node test suites while check-exchange.mjs exits 1 on a
    # stale declaration elsewhere — the ledger recorded green on a PR that
    # would fail CI on this branch's own gate.
    assert EXCHANGE_CHECK_CMD in all_commands("music/", _repo(tmp_path))


def test_all_commands_runs_the_exchange_gate_for_a_project_less_zone_too(tmp_path):
    # Important 2: EXCHANGE_CHECK_CMD is a whole-repo gate — its result does
    # not depend on which zone changed, so gating it on "has a project" was
    # the wrong condition. docs/, the most-used safe zone, must run it too.
    #
    # Reproduced against a real copy of the repo: the Forge creates
    # docs/demo.html with two undeclared <script src="../music/js/...">
    # tags. Before this fix, VERIFY returned ok=True, ran=() and recorded
    # checks: green, while `node scripts/check-exchange.mjs` on the same
    # tree exited 1 naming both undeclared references. See
    # test_verify_important2.py for that exact end-to-end reproduction;
    # this test pins the unit-level cause.
    assert EXCHANGE_CHECK_CMD in all_commands("docs/", _repo(tmp_path))


def test_an_unreadable_declaration_raises_rather_than_narrowing(tmp_path):
    # The whole point: falling back to own-zone checks here would report
    # `checks: green` having verified less than it claims.
    with pytest.raises(ExchangeError):
        all_commands("music/", _repo(tmp_path, "{ not json"))


def test_a_missing_declaration_raises_even_for_a_zone_with_no_consumers(tmp_path):
    # Fail closed everywhere. Without the file we cannot prove a zone has no
    # consumers, so "no consumers" is not an answer we are entitled to give.
    with pytest.raises(ExchangeError):
        all_commands("crew/tests/", tmp_path)


def test_a_project_none_zone_still_raises_on_a_missing_declaration(tmp_path):
    # docs/ has no project (ZONE_PROJECT["docs/"] is None). A version of
    # all_commands that checks `if project is None: return` *before*
    # calling load(root) would skip validation entirely for every such
    # zone and never notice a missing or broken declaration — the test
    # above never catches this because "crew/tests/" has a project and so
    # reaches load(root) either way.
    with pytest.raises(ExchangeError):
        all_commands("docs/", tmp_path)


def test_run_checks_fails_when_the_declaration_is_unreadable(tmp_path):
    from forge.verify import run_checks

    _repo(tmp_path, "{ not json")
    result = run_checks("music/", tmp_path, runner=lambda cmd, root: (0, "ok"))
    assert result.ok is False
    assert "exchange" in result.output.lower()


# --- Important 2: a producer with no own tests must still contribute its --
# --- consumers' checks — ZONE_PROJECT["madlibs/"] must not read as "not a -
# --- project" the way ZONE_PROJECT["docs/"] correctly does. ---------------


def test_madlibs_picks_up_a_declared_consumers_checks(tmp_path):
    # film/index.html loading ../madlibs/js/generator.js, honestly declared:
    # madlibs has no tests of its own, but film — which consumes it — does,
    # and that check must run when madlibs is verified.
    data = {
        "publishes": {
            "madlibs/generator": {
                "project": "madlibs", "summary": "Generate a story idea.",
                "files": ["madlibs/js/generator.js"],
            },
        },
        "consumes": [
            {"project": "film", "id": "madlibs/generator", "via": "script",
             "page": "film/index.html", "contract": "film/tests/film-logic.test.js"},
        ],
    }
    cmds = all_commands("madlibs/", _repo(tmp_path, data))
    assert ("node", "film/tests/film-logic.test.js") in cmds


def test_docs_still_contributes_no_consumer_checks_even_with_the_same_shaped_declaration(tmp_path):
    # docs/ is the one zone that must stay project-less: pinned separately
    # so a fix that (wrongly) maps every zone to a project doesn't pass the
    # madlibs test above by accident. It still runs the exchange gate (see
    # test_all_commands_runs_the_exchange_gate_for_a_project_less_zone_too)
    # but must never pick up film's test just because madlibs happens to be
    # declared as a producer elsewhere.
    data = {
        "publishes": {
            "madlibs/generator": {
                "project": "madlibs", "summary": "Generate a story idea.",
                "files": ["madlibs/js/generator.js"],
            },
        },
        "consumes": [
            {"project": "film", "id": "madlibs/generator", "via": "script",
             "page": "film/index.html", "contract": "film/tests/film-logic.test.js"},
        ],
    }
    assert all_commands("docs/", _repo(tmp_path, data)) == (EXCHANGE_CHECK_CMD,)


def test_run_checks_runs_the_downstream_command(tmp_path):
    from forge.verify import run_checks

    calls = []
    result = run_checks("music/", _repo(tmp_path),
                        runner=lambda cmd, root: (calls.append(cmd), (0, "ok"))[1])
    assert result.ok is True
    assert ("node", "film/tests/film-logic.test.js") in calls
