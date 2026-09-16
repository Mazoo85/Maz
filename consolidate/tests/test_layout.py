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


# --------------------------------------------------------------------------
# never overwriting a file we did not write
# --------------------------------------------------------------------------

def test_a_file_we_generated_is_ours_to_rewrite(tmp_path):
    layout.write_scaffold(tmp_path, a_plan())
    assert layout.is_generated(tmp_path / "README.md")


def test_someone_elses_readme_is_not_ours_to_rewrite(tmp_path):
    (tmp_path / "README.md").write_text("# My Project\n\nYears of work.\n")
    assert not layout.is_generated(tmp_path / "README.md")


def test_a_file_that_is_not_there_yet_is_free(tmp_path):
    assert layout.is_generated(tmp_path / "nothing-here.md")


def test_an_existing_readme_is_reported_as_a_conflict_before_anything_runs(tmp_path):
    (tmp_path / "README.md").write_text("# My Project\n")
    assert layout.conflicts(tmp_path, a_plan()) == ["README.md"]


def test_an_existing_readme_is_never_overwritten(tmp_path):
    mine = "# My Project\n\nYears of work.\n"
    (tmp_path / "README.md").write_text(mine)
    written = layout.write_scaffold(tmp_path, a_plan())
    assert (tmp_path / "README.md").read_text() == mine
    assert "README.md" not in written


def test_routing_the_index_elsewhere_removes_the_conflict(tmp_path):
    mine = "# My Project\n"
    (tmp_path / "README.md").write_text(mine)
    plan = a_plan()
    adopted = plan.__class__(plan.dest_name, plan.prefix, plan.placements, index_file="PROJECTS.md")
    assert layout.conflicts(tmp_path, adopted) == []
    written = layout.write_scaffold(tmp_path, adopted)
    assert "PROJECTS.md" in written
    assert (tmp_path / "README.md").read_text() == mine


def test_the_manifest_records_where_the_index_went(tmp_path):
    plan = a_plan()
    adopted = plan.__class__(plan.dest_name, plan.prefix, plan.placements,
                             index_file="PROJECTS.md", host="o/home")
    layout.write_scaffold(tmp_path, adopted)
    assert layout.read_manifest(tmp_path) == adopted


# --------------------------------------------------------------------------
# the index of a repository that was adopted rather than created
# --------------------------------------------------------------------------

def an_adopted_plan(**kwargs):
    plan = a_plan()
    return plan.__class__(plan.dest_name, plan.prefix, plan.placements,
                          index_file="PROJECTS.md", adopted=True, **kwargs)


def test_an_adopted_index_lists_what_was_already_in_the_repo():
    text = layout.render_readme(an_adopted_plan(), existing=["engine", "apps", "docs"])
    assert "Already here" in text
    assert "[`engine`](engine)" in text
    assert "[`apps`](apps)" in text


def test_an_adopted_index_still_lists_what_was_folded_in():
    text = layout.render_readme(an_adopted_plan(), existing=["engine"])
    assert "Folded in from another repository" in text
    assert "projects/alpha" in text


def test_an_adopted_index_with_nothing_folded_in_is_not_an_empty_page():
    plan = build_plan([], dest_name="Home")
    adopted = plan.__class__(plan.dest_name, plan.prefix, (), index_file="PROJECTS.md", adopted=True)
    text = layout.render_readme(adopted, existing=["engine", "apps"])
    assert "This repository is the home" in text
    assert "[`engine`](engine)" in text


def test_a_newly_created_repo_has_no_already_here_section():
    text = layout.render_readme(a_plan(), existing=["ignored"])
    assert "Already here" not in text


def test_an_adopted_index_does_not_tell_you_to_cd_into_projects():
    plan = build_plan([], dest_name="Home")
    adopted = plan.__class__(plan.dest_name, plan.prefix, (), index_file="PROJECTS.md", adopted=True)
    text = layout.render_readme(adopted, existing=["engine"])
    assert "cd projects/<project>" not in text
    assert "still works unchanged" in text
