# consolidate

Folds many repositories into **one** repository — each project in its own
directory, every commit of its history intact — and tells you which of them
were doing the same job so you can stop maintaining two of anything.

It never pushes anywhere, never changes your original repositories, and does
nothing at all until you add `--yes`.

## The short version

```sh
pip install typer rich                  # one-time
cd consolidate
```

**If one repo is already your main one** (the usual case — it is how Maz is set
up), make it the home and fold the others into it:

```sh
python -m consolidate.cli adopt ~/my-main-repo --name "My Project" --yes
python -m consolidate.cli add   ~/my-main-repo --repo owner/other-repo --yes
```

**If you want a brand new repo instead**, built out of several existing ones:

```sh
python -m consolidate.cli report --user YOUR-GITHUB-NAME       # what am I doing twice?
python -m consolidate.cli build  ~/one-repo --user YOUR-GITHUB-NAME --yes
```

Either way it all happens on your own machine. Nothing is online until you
choose to push it, and the last thing it prints is the exact command for that.

## The commands

### `report` — find the work you are doing twice

```sh
python -m consolidate.cli report --user YOUR-GITHUB-NAME
```

Downloads a shallow copy of each repo (fast — one commit each, not the whole
history) and compares them. It tells you three things:

- **Projects that overlap** — "these two repos are 80% the same files"
- **The same file kept in more than one place** — byte-for-byte identical copies
- **Filenames that exist in several projects with different contents** — the
  dangerous ones: copies that drifted apart, where fixing one never fixes the other

This is the command that answers "I don't want several programs for the same
thing". Run it *before* you merge anything.

### `plan` — see what would happen

```sh
python -m consolidate.cli plan --user YOUR-GITHUB-NAME
```

Prints a table: every repository, the directory it would land in, and the
reason for anything left out. Nothing is downloaded or created.

Save it and run it later, unchanged:

```sh
python -m consolidate.cli plan  --user YOU --out plan.json
python -m consolidate.cli build ~/one-repo --from-plan plan.json --yes
```

### `build` — make the repository

```sh
python -m consolidate.cli build ~/one-repo --user YOUR-GITHUB-NAME        # dry run
python -m consolidate.cli build ~/one-repo --user YOUR-GITHUB-NAME --yes  # for real
```

Without `--yes` it prints every git command it *would* run and stops. With
`--yes` it runs exactly that list, then checks its own work: every project
present, every source commit reachable, nothing left uncommitted.

You get:

```
one-repo/
├── README.md            a table of every project and where it came from
├── CONSOLIDATION.md     original repo, branch and commit for each one
├── consolidate.json     the same, for the tool to read next time
└── projects/
    ├── alpha/           ← a whole repository, history and all
    ├── beta/
    └── gamma/
```

### `adopt` — make a repo you already have the home

If one of your repos is already the main one, you don't want a *new* repo —
you want that one to become the home everything else folds into:

```sh
python -m consolidate.cli adopt ~/my-main-repo --name "My Project"        # dry run
python -m consolidate.cli adopt ~/my-main-repo --name "My Project" --yes
```

Nothing in it moves. Every path, build and command that worked before still
works. It gains three small files:

- `PROJECTS.md` — an index of what's in here, existing folders included
- `CONSOLIDATION.md` — the record of anything folded in later
- `consolidate.json` — the same, for the tool to read next time

**Your own `README.md` is never touched.** Every file this tool generates carries
a marker in its first line, and a file without that marker is never overwritten —
so a repo with years of history in its front page keeps it. That check runs
*before* anything is written, and it will stop and tell you rather than guess.

### `add` — fold another repo into the home

```sh
python -m consolidate.cli add ~/my-main-repo --repo owner/name --yes
```

The home repo is never folded into itself, a repo already folded in is not
added twice, and a newcomer whose name clashes with a folder you already have
gets a different one.

### `check` — find duplication *inside* one repo you already have

Sometimes the several programs doing the same thing are already in the same
repository. Same comparison, pointed inwards:

```sh
python -m consolidate.cli check ~/my-repo              # every top-level directory
python -m consolidate.cli check ~/my-repo -d a -d b    # just these two
```

It ignores the filenames every project is supposed to have its own copy of —
`README.md`, `index.html`, `__init__.py`, `pyproject.toml` and friends — so
what is left is the code that genuinely drifted apart.

### `update` — pull in later changes

If you keep working in one of the original repos afterwards:

```sh
python -m consolidate.cli update ~/one-repo --yes          # all projects
python -m consolidate.cli update ~/one-repo alpha --yes    # just one
```

## How the history is kept

Copying files between repositories throws the history away. This tool uses
`git subtree` instead, which grafts the source repository's whole commit
graph under a subdirectory. Afterwards `git log` in the consolidated repo
shows every original commit, with its real author and date.

`CONSOLIDATION.md` records the commit each project was at when it was merged,
which is also the handle for reading that project's history on its own:

```sh
git log --oneline --graph                    # everything, interleaved
git log --oneline <commit from the table>    # exactly one project's history
```

Plain `git log projects/alpha/lib/util.js` shows only the merge commit — the
old commits remember the file at its original path, `lib/util.js`. That is
normal, and `CONSOLIDATION.md` in the built repo spells out the commands that
work.

## Choices it makes for you

| Situation | What happens | Change it with |
| --- | --- | --- |
| A **fork** of someone else's project | Left out — folding it in ends your ability to pull upstream fixes or send changes back | `--include-forks` |
| An **archived** repo | Folded in, and noted as archived | `--skip-archived` |
| An **empty** repo | Left out — there is nothing to fold in | — |
| Two repos with the **same name** | Second one gets its owner as a prefix | — |
| Folder is **not empty** | Refuses, so nothing of yours is clobbered | — |
| Repo has **uncommitted changes** | Refuses to update it | — |

Names are normalised the same way for everything: lowercase, hyphens, no
spaces. That is most of what "a clean format" means in practice.

## Private repositories

Public repos need nothing. For private ones, set a token first:

```sh
export GITHUB_TOKEN=ghp_your_token_here
```

## Using it without GitHub

Any list of repositories works, including local folders — which is also how
the tests drive it:

```sh
python -m consolidate.cli build ~/one-repo --repo owner/name --repo owner/other --yes
python -m consolidate.cli build ~/one-repo --from-json repos.json --yes
```

`repos.json` is just:

```json
{"repos": [{"owner": "me", "name": "alpha", "url": "/home/me/code/alpha"}]}
```

## Running the tests

```sh
cd consolidate && python -m pytest -q
```

163 tests. The ones in `tests/test_build.py` create real git repositories in a
temporary folder, consolidate them, and then go looking for every original
commit — because "keeps all your history" is a claim worth checking rather than
asserting.

## How it is put together

| File | What it does |
| --- | --- |
| `models.py` | the vocabulary: a repo, where it lands, the whole plan |
| `discover.py` | where the list of repos comes from (GitHub, a file, the CLI) |
| `plan.py` | naming, collisions, and the include/skip decisions |
| `overlap.py` | finds the duplicated work |
| `layout.py` | writes the generated index, record and manifest — and refuses to overwrite a file it did not write |
| `gitops.py` | the only module that shells out to git |
| `build.py` | runs the plan; every step is recorded, dry-run included |
| `cli.py` | the commands you type |

Everything except `gitops.py` and `cli.py` is pure Python with no network and
no subprocess, which is why most of the test suite runs in milliseconds.

It deliberately does not import from `forge/`, even though there is some
overlap in the git and GitHub helpers: this tool has to run against arbitrary
repositories on any machine, including outside this repo, so it stands alone.
