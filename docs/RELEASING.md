# Releasing & Distributing a Maz Game

This covers cutting an engine release and getting a finished game to players.

## Cut a release (automated)

1. Update `CHANGELOG.md` and bump the version (`core::Version` / `CMakeLists.txt` project version).
2. Tag it and push:
   ```bash
   git tag v1.2.3
   git push origin v1.2.3
   ```
3. `.github/workflows/release.yml` fires: it builds with warnings-as-errors, runs the **full ctest
   suite**, packages the sample games with `tools/package.sh`, and creates a GitHub Release with the
   `.tar.gz` bundles attached and auto-generated notes.

Trigger it manually (without a tag) from the Actions tab via **workflow_dispatch**, optionally choosing
which app targets to package.

## Package a single game by hand

```bash
cmake -S . -B build && cmake --build build          # build first
tools/package.sh <app> <version>                     # e.g. tools/package.sh zomboid 1.0.0
tools/package.sh --list                              # what's packageable
```

This produces `dist/<app>-<version>-<os>-<arch>.tar.gz` — a self-contained bundle (executable + SDL3
runtime + compiled shaders + assets + a launcher) that it then **verifies** by running the packaged
launcher from a scratch directory. A player downloads it, extracts, and runs the launcher; no engine,
toolchain, or install needed.

## Publishing to itch.io (the common indie path)

The bundle from `tools/package.sh` is exactly what itch.io wants. Recommended with `butler` (itch's CLI):

```bash
# one-time: install butler and `butler login`
butler push dist/mygame-1.0.0-linux-x86_64.tar.gz  <user>/<game>:linux
# do the same for windows/mac bundles built on those OSes, e.g. :windows, :osx
```

Players then get one-click download/updates through the itch app. Steam works similarly via `steamcmd`
+ the app depot, once you have a Steamworks account.

## What this repo can and can't do for you

- **Automated here:** the release pipeline (build → test → package → attach bundles) and the
  itch/Steam upload commands above are real and runnable.
- **Needs you (the honest human step — not markable as "done" by the engine):**
  - **Real players & mileage.** An engine earns trust by shipping games that real people play over time.
    That can only accumulate by you (and others) actually releasing games with it.
  - **A community.** Discussions, third-party plugins, tutorials by other people, bug reports from
    strangers — these require real humans choosing to show up. The scaffolding here (issue/PR templates,
    `CONTRIBUTING.md`, `CODE_OF_CONDUCT.md`, the plugin system) lowers the barrier, but the people are
    yours to invite.
  - **Store accounts.** itch.io is free; Steam, console, and mobile stores need paid developer accounts
    and their own review processes.
