# The Forge's jobs

This is where you tell [**the Forge**](FORGE.md) — the repo's nightly loop —
what to work on. One line per job. Nothing else reads this file, and the Forge
reads nothing else for instructions.

To hand it a job, write a normal checklist item and put the file it should
change in backticks:

```markdown
- [ ] Add a volume slider to `music/js/player.js`
- [ ] Fix the broken recipe link in `scraper/recipes/README.md`
```

Tick the box (`- [x]`) when you no longer want it, or delete the line.

## Rules worth knowing before you write one

- **Write each job on a single line.** The parser reads one line per item, so
  a wrapped continuation is silently dropped and the job reaches the Forge as
  a truncated sentence.
- **The file must already exist**, spelled exactly as the repo spells it. A
  name it cannot find is treated as prose and the job is skipped — so a typo
  that lands on nothing costs you a quiet night. A typo that lands on a
  *different* real file in a safe zone is work it will happily do, so spell it
  right.
- **Naming a file is not permission to change it.** The safe zones in
  `forge.json` still decide, and `forge/` and `.github/workflows/` can never be
  touched at all. A job pointing at `engine/` is skipped every night, however
  you word it.
- **One job a night, as a draft pull request.** Nothing is merged for you.
- **Examples in fenced code blocks are ignored**, which is why the two lines
  above are illustrations rather than work. They also name files that do not
  exist, so they stay inert even if that rule ever breaks.
- **This file and `FORGE.md` are off limits to it.** The Forge cannot edit the
  file that decides what it works on.

## Why this is not in the roadmap

It was, as a "Phase 14" in `docs/ROADMAP.md`. Then the repo's two trunks were
unified, the merge kept the engine's roadmap, and the Forge's intake section —
along with the one job sitting in it — disappeared without anyone noticing.
`FORGE.md` went on pointing at a heading that no longer existed.

A file nobody else edits cannot be lost that way. `docs/ROADMAP.md` is still
read as context, but no job is expected to live there.

## Open

- [ ] Add direct unit tests for `scraper/scraper/robots.py` covering host keying, the cache, user-agent matching, and the fetch-failure case that must fall back to allowing the URL — it is currently only exercised indirectly, through the crawl tests

An empty list here means a quiet night, and a quiet night is a correct one.
