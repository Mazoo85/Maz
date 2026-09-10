"""Reading half-finished work out of the codebase-memory graph."""

from forge.signals.memory import parse

SAMPLE = "\n".join([
    '{"type":"entity","name":"Maz Engine","entityType":"component",'
    '"observations":["Built with CMake","Sprite batching is still to do for rotated quads"]}',
    '{"type":"entity","name":"SONG FORGE","entityType":"component",'
    '"observations":["Writes complete songs in the browser"]}',
    '{"type":"relation","from":"Maz Engine","to":"Maz Repository","relationType":"part of"}',
])


def test_unfinished_observations_become_candidates():
    cands = parse(SAMPLE)
    assert len(cands) == 1
    assert "rotated quads" in cands[0].task


def test_source_names_the_entity():
    assert parse(SAMPLE)[0].source == "memory:Maz Engine"


def test_kind_is_memory_and_paths_are_empty():
    c = parse(SAMPLE)[0]
    assert c.kind == "memory"
    assert c.paths == ()


def test_settled_observations_are_ignored():
    line = ('{"type":"entity","name":"X","entityType":"component",'
            '"observations":["This is finished and documented"]}')
    assert parse(line) == []


def test_relation_lines_are_ignored():
    line = '{"type":"relation","from":"A","to":"B","relationType":"uses"}'
    assert parse(line) == []


def test_malformed_lines_are_skipped():
    text = "{ not json\n" + SAMPLE
    assert len(parse(text)) == 1


def test_empty_input_yields_nothing():
    assert parse("") == []
