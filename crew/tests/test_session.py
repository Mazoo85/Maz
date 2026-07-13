"""Session state round-trips and never crashes on a corrupt file."""

from crew import session as s
from crew.config import CrewConfig


def test_round_trip(tmp_path):
    cfg = CrewConfig()
    st = s.SessionState(session_id="abc-123", task="demo", phase="code")
    s.save(st, cfg, root=tmp_path)

    loaded = s.load(cfg, root=tmp_path)
    assert loaded.session_id == "abc-123"
    assert loaded.task == "demo"
    assert loaded.phase == "code"


def test_load_missing_is_blank(tmp_path):
    loaded = s.load(CrewConfig(), root=tmp_path)
    assert loaded.session_id is None and loaded.task is None


def test_clear(tmp_path):
    cfg = CrewConfig()
    s.save(s.SessionState(session_id="x"), cfg, root=tmp_path)
    s.clear(cfg, root=tmp_path)
    assert s.load(cfg, root=tmp_path).session_id is None


def test_corrupt_file_recovers(tmp_path):
    cfg = CrewConfig()
    d = cfg.state_dir(tmp_path)
    d.mkdir(parents=True)
    (d / s.SESSION_FILE).write_text("{not valid json")

    loaded = s.load(cfg, root=tmp_path)  # must not raise
    assert loaded.session_id is None
