# Live Cams

**A wall of public livestreams from cities in and near conflict zones.** Find
what's live right now in Kyiv, Jerusalem, Beirut and more, pin the streams you
want, and watch them side by side.

Open `index.html` in any browser. No build step, no server, no dependencies.
Your wall is kept in your browser (localStorage) and nothing else leaves it.
Works on a phone or a computer, in light or dark mode.

A standalone app: it is not part of MAZ ARCADE.

## How to use it

1. **Find what's live now.** Tap a city. YouTube opens with its *Live* filter
   on, so you only see streams that are on air at that moment: city webcams
   and people streaming themselves.
2. **Pin a stream.** Copy the stream's link, paste it into *Stream link*, give
   it a name and an area, and press **Pin to wall**.
3. **Watch.** Every stream plays muted in the grid. **Enlarge** opens one full
   size with sound. Filter the wall by area, and pick the tile size.

Links it understands: `youtube.com/watch?v=…`, `youtu.be/…`,
`youtube.com/live/…`, embed and shorts links, and channel links
(`youtube.com/channel/UC…`), which play whatever that channel has live.
Any other https page is pinned as a tile that opens in a new tab, because
most webcam sites don't allow their player inside another page.

## Why there's no built-in list of cameras

Webcam streams change address often and go dark without warning, so a fixed
list would quietly fill up with dead tiles. The city buttons always search what
is live *now*, and the wall keeps only what you chose.

## What it will not show

Only streams their owners chose to make public. It has no way to show private
or unsecured home or security cameras, and it won't get one: watching those
without the owner's permission is a privacy violation, and in a war zone a
live view of a street can be used to aim strikes.

## Files

| File | What it is |
|---|---|
| `js/livecams.js` | The engine: reading pasted links, the city list, the wall list, the save format. No DOM, so it is tested in plain Node. |
| `js/app.js` | The page: city buttons, the pin form, the wall, the full-size view. |
| `css/style.css` | The look: a dark control-room wall, light if the device asks. |
| `tests/livecams-logic.test.js` | `node livecams/tests/livecams-logic.test.js` |
