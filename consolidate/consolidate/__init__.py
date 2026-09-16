"""maz-consolidate — fold many repositories into one clean repository.

The package is deliberately split so that the thinking is testable without a
network, a GitHub token, or even git:

  models.py     plain dataclasses — a repo, where it lands, the whole plan
  discover.py   where the list of repos comes from (GitHub, a file, the CLI)
  plan.py       naming, collision handling and include/skip policy
  overlap.py    finds the duplicate work across repos
  layout.py     renders the generated files of the consolidated repo
  gitops.py     the only module that shells out to git
  build.py      orchestration; every step is reported, nothing is hidden
  cli.py        the human-facing commands (the only module needing typer/rich)
"""

__version__ = "0.1.0"
