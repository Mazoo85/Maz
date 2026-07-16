"""Output writers: JSONL, CSV, and SQLite, behind one small interface.

Every writer implements ``write(record)`` and ``close()``. :class:`MultiWriter`
fans each record out to several formats at once. Records are plain dicts.

De-duplication: when a ``dedup_key`` is configured, a record whose key value has
already been seen is skipped. SQLite enforces this durably with a ``UNIQUE``
index (so re-running a scrape is incremental — duplicates are ignored, not
appended), while the file writers dedup within the current run.
"""

from __future__ import annotations

import csv
import json
import sqlite3
from pathlib import Path


class Writer:
    """Interface: subclasses implement ``write`` and ``close``."""

    def write(self, record: dict) -> bool:  # returns True if written (not a dup)
        raise NotImplementedError

    def close(self) -> None:
        pass

    def __enter__(self) -> "Writer":
        return self

    def __exit__(self, *exc) -> None:
        self.close()


def _dedup_value(record: dict, dedup_key: str | None):
    return record.get(dedup_key) if dedup_key else None


class JsonlWriter(Writer):
    """One JSON object per line."""

    def __init__(self, path: str | Path, dedup_key: str | None = None) -> None:
        self._path = Path(path)
        self._path.parent.mkdir(parents=True, exist_ok=True)
        self._fh = self._path.open("w", encoding="utf-8")
        self._dedup_key = dedup_key
        self._seen: set = set()

    def write(self, record: dict) -> bool:
        key = _dedup_value(record, self._dedup_key)
        if key is not None:
            if key in self._seen:
                return False
            self._seen.add(key)
        self._fh.write(json.dumps(record, ensure_ascii=False) + "\n")
        return True

    def close(self) -> None:
        self._fh.close()


class CsvWriter(Writer):
    """A CSV file. Columns are the keys of the first record written."""

    def __init__(self, path: str | Path, dedup_key: str | None = None) -> None:
        self._path = Path(path)
        self._path.parent.mkdir(parents=True, exist_ok=True)
        self._fh = self._path.open("w", encoding="utf-8", newline="")
        self._dedup_key = dedup_key
        self._seen: set = set()
        self._writer: csv.DictWriter | None = None

    def write(self, record: dict) -> bool:
        key = _dedup_value(record, self._dedup_key)
        if key is not None:
            if key in self._seen:
                return False
            self._seen.add(key)
        if self._writer is None:
            self._writer = csv.DictWriter(self._fh, fieldnames=list(record.keys()))
            self._writer.writeheader()
        # Ignore any keys beyond the header columns (stable schema from record 1).
        row = {k: record.get(k) for k in self._writer.fieldnames}
        self._writer.writerow(row)
        return True

    def close(self) -> None:
        self._fh.close()


class SqliteWriter(Writer):
    """A SQLite table with one JSON column plus promoted top-level fields.

    When ``dedup_key`` is set, that value goes in a ``UNIQUE`` column and inserts
    use ``INSERT OR IGNORE`` — so duplicates (within a run *or* across re-runs of
    the same DB) are silently skipped, making scrapes resumable/incremental.
    """

    def __init__(self, path: str | Path, dedup_key: str | None = None, table: str = "records") -> None:
        self._path = Path(path)
        self._path.parent.mkdir(parents=True, exist_ok=True)
        self._dedup_key = dedup_key
        self._table = table
        self._conn = sqlite3.connect(str(self._path))
        self._conn.execute(
            f"CREATE TABLE IF NOT EXISTS {self._table} ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  dedup_key TEXT,"
            "  data TEXT NOT NULL"
            ")"
        )
        if dedup_key:
            self._conn.execute(
                f"CREATE UNIQUE INDEX IF NOT EXISTS ux_{self._table}_dedup "
                f"ON {self._table}(dedup_key) WHERE dedup_key IS NOT NULL"
            )
        self._conn.commit()

    def write(self, record: dict) -> bool:
        key = _dedup_value(record, self._dedup_key)
        payload = json.dumps(record, ensure_ascii=False)
        verb = "INSERT OR IGNORE" if self._dedup_key else "INSERT"
        cur = self._conn.execute(
            f"{verb} INTO {self._table} (dedup_key, data) VALUES (?, ?)",
            (None if key is None else str(key), payload),
        )
        self._conn.commit()
        return cur.rowcount > 0

    def close(self) -> None:
        self._conn.close()


# Map format name -> writer class and file extension.
_FORMATS = {
    "jsonl": (JsonlWriter, "jsonl"),
    "csv": (CsvWriter, "csv"),
    "sqlite": (SqliteWriter, "db"),
}


class MultiWriter(Writer):
    """Fan each record out to several format writers at once."""

    def __init__(self, writers: list[Writer]) -> None:
        self._writers = writers

    def write(self, record: dict) -> bool:
        written = False
        for w in self._writers:
            # ``or`` so a record counts as written if any writer accepted it.
            written = w.write(record) or written
        return written

    def close(self) -> None:
        for w in self._writers:
            w.close()


def open_writers(
    formats,
    output_dir: str | Path,
    name: str,
    dedup_key: str | None = None,
) -> MultiWriter:
    """Open a :class:`MultiWriter` for ``formats`` under ``output_dir/<name>.*``."""
    out = Path(output_dir)
    writers: list[Writer] = []
    for fmt in formats:
        cls, ext = _FORMATS[fmt]
        writers.append(cls(out / f"{name}.{ext}", dedup_key=dedup_key))
    return MultiWriter(writers)
