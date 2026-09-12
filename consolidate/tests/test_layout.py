import json

from consolidate import layout
from consolidate.models import SourceRepo
from consolidate.plan import build_plan


def a_plan(**kwargs):
    return build_plan(
        [
            SourceRepo("o", "alpha", "https://github.com/o/alpha.git", description="the first one"),
            SourceRepo("o", "forked", "https://github.com/o/forked.git", is_fork=True),
        ],
        dest_name="mine",
        **kwargs,
    )


def test_the_readme_lists_every_folded_in_project_with_its_origin():
    text = layout.render_readme(a_plan())
    assert "projects/alpha" in text
    assert "the first one" in text
    assert "https://github.com/o/alpha" in text


def test_the_readme_says_what_was_left_out_and_why():
    text = layout.render_readme(a_plan())
    assert "Deliberately left out" in text
    assert "o/forked" in text
    assert "upstream" in text


def test_a_plan_with_nothing_in_it_still_renders():
    plan = build_plan([], dest_name="empty-one")
    assert "_none yet_" in layout.render_readme(plan)
    assert "_none_" in layout.render_provenance(plan)


def test_the_record_carries_the_commit_each_project_was_merged_at():
    text = layout.render_provenance(a_plan(), shas={"projects/alpha": "a" * 40})
    assert "a" * 12 in text
    assert "git subtree" in text


def test_the_record_explains_how_to_read_a_projects_history():
    text = layout.render_provenance(a_plan(), shas={"projects/alpha": "a" * 40})
    assert "git log --oneline <commit from the table>" in text
    assert "--follow" in text


def test_the_manifest_is_a_plan_plus_the_commits_it_merged():
    text = layout.render_manifest(a_plan(), shas={"projects/alpha": "a" * 40})
    data = json.loads(text)
    assert data["dest_name"] == "mine"
    assert data["merged_commits"] == {"projects/alpha": "a" * 40}
    assert len(data["placements"]) == 2


def test_the_scaffold_writes_the_files_the_repo_needs(tmp_path):
    written = layout.write_scaffold(tmp_path, a_plan())
    assert written == [".gitignore", "CONSOLIDATION.md", "README.md", "consolidate.json"]
    for name in written:
        assert (tmp_path / name).read_text().strip()


def test_a_hand_edited_gitignore_is_never_overwritten(tmp_path):
    (tmp_path / ".gitignore").write_text("my own rules\n")
    layout.write_scaffold(tmp_path, a_plan())
    assert (tmp_path / ".gitignore").read_text() == "my own rules\n"


def test_a_built_repo_can_read_its_own_plan_back(tmp_path):
    plan = a_plan()
    layout.write_scaffold(tmp_path, plan)
    assert layout.read_manifest(tmp_path) == plan


def test_reading_a_manifest_that_is_not_there_is_not_an_error(tmp_path):
    assert layout.read_manifest(tmp_path) is None
    assert layout.read_manifest_commits(tmp_path) == {}


def test_recorded_commits_are_read_back(tmp_path):
    layout.write_scaffold(tmp_path, a_plan(), shas={"projects/alpha": "a" * 40})
    assert layout.read_manifest_commits(tmp_path) == {"projects/alpha": "a" * 40}


def test_a_corrupt_manifest_does_not_crash_the_build(tmp_path):
    (tmp_path / layout.MANIFEST_NAME).write_text("{ not json")
    assert layout.read_manifest_commits(tmp_path) == {}
