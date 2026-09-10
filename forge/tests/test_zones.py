"""Zone enforcement: safe zones admit, no-touch always wins."""

from forge.config import ForgeConfig, load_config
from forge.models import Candidate
from forge.zones import is_no_touch, risk_keys_for, zone_for


def test_path_inside_safe_zone_returns_zone():
    cfg = ForgeConfig()
    assert zone_for(("docs/ROADMAP.md",), cfg) == "docs/"


def test_path_outside_safe_zones_returns_none():
    cfg = ForgeConfig()
    assert zone_for(("engine/src/render/vk.cpp",), cfg) is None


def test_mixed_paths_reject_when_any_is_outside():
    cfg = ForgeConfig()
    assert zone_for(("docs/a.md", "engine/src/b.cpp"), cfg) is None


def test_empty_paths_returns_none():
    # A candidate with no known paths cannot be proven safe, so it isn't.
    assert zone_for((), ForgeConfig()) is None


def test_no_touch_beats_safe_zone(tmp_path):
    # Even if a user puts forge/ in safe_zones, no_touch wins.
    (tmp_path / "forge.json").write_text('{"safe_zones": ["forge/", "docs/"]}')
    cfg = load_config(root=tmp_path)
    assert is_no_touch(("forge/decide.py",), cfg) is True
    assert zone_for(("forge/decide.py",), cfg) is None
    assert zone_for(("docs/x.md",), cfg) == "docs/"


def test_workflows_are_no_touch():
    assert is_no_touch((".github/workflows/ci.yml",), ForgeConfig()) is True


def test_risk_keys_for_build_system():
    assert "risk_build_system" in risk_keys_for(("CMakeLists.txt",))
    assert "risk_engine_core" in risk_keys_for(("engine/src/core/app.cpp",))
    assert risk_keys_for(("docs/a.md",)) == ()


def test_risk_keys_for_cross_cutting():
    many = tuple(f"music/js/f{i}.js" for i in range(7))
    assert "risk_cross_cutting" in risk_keys_for(many)


def test_candidate_key_is_stable():
    c1 = Candidate(task="Add clang-tidy", source="roadmap:phase-0", kind="roadmap")
    c2 = Candidate(task="Add clang-tidy", source="roadmap:phase-0", kind="roadmap", detail="x")
    assert c1.key() == c2.key()


# --- Critical: ".." traversal must not defeat the leash ------------------


def test_dotdot_traversal_into_forge_is_no_touch_and_no_zone():
    cfg = ForgeConfig()
    path = "docs/../forge/decide.py"
    assert is_no_touch((path,), cfg) is True
    assert zone_for((path,), cfg) is None


def test_dotdot_traversal_into_workflows_is_no_touch_and_no_zone():
    cfg = ForgeConfig()
    path = "docs/../.github/workflows/x.yml"
    assert is_no_touch((path,), cfg) is True
    assert zone_for((path,), cfg) is None


# --- Minor: unparseable path shapes must fail closed too ------------------


def test_absolute_path_is_no_touch_and_no_zone():
    cfg = ForgeConfig()
    assert is_no_touch(("/forge/decide.py",), cfg) is True
    assert zone_for(("/forge/decide.py",), cfg) is None


def test_backslash_path_is_no_touch_and_no_zone():
    cfg = ForgeConfig()
    assert is_no_touch(("forge\\decide.py",), cfg) is True
    assert zone_for(("forge\\decide.py",), cfg) is None


def test_empty_and_whitespace_paths_are_no_touch_and_no_zone():
    cfg = ForgeConfig()
    assert is_no_touch(("",), cfg) is True
    assert zone_for(("",), cfg) is None
    assert is_no_touch(("   ",), cfg) is True
    assert zone_for(("   ",), cfg) is None


def test_dotslash_prefix_is_still_caught_as_no_touch():
    # The existing "./" strip must not regress: this is a no-touch hit, not
    # a suspicious-path rejection.
    cfg = ForgeConfig()
    assert is_no_touch(("./forge/x.py",), cfg) is True
    assert zone_for(("./forge/x.py",), cfg) is None


def test_dots_in_ordinary_filenames_are_not_treated_as_traversal():
    # ".." must only be caught as a whole path *segment*; a filename that
    # merely contains two dots is ordinary and must still be admitted.
    cfg = ForgeConfig()
    assert is_no_touch(("docs/a..b.md",), cfg) is False
    assert zone_for(("docs/a..b.md",), cfg) == "docs/"
    assert is_no_touch(("docs/v1.2..3/notes.md",), cfg) is False
    assert zone_for(("docs/v1.2..3/notes.md",), cfg) == "docs/"
