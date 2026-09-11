"""What "green" covers: a zone's own checks, plus everything downstream."""

import json

import pytest

from forge.checks import EXCHANGE_CHECK_CMD, PROJECT_CHECKS, all_commands, commands_for
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


def test_commands_for_is_unchanged_and_takes_no_root():
    # The pure, own-zone map. Task 4 must not alter what existing callers see.
    assert commands_for("music/") == (("node", "music/tests/music-logic.test.js"),)
    assert commands_for("docs/") == ()
    assert commands_for("nowhere/") == ()
    assert commands_for("tests/") == ()


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


def test_a_zone_with_no_checks_of_its_own_stays_empty(tmp_path):
    assert all_commands("docs/", _repo(tmp_path)) == ()


def test_a_consumer_with_no_known_checks_adds_nothing(tmp_path):
    # zomboid consumes music but has no entry in PROJECT_CHECKS. Inventing a
    # command for it would be the `tests/` mistake all over again.
    data = json.loads(json.dumps(GOOD))
    data["consumes"].append({
        "project": "zomboid", "id": "music/composer", "via": "script",
        "page": "zomboid/index.html", "contract": "zomboid/tests/t.js",
    })
    assert "zomboid" not in PROJECT_CHECKS
    cmds = all_commands("music/", _repo(tmp_path, data))
    assert not any("zomboid" in " ".join(c) for c in cmds)


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


def test_all_commands_does_not_run_the_exchange_gate_for_a_project_less_zone(tmp_path):
    assert EXCHANGE_CHECK_CMD not in all_commands("docs/", _repo(tmp_path))


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


def test_docs_still_contributes_nothing_even_with_the_same_shaped_declaration(tmp_path):
    # docs/ is the one zone that must stay project-less: pinned separately
    # so a fix that (wrongly) maps every zone to a project doesn't pass the
    # madlibs test above by accident.
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
    assert all_commands("docs/", _repo(tmp_path, data)) == ()


def test_run_checks_runs_the_downstream_command(tmp_path):
    from forge.verify import run_checks

    calls = []
    result = run_checks("music/", _repo(tmp_path),
                        runner=lambda cmd, root: (calls.append(cmd), (0, "ok"))[1])
    assert result.ok is True
    assert ("node", "film/tests/film-logic.test.js") in calls
