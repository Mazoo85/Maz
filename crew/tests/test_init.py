"""`crew init` scaffolds a starter crew.json that load_config reads back."""

import json

from crew.config import (
    CONFIG_FILENAME,
    default_config_dict,
    load_config,
    write_starter_config,
)


def test_writes_starter_file(tmp_path):
    written, path = write_starter_config(root=tmp_path)
    assert written is True
    assert path == tmp_path / CONFIG_FILENAME
    data = json.loads(path.read_text())
    assert data == default_config_dict()


def test_starter_file_is_valid_config(tmp_path):
    write_starter_config(root=tmp_path)
    cfg = load_config(root=tmp_path)
    # A freshly scaffolded file round-trips to the defaults.
    assert cfg.max_fix_rounds == 3
    assert cfg.max_turns == 40


def test_does_not_clobber_without_force(tmp_path):
    (tmp_path / CONFIG_FILENAME).write_text('{"max_fix_rounds": 9}')
    written, _ = write_starter_config(root=tmp_path)
    assert written is False
    assert load_config(root=tmp_path).max_fix_rounds == 9  # untouched


def test_force_overwrites(tmp_path):
    (tmp_path / CONFIG_FILENAME).write_text('{"max_fix_rounds": 9}')
    written, _ = write_starter_config(root=tmp_path, force=True)
    assert written is True
    assert load_config(root=tmp_path).max_fix_rounds == 3  # back to default
