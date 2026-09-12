# NAME MAKER

Press a button, get a name: one adjective, one noun. A thousand of each, so a
million possible names — and none of them ever repeat until you have seen them
all.

### ▶ [Open it](https://mazoo85.github.io/Maz/namemaker/) · [back to MAZ ARCADE](../index.html)

- **One file.** The whole app — styles, the two word banks, the logic — is
  `index.html`. Nothing to build, nothing to load.
- **Installable.** It carries a web manifest and a service worker, so a phone
  can add it to the home screen and it runs with no signal at all.
- **Copy button**, and a list of the last names you made.

## Where it came from

This app was written on its own branch (`claude/random-name-generator-pn59hd`)
and, until now, was the *only* thing GitHub Pages had ever published for this
repository — the site had never been pointed at the branch the arcade lives on.
Moving it in here means it keeps its address on the live site once Pages is
pointed at the right branch, instead of disappearing with the old one.

What changed in the move, and nothing else did:

- `name-maker.html` became `namemaker/index.html`, so it opens at
  `/namemaker/` like every other project here;
- the manifest, icon and service-worker paths were made explicit (`./…`) and the
  cache version bumped, since the app now lives one folder down;
- the shared MAZ ARCADE nav pill was added, so it is not a dead end.

The word banks, the layout and the behaviour are untouched.
