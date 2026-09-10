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
    # Relation record with observations that would match if treated as an entity
    relation_line = (
        '{"type":"relation","from":"A","to":"B","relationType":"uses",'
        '"observations":["Still to implement the connection"]}'
    )
    # Relations are ignored despite having matching observations
    assert parse(relation_line) == []

    # The same observation in an entity record DOES produce a candidate
    entity_line = (
        '{"type":"entity","name":"A","entityType":"component",'
        '"observations":["Still to implement the connection"]}'
    )
    cands = parse(entity_line)
    assert len(cands) == 1
    assert "connection" in cands[0].task


def test_malformed_lines_are_skipped():
    text = "{ not json\n" + SAMPLE
    assert len(parse(text)) == 1


def test_empty_input_yields_nothing():
    # Empty string yields nothing
    assert parse("") == []

    # Blank lines and whitespace-only lines do not break parsing;
    # valid records among them are still found
    text = (
        "\n"
        "  \n"
        "\t\n"
        '{"type":"entity","name":"X","entityType":"component",'
        '"observations":["Still to add feature"]}\n'
        "\n"
        "   \t   \n"
    )
    cands = parse(text)
    assert len(cands) == 1
    assert "feature" in cands[0].task
