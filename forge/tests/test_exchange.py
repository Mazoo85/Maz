"""Reading shared/exchange.json — who depends on whom."""

import json

import pytest

from forge.exchange import Exchange, ExchangeError, is_loadable, load

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


def _write(root, data):
    shared = root / "shared"
    shared.mkdir(parents=True, exist_ok=True)
    if isinstance(data, str):
        (shared / "exchange.json").write_text(data, encoding="utf-8")
    else:
        (shared / "exchange.json").write_text(json.dumps(data), encoding="utf-8")
    return root


def test_consumers_of_names_the_downstream_project(tmp_path):
    ex = load(_write(tmp_path, GOOD))
    assert ex.consumers_of("music") == ("film",)


def test_a_project_nobody_depends_on_has_no_consumers(tmp_path):
    ex = load(_write(tmp_path, GOOD))
    assert ex.consumers_of("madlibs") == ()


def test_the_consumer_is_not_its_own_consumer(tmp_path):
    ex = load(_write(tmp_path, GOOD))
    assert ex.consumers_of("film") == ()


def test_a_project_that_consumes_its_own_publication_is_excluded(tmp_path):
    # `film` never publishes anything in GOOD, so the test above never
    # exercises the `consumer != project` guard: publisher.get(id) == "film"
    # is false regardless of that check. Here `music` both publishes and
    # consumes "music/composer", so consumers_of("music") only excludes
    # "music" itself if the guard is actually applied.
    data = json.loads(json.dumps(GOOD))
    data["consumes"].append({
        "project": "music", "id": "music/composer", "via": "script",
        "page": "music/index.html", "contract": "music/tests/music-logic.test.js",
    })
    ex = load(_write(tmp_path, data))
    assert ex.consumers_of("music") == ("film",)


def test_two_consumers_of_one_project_are_both_named_once(tmp_path):
    # Five consumers, not two: with only two names ("film", "shooter"), a set
    # has roughly a coin-flip chance of iterating in the already-sorted order
    # by luck of hash randomization, which would let `tuple(sorted(out))` ->
    # `tuple(out)` survive this test on some runs and not others. Five
    # distinct names spread across the alphabet made that mutant's escape
    # probability negligible in 30/30 empirical trials against every CPython
    # hash seed tried.
    data = json.loads(json.dumps(GOOD))
    data["publishes"]["music/player"] = {
        "project": "music", "summary": "Play.", "files": ["music/js/engine.js"],
    }
    for consumer in ("shooter", "zephyr", "atlas", "quiz"):
        data["consumes"].append({
            "project": consumer, "id": "music/player", "via": "script",
            "page": f"{consumer}/index.html", "contract": f"{consumer}/tests/t.js",
        })
    ex = load(_write(tmp_path, data))
    assert ex.consumers_of("music") == ("atlas", "film", "quiz", "shooter", "zephyr")


def test_a_missing_file_raises_rather_than_returning_empty(tmp_path):
    # Returning an empty Exchange would silently narrow the Forge's checks,
    # which is the exact defect this whole feature exists to remove.
    with pytest.raises(ExchangeError):
        load(tmp_path)


def test_invalid_json_raises(tmp_path):
    with pytest.raises(ExchangeError):
        load(_write(tmp_path, "{ not json"))


def test_invalid_utf8_raises(tmp_path):
    shared = tmp_path / "shared"
    shared.mkdir(parents=True)
    (shared / "exchange.json").write_bytes(b'{"publishes": {}, "consumes": [\xff]}')
    with pytest.raises(ExchangeError):
        load(tmp_path)


def test_publishes_of_the_wrong_type_raises(tmp_path):
    with pytest.raises(ExchangeError):
        load(_write(tmp_path, {"publishes": [], "consumes": []}))


def test_consumes_of_the_wrong_type_raises(tmp_path):
    with pytest.raises(ExchangeError):
        load(_write(tmp_path, {"publishes": {}, "consumes": {}}))


def test_a_consumes_entry_that_is_not_a_dict_raises(tmp_path):
    # The container is a list, but one element is junk. A guard that checks
    # the container and not its elements is a defect this codebase has
    # shipped five times.
    with pytest.raises(ExchangeError):
        load(_write(tmp_path, {"publishes": {}, "consumes": ["film"]}))


def test_a_publishes_entry_that_is_not_a_dict_raises(tmp_path):
    with pytest.raises(ExchangeError):
        load(_write(tmp_path, {"publishes": {"music/x": "music"}, "consumes": []}))


def test_a_consumes_entry_naming_an_unpublished_id_raises(tmp_path):
    data = json.loads(json.dumps(GOOD))
    data["consumes"][0]["id"] = "music/ghost"
    with pytest.raises(ExchangeError):
        load(_write(tmp_path, data))


def test_is_loadable_is_true_for_a_good_file(tmp_path):
    assert is_loadable(_write(tmp_path, GOOD)) is True


def test_is_loadable_is_false_for_a_bad_file_and_does_not_raise(tmp_path):
    assert is_loadable(_write(tmp_path, "{ not json")) is False


def test_is_loadable_is_false_when_the_file_is_missing(tmp_path):
    assert is_loadable(tmp_path) is False
