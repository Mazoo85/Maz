"""Output writers: JSONL/CSV/SQLite round-trips and de-duplication."""

import csv
import json
import sqlite3

from scraper.store import CsvWriter, JsonlWriter, SqliteWriter, open_writers

RECORDS = [
    {"text": "A", "author": "Amy", "href": "/1"},
    {"text": "B", "author": "Bob", "href": "/2"},
    {"text": "A-again", "author": "Amy", "href": "/1"},  # duplicate href
]


def test_jsonl_roundtrip_and_dedup(tmp_path):
    path = tmp_path / "out.jsonl"
    with JsonlWriter(path, dedup_key="href") as w:
        written = [w.write(r) for r in RECORDS]
    assert written == [True, True, False]  # third is a dup on href
    lines = [json.loads(line) for line in path.read_text().splitlines()]
    assert [r["href"] for r in lines] == ["/1", "/2"]


def test_jsonl_no_dedup_writes_all(tmp_path):
    path = tmp_path / "out.jsonl"
    with JsonlWriter(path) as w:
        assert all(w.write(r) for r in RECORDS)
    assert len(path.read_text().splitlines()) == 3


def test_csv_roundtrip_and_header(tmp_path):
    path = tmp_path / "out.csv"
    with CsvWriter(path, dedup_key="href") as w:
        for r in RECORDS:
            w.write(r)
    rows = list(csv.DictReader(path.read_text().splitlines()))
    assert [r["href"] for r in rows] == ["/1", "/2"]
    assert set(rows[0].keys()) == {"text", "author", "href"}


def test_csv_stable_columns_from_first_record(tmp_path):
    path = tmp_path / "out.csv"
    with CsvWriter(path) as w:
        w.write({"a": 1, "b": 2})
        w.write({"a": 3, "b": 4, "c": 5})  # extra key ignored
    rows = list(csv.reader(path.read_text().splitlines()))
    assert rows[0] == ["a", "b"]
    assert rows[2] == ["3", "4"]


def test_sqlite_roundtrip_and_dedup(tmp_path):
    path = tmp_path / "out.db"
    with SqliteWriter(path, dedup_key="href") as w:
        written = [w.write(r) for r in RECORDS]
    assert written == [True, True, False]
    conn = sqlite3.connect(str(path))
    rows = conn.execute("SELECT dedup_key, data FROM records ORDER BY id").fetchall()
    conn.close()
    assert [row[0] for row in rows] == ["/1", "/2"]
    assert json.loads(rows[0][1])["author"] == "Amy"


def test_sqlite_incremental_across_runs(tmp_path):
    # Re-opening the same DB and writing a seen key must be ignored (resumable).
    path = tmp_path / "out.db"
    with SqliteWriter(path, dedup_key="href") as w:
        w.write(RECORDS[0])
    with SqliteWriter(path, dedup_key="href") as w:
        assert w.write(RECORDS[0]) is False  # already in the DB from the first run
        assert w.write(RECORDS[1]) is True
    conn = sqlite3.connect(str(path))
    (count,) = conn.execute("SELECT COUNT(*) FROM records").fetchone()
    conn.close()
    assert count == 2


def test_open_writers_multiformat(tmp_path):
    mw = open_writers(["jsonl", "csv", "sqlite"], tmp_path, "quotes", dedup_key="href")
    with mw:
        for r in RECORDS:
            mw.write(r)
    assert (tmp_path / "quotes.jsonl").exists()
    assert (tmp_path / "quotes.csv").exists()
    assert (tmp_path / "quotes.db").exists()
