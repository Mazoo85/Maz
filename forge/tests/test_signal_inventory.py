"""Reading docs/inventory.json — the repo's own catalogue — as candidates."""

import json

import pytest

from forge.models import Candidate
from forge.signals.inventory import MAX_FANOUT, QUEUE_PATH, collect

QUEUE = {
    "schema": 1,
    "stats": {"totalArtifacts": 3},
    "queue": [
        {
            "id": "web:madlibs#tests",
            "priority": 1,
            "kind": "web-app",
            "check": "tests",
            "title": "MADLIBS STORY FORGE: has a logic test",
            "detail": "Add madlibs/tests/ with a dependency-free Node test.",
            "paths": ["madlibs"],
            "source": "inventory:web:madlibs:tests",
        },
        {
            "id": "group:app:headless",
            "priority": 2,
            "kind": "app",
            "check": "headless",
            "title": "3 apps cannot run without a display",
            "detail": "area2d, bus, ccd. Example: Teach apps/area2d the --headless flags.",
            "paths": ["apps/area2d", "apps/bus", "apps/ccd"],
            "source": "inventory:app:headless",
            "memberCount": 3,
            "members": [
                {"id": "app:area2d", "name": "area2d", "path": "apps/area2d",
                 "fix": "Teach apps/area2d the --headless / --frames N flags."},
                {"id": "app:bus", "name": "bus", "path": "apps/bus",
                 "fix": "Teach apps/bus the --headless / --frames N flags."},
                {"id": "app:ccd", "name": "ccd", "path": "apps/ccd",
                 "fix": "Teach apps/ccd the --headless / --frames N flags."},
            ],
        },
        {
            "id": "pairing:songforge-scores-the-silent-games",
            "priority": 1,
            "kind": "pairing",
            "check": "",
            "title": "SONG FORGE scores the two silent browser games",
            "detail": "Publish a playback-only entry point and consume it.",
            "paths": [],
            "source": "inventory:pairing:songforge-scores-the-silent-games",
        },
    ],
}


def write_queue(root, data=QUEUE, path=QUEUE_PATH):
    target = root / path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(data), encoding="utf-8")
    return target


def test_an_itemised_task_becomes_one_candidate(tmp_path):
    write_queue(tmp_path)
    by_source = {c.source: c for c in collect(tmp_path)}
    c = by_source["inventory:web:madlibs:tests"]
    assert c.kind == "inventory"
    assert "MADLIBS" in c.task
    assert "dependency-free Node test" in c.task
    assert c.paths == ("madlibs",)
    assert c.detail == "p1"


def test_a_grouped_task_fans_out_to_one_candidate_per_member(tmp_path):
    write_queue(tmp_path)
    fanned = [c for c in collect(tmp_path) if c.source.startswith("inventory:app:headless:")]
    assert len(fanned) == 3
    # Each candidate must be scoped to its OWN app and carry its OWN fix text.
    # Handing all three the group's first sentence would send Crew to the wrong
    # file twice out of three times.
    by_path = {c.paths[0]: c for c in fanned}
    assert set(by_path) == {"apps/area2d", "apps/bus", "apps/ccd"}
    assert "apps/bus" in by_path["apps/bus"].task
    assert "apps/area2d" not in by_path["apps/bus"].task


def test_the_group_itself_is_not_also_offered(tmp_path):
    write_queue(tmp_path)
    sources = {c.source for c in collect(tmp_path)}
    assert "inventory:app:headless" not in sources, "the group and its members would be the same work twice"


def test_fanout_is_capped(tmp_path):
    many = json.loads(json.dumps(QUEUE))
    group = many["queue"][1]
    group["members"] = [
        {"id": f"app:a{i}", "name": f"a{i}", "path": f"apps/a{i}", "fix": f"Teach apps/a{i} the flags."}
        for i in range(MAX_FANOUT + 25)
    ]
    write_queue(tmp_path, many)
    fanned = [c for c in collect(tmp_path) if c.source.startswith("inventory:app:headless:")]
    assert len(fanned) == MAX_FANOUT


def test_a_task_with_no_paths_is_dropped(tmp_path):
    write_queue(tmp_path)
    sources = {c.source for c in collect(tmp_path)}
    # zone_for treats unknown scope as unsafe, so a pathless proposal can only
    # ever be scored and rejected. Dropping it here keeps the pulse honest.
    assert "inventory:pairing:songforge-scores-the-silent-games" not in sources


def test_a_task_wider_than_the_file_cap_is_dropped(tmp_path):
    wide = json.loads(json.dumps(QUEUE))
    wide["queue"] = [dict(QUEUE["queue"][0], paths=[f"madlibs/js/f{i}.js" for i in range(20)])]
    write_queue(tmp_path, wide)
    assert collect(tmp_path, max_files=12) == []
    assert len(collect(tmp_path, max_files=25)) == 1


def test_tests_nearby_is_set_when_a_test_directory_sits_beside_the_work(tmp_path):
    write_queue(tmp_path)
    (tmp_path / "madlibs" / "tests").mkdir(parents=True)
    by_source = {c.source: c for c in collect(tmp_path)}
    assert by_source["inventory:web:madlibs:tests"].tests_nearby is True


def test_tests_nearby_is_false_without_one(tmp_path):
    write_queue(tmp_path)
    (tmp_path / "madlibs").mkdir(parents=True)
    by_source = {c.source: c for c in collect(tmp_path)}
    assert by_source["inventory:web:madlibs:tests"].tests_nearby is False


@pytest.mark.parametrize(
    "payload",
    [
        None,                                  # no file at all
        "not json{",                           # unparseable
        json.dumps([1, 2, 3]),                 # valid JSON, wrong shape
        json.dumps({"schema": 99, "queue": QUEUE["queue"]}),   # a schema from the future
        json.dumps({"schema": 1, "queue": "nope"}),
        json.dumps({"schema": 1, "queue": [{"priority": 1}]}),  # entry missing everything
        json.dumps({"schema": 1, "queue": [{"priority": 9, "title": "x", "source": "y", "paths": ["z"]}]}),
    ],
)
def test_a_broken_queue_costs_the_signal_not_the_night(tmp_path, payload):
    if payload is not None:
        (tmp_path / "docs").mkdir(parents=True, exist_ok=True)
        (tmp_path / QUEUE_PATH).write_text(payload, encoding="utf-8")
    assert collect(tmp_path) == []


def test_every_candidate_is_a_candidate(tmp_path):
    write_queue(tmp_path)
    found = collect(tmp_path)
    assert found
    assert all(isinstance(c, Candidate) for c in found)
    # sense() rejects a collector whose items are the wrong shape, and every
    # candidate needs a stable identity for strike counting.
    assert len({c.key() for c in found}) == len(found)
