import io
import json

import pytest

from consolidate import discover
from consolidate.discover import DiscoveryError
from consolidate.models import SourceRepo


class FakeResponse(io.BytesIO):
    def __init__(self, payload, headers=None):
        super().__init__(json.dumps(payload).encode())
        self.headers = headers or {}

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        return False


@pytest.mark.parametrize(
    "spec",
    [
        "Mazoo85/Maz",
        "https://github.com/Mazoo85/Maz",
        "https://github.com/Mazoo85/Maz.git",
        "git@github.com:Mazoo85/Maz.git",
        "github.com/Mazoo85/Maz",
    ],
)
def test_every_way_of_writing_a_repo_parses_the_same(spec):
    assert discover.parse_spec(spec) == ("Mazoo85", "Maz")


@pytest.mark.parametrize("spec", ["", "not a repo", "owner/", "/name", "a/b/c"])
def test_nonsense_specs_are_rejected_with_advice(spec):
    with pytest.raises(DiscoveryError) as err:
        discover.parse_spec(spec)
    assert "owner/name" in str(err.value)


def test_from_specs_needs_no_network():
    repos = discover.from_specs(["Mazoo85/Maz"])
    assert repos == [
        SourceRepo(owner="Mazoo85", name="Maz", url="https://github.com/Mazoo85/Maz.git")
    ]


def test_a_discovered_list_survives_a_round_trip_through_a_file(tmp_path):
    repos = [SourceRepo("o", "a", "u", description="d", is_fork=True, default_branch="master")]
    path = tmp_path / "repos.json"
    discover.to_file(repos, path)
    assert discover.from_file(path) == repos


def test_a_bare_list_in_the_file_also_works(tmp_path):
    path = tmp_path / "repos.json"
    path.write_text(json.dumps([{"owner": "o", "name": "a", "url": "u"}]))
    assert discover.from_file(path)[0].slug == "o/a"


def test_from_github_reads_the_fields_that_drive_the_plan():
    payload = [
        {
            "name": "Maz",
            "owner": {"login": "Mazoo85"},
            "clone_url": "https://github.com/Mazoo85/Maz.git",
            "description": "  an engine  ",
            "default_branch": "main",
            "fork": False,
            "archived": True,
            "visibility": "public",
            "pushed_at": "2026-09-12T19:02:50Z",
        }
    ]
    repos = discover.from_github("Mazoo85", token=None, opener=lambda *a, **k: FakeResponse(payload))
    assert len(repos) == 1
    assert repos[0] == SourceRepo(
        owner="Mazoo85",
        name="Maz",
        url="https://github.com/Mazoo85/Maz.git",
        description="an engine",
        default_branch="main",
        archived=True,
        visibility="public",
        pushed_at="2026-09-12T19:02:50Z",
    )


def test_from_github_follows_pagination_to_the_end():
    pages = [
        ([{"name": "one", "owner": {"login": "o"}, "clone_url": "u1"}],
         {"Link": '<https://api.github.com/next>; rel="next"'}),
        ([{"name": "two", "owner": {"login": "o"}, "clone_url": "u2"}], {}),
    ]
    calls = iter(pages)

    def opener(*_a, **_k):
        payload, headers = next(calls)
        return FakeResponse(payload, headers)

    repos = discover.from_github("o", token=None, opener=opener)
    assert sorted(r.name for r in repos) == ["one", "two"]


def test_repos_owned_by_someone_else_are_filtered_out():
    payload = [
        {"name": "mine", "owner": {"login": "me"}, "clone_url": "u"},
        {"name": "theirs", "owner": {"login": "someone-else"}, "clone_url": "u"},
    ]
    repos = discover.from_github("me", token=None, opener=lambda *a, **k: FakeResponse(payload))
    assert [r.name for r in repos] == ["mine"]


def test_asking_for_nothing_at_all_is_an_error():
    with pytest.raises(DiscoveryError):
        discover.from_github(None, token=None)


def test_next_link_is_read_out_of_the_link_header():
    header = {"Link": '<https://api.github.com/x?page=2>; rel="next", <y>; rel="last"'}
    assert discover._next_link(header) == "https://api.github.com/x?page=2"
    assert discover._next_link({"Link": '<y>; rel="last"'}) is None
    assert discover._next_link({}) is None
