#!/usr/bin/env python3
"""Pack udio-rescue/extension/ into udio-rescue/udio-rescue-extension.zip.

The zip is what people download and install, so it is kept in the repo.
    python3 udio-rescue/build-zip.py          # rebuild the zip
    python3 udio-rescue/build-zip.py --check  # fail if the zip is out of date (CI)
"""
import sys
import zipfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
SRC = HERE / "extension"
ZIP = HERE / "udio-rescue-extension.zip"
TOP = "udio-rescue-extension"
STAMP = (2026, 1, 1, 0, 0, 0)  # fixed, so the same files always make the same zip


def wanted():
    return {
        f"{TOP}/{p.relative_to(SRC).as_posix()}": p.read_bytes()
        for p in sorted(SRC.rglob("*"))
        if p.is_file() and not p.name.startswith(".")
    }


def build():
    with zipfile.ZipFile(ZIP, "w", zipfile.ZIP_DEFLATED) as z:
        for name, data in wanted().items():
            info = zipfile.ZipInfo(name, STAMP)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            z.writestr(info, data)
    print(f"wrote {ZIP.relative_to(HERE.parent)}")


def check():
    if not ZIP.exists():
        sys.exit("udio-rescue-extension.zip is missing: run python3 udio-rescue/build-zip.py")
    with zipfile.ZipFile(ZIP) as z:
        have = {n: z.read(n) for n in z.namelist()}
    if have != wanted():
        sys.exit("udio-rescue-extension.zip is out of date: run python3 udio-rescue/build-zip.py and commit it")
    print("ok — udio-rescue-extension.zip matches extension/")


if __name__ == "__main__":
    check() if "--check" in sys.argv else build()
