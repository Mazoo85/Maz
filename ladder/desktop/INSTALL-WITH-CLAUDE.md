# Installing Ladder 24 with Claude on your computer

Paste everything in the box below into **Claude Code running on your computer**
(the Claude desktop app's Code tab, or `claude` in a terminal). It downloads
the newest Ladder 24 installer that GitHub built, closes the old version,
installs the new one over it, and checks it worked. Your saved sounds,
favourites and songs are kept: they live outside the program.

```text
Please install the newest Ladder 24 desktop program on this computer, replacing
the version that is already installed. Ladder 24 is my own synthesizer app; its
source is the GitHub repository Mazoo85/Maz, branch claude/ladder-24-synth,
folder ladder/desktop. GitHub Actions builds its installers in the workflow
"Ladder 24 desktop" (.github/workflows/ladder-desktop.yml).

1. Work out whether this computer is Windows or a Mac (and on a Mac, Apple chip
   arm64 or Intel x64).

2. Get the installer from the newest successful run of that workflow on branch
   claude/ladder-24-synth. The run I know of is
   https://github.com/Mazoo85/Maz/actions/runs/41257298841 (version 1.2.0); use a
   newer successful run if there is one. If the GitHub CLI is installed and signed
   in, use it, for example:
     gh run download <run id> -R Mazoo85/Maz -n "Ladder 24 for Windows" -D "%TEMP%\ladder24"
     gh run download <run id> -R Mazoo85/Maz -n "Ladder 24 for Mac" -D /tmp/ladder24
   If gh isn't available or signed in, ask me to download that artifact from the
   run page in my browser and tell you where the file landed. If the artifacts have
   expired, build it instead: clone the branch, then in ladder/desktop run
   "npm ci" and "npm run dist" (needs Node.js 22) and use the installer in dist/.

3. Close Ladder 24 if it is running.
     Windows: taskkill /IM "Ladder 24.exe" /F   (ignore "not found")
     Mac:     osascript -e 'quit app "Ladder 24"'   (ignore errors)

4. Install it over the old one.
     Windows: run Ladder-24-Setup-<version>.exe with /S for a silent install
       (it installs for my user only, no administrator needed, and replaces the
       old version in place). If Windows SmartScreen blocks it, tell me to click
       "More info" then "Run anyway".
     Mac: pick the .dmg matching the chip (arm64 or x64), then
       hdiutil attach the .dmg, copy "Ladder 24.app" into /Applications
       replacing the old one (rm -rf the old app first, then ditto), detach the
       disk image, and run: xattr -dr com.apple.quarantine "/Applications/Ladder 24.app"
       so macOS doesn't block it.

5. Check it worked: confirm the installed version matches the installer
   (Windows: the version in the uninstall entry for "Ladder 24"; Mac:
   CFBundleShortVersionString in "/Applications/Ladder 24.app/Contents/Info.plist"),
   then open Ladder 24 and tell me it's ready. In the new version the top
   section menu lists "Layers & Splits (30)".

Don't delete anything in my user data or app-data folders: my saved sounds live
there. Tell me plainly if any step fails and what you saw.
```

## Doing it by hand instead

The same steps without Claude are in [README.md](README.md#installing).
