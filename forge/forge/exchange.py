"""Who depends on whom — read from shared/exchange.json.

The Forge's checks are only as honest as its knowledge of what a change can
break. `music/`'s own tests say nothing about SCRIPT FORGE, which loads five
of SONG FORGE's files; without this file the loop could break SCRIPT FORGE,
record `checks: green`, and open a pull request describing verified work.

Unlike every collector in `signals/`, this module **raises**. That is
deliberate and is the whole point. A signal that cannot read its source
should degrade to fewer candidates; a module that cannot tell you what a
change might break must not answer "nothing", because "nothing" is
indistinguishable from a correct answer and silently narrows what gets
verified. Callers decide what to do with the failure — see
`checks.all_commands` and `decide`.
"""

from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path

EXCHANGE_PATH = "shared/exchange.json"


class ExchangeError(Exception):
    """The declaration is missing, unreadable, or not the shape promised."""


@dataclass(frozen=True)
class Exchange:
    """The dependency graph, already validated."""

    # published id -> the project that publishes it
    publisher: dict
    # (consuming project, published id) pairs
    uses: tuple

    def consumers_of(self, project: str) -> tuple[str, ...]:
        """Projects that consume something `project` publishes.

        Direct consumers only, deliberately not transitive: if `film`
        consumed something `madlibs` published, and something else in turn
        consumed something `film` published, `consumers_of("madlibs")` would
        still name only `film`, never the project one hop further out.
        Unreachable today with the graph's single edge (music -> film), but
        the movie-maker spec already plans three more
        (`docs/superpowers/specs/2026-09-11-the-exchange-design.md`: a
        renderer upgrade, MADLIBS becoming the story brain, Maz Engine
        rendering the reel) — the first of those to chain onto an existing
        edge would otherwise recurse outward through the whole graph, and a
        declared cycle (A depends on B depends on A) would recurse forever
        rather than terminate. Widen this only alongside a cycle guard, not
        by accident.

        Sorted and deduplicated so a caller's command list is stable between
        runs: the ledger records what was run, and a set's iteration order
        would make two identical nights look different.
        """
        out = set()
        for consumer, published_id in self.uses:
            if self.publisher.get(published_id) == project and consumer != project:
                out.add(consumer)
        return tuple(sorted(out))


def load(root: Path) -> Exchange:
    """Read and validate the declaration. Raises `ExchangeError`, never returns empty on error."""
    path = root / EXCHANGE_PATH
    try:
        raw = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, ValueError) as exc:
        raise ExchangeError(f"{EXCHANGE_PATH}: {exc}") from exc

    if not isinstance(raw, dict):
        raise ExchangeError(f"{EXCHANGE_PATH}: top level is not an object")

    publishes = raw.get("publishes")
    consumes = raw.get("consumes")
    if not isinstance(publishes, dict):
        raise ExchangeError(f'{EXCHANGE_PATH}: "publishes" is not an object')
    if not isinstance(consumes, list):
        raise ExchangeError(f'{EXCHANGE_PATH}: "consumes" is not an array')

    publisher: dict = {}
    for published_id, entry in publishes.items():
        # The container being a dict says nothing about its values.
        if not isinstance(entry, dict):
            raise ExchangeError(f'{EXCHANGE_PATH}: published id "{published_id}" is not an object')
        project = entry.get("project")
        if not isinstance(project, str) or not project:
            raise ExchangeError(f'{EXCHANGE_PATH}: published id "{published_id}" names no project')
        publisher[published_id] = project

    uses: list = []
    for entry in consumes:
        if not isinstance(entry, dict):
            raise ExchangeError(f'{EXCHANGE_PATH}: a "consumes" entry is not an object')
        consumer = entry.get("project")
        published_id = entry.get("id")
        if not isinstance(consumer, str) or not consumer:
            raise ExchangeError(f'{EXCHANGE_PATH}: a "consumes" entry names no project')
        if not isinstance(published_id, str) or published_id not in publisher:
            raise ExchangeError(
                f'{EXCHANGE_PATH}: {consumer} consumes "{published_id}", which nothing publishes'
            )
        uses.append((consumer, published_id))

    return Exchange(publisher=publisher, uses=tuple(uses))


def is_loadable(root: Path) -> bool:
    """True if `load` would succeed. Never raises."""
    try:
        load(root)
    except ExchangeError:
        return False
    return True
