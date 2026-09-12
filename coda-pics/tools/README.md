# coda-pics/tools

`build-artifact.cjs` packs the whole app into one self-contained HTML file, for
hosts that serve a single page — a Claude artifact, an email attachment, a USB
stick. It reads the real `index.html`, `css/style.css` and the engine files, so
there is only ever one copy of CODA PICS and this cannot drift from it.

```sh
node coda-pics/tools/build-artifact.cjs out.html
```

Three things differ in the packed copy, and all three are honest about it:

- **No worker.** There is no second file to fetch, so the app paints on the main
  thread (`CODA_NO_WORKER`, a hook the app already offers a host).
- **Saving goes through the host.** A sandboxed page cannot start its own
  download, so files are handed to the viewer's save prompt (`CODA_SAVE`), and
  the save buttons stay hidden where even that is unavailable.
- **No Google Photos.** A sandboxed page may not reach Google at all, so the box
  is replaced with a line saying where it does work. A button that silently does
  nothing is worse than no button — and the build *fails* rather than quietly
  shipping one if it cannot find that box, which is what stops this file rotting
  the next time `index.html` changes shape.

Choosing a photo off the device is untouched: that never needed a network.
