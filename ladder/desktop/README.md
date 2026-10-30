# Ladder 24 — desktop program

The same synthesizer as `ladder/index.html`, as a program with its own window
and icon for Windows, Mac and Linux. It works with no internet connection (the
panel's typefaces fall back to built-in ones offline) and talks to USB MIDI
keyboards directly.

## Getting the installer

GitHub builds the installers whenever anything in `ladder/` changes
(`.github/workflows/ladder-desktop.yml`). Open the repository's **Actions** tab,
choose the latest **Ladder 24 desktop** run, and download from **Artifacts**
at the bottom of the page:

| Download | For | File inside |
|---|---|---|
| Ladder 24 for Windows | Windows 10 or 11 | `Ladder-24-Setup-1.0.0.exe` |
| Ladder 24 for Mac | Apple-chip Macs (`arm64`) and Intel Macs (`x64`) | `Ladder-24-1.0.0-arm64.dmg`, `Ladder-24-1.0.0-x64.dmg` |
| Ladder 24 for Linux | Linux, 64-bit | `Ladder-24-1.0.0.AppImage` |

Each download is a `.zip`; open it to get the installer.

## Installing

The program isn't signed with a paid Microsoft or Apple certificate, so each
system asks once before the first run.

**Windows.** Double-click `Ladder-24-Setup-1.0.0.exe`. If a blue "Windows
protected your PC" box appears, click **More info**, then **Run anyway**. Follow
the installer; it puts Ladder 24 in the Start menu and on the desktop. It
installs for your user only and needs no administrator password.

**Mac.** Double-click the `.dmg` (`arm64` for an Apple-chip Mac, `x64` for an
Intel Mac; Apple menu → About This Mac says which). Drag **Ladder 24** into
**Applications**. The first time you open it, macOS says it can't check the
developer: click **Done**, open **System Settings → Privacy & Security**,
scroll down, click **Open Anyway** next to Ladder 24, and confirm. After that it
opens normally.

**Linux.** Make the file runnable (`chmod +x Ladder-24-1.0.0.AppImage`) and
double-click it.

## Building it yourself

Needs Node.js 22.

```sh
cd ladder/desktop
npm ci
npm start        # run it straight from the source
npm run dist     # build the installer for this computer into dist/
```

`npm run web` copies `ladder/index.html`, `css/` and `js/` into `web/`, which is
what the program loads (from `ladder://app/`, so the sound engine can run on
its own audio thread). `main.js` is the whole desktop shell: the window, that
`ladder://` address, and permission for MIDI keyboards.
