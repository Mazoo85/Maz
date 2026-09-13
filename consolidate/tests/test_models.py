from consolidate.models import Placement, Plan, SourceRepo, Step


def test_source_repo_round_trips_through_a_dict():
    repo = SourceRepo("Mazoo85", "Maz", "https://x/Maz.git", description="an engine", is_fork=True)
    assert SourceRepo.from_dict(repo.to_dict()) == repo


def test_from_dict_ignores_fields_it_does_not_know():
    repo = SourceRepo.from_dict({"owner": "o", "name": "n", "url": "u", "stars": 12})
    assert repo.slug == "o/n"


def test_plan_splits_included_from_skipped():
    a = Placement(SourceRepo("o", "a", "u"), "projects/a", "include")
    b = Placement(SourceRepo("o", "b", "u"), "projects/b", "skip", "a fork")
    plan = Plan("mine", "projects", (a, b))
    assert plan.included == (a,)
    assert plan.skipped == (b,)


def test_plan_round_trips_through_json():
    plan = Plan("mine", "projects", (Placement(SourceRepo("o", "a", "u"), "projects/a", "include"),))
    assert Plan.from_json(plan.to_json()) == plan


def test_a_planned_step_is_neither_pass_nor_fail():
    assert Step("init", "make a repo").ok is None
