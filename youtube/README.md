# YouTube for Claude

**Lets Claude look things up on YouTube from any session**: web, phone, desktop or a
cloud session. Search for videos, see what's trending in a country, get a video's or a
channel's details and numbers, list a channel's or a playlist's videos, read comments.

It is **pointed at reusable videos**. Searches default to videos their owners marked
*Creative Commons* (CC BY), which may be reused with credit. Every video in every answer
says whether it is reusable and, when it is, carries a ready-made credit line. It is
read-only: it never downloads a video and never uploads anything. Uploading your own
videos, thumbnails and playlists stays with the YouTube actions in Zapier.

It is a tiny server (one file) that runs free on Cloudflare Workers, asks Google's
official YouTube Data API with an API key you create once, and speaks the Model Context
Protocol (MCP), which is how claude.ai talks to a *custom connector*.

A standalone project: it is not part of MAZ ARCADE and not built on the Maz engine.

## The easy way, on Windows (about 5 minutes)

Do step 1 below to get your YouTube key. Then open **PowerShell** (Start menu, type
`PowerShell`, press Enter), paste this line and press Enter:

```powershell
irm https://raw.githubusercontent.com/Mazoo85/Maz/claude/youtube-extension/youtube/setup.ps1 | iex
```

It asks for your key first: copy it from the Google Cloud Credentials page (**Show key**),
right-click in the PowerShell window to paste it, and press Enter.

[`setup.ps1`](setup.ps1) installs Node.js if it is missing, opens Cloudflare in your browser
so you can click **Allow** (make a free account there first if you have none), deploys the
server, stores your key and a new private password, checks that it all answers, copies the
connector address to your clipboard and opens the claude.ai Connectors page. Then do step 3.

## Put it online (about 15 minutes, free, no card)

### 1. Get a YouTube API key from Google

1. Open <https://console.cloud.google.com/> and sign in with any Google account.
2. At the top, open the project picker and press **New project**. Name it
   `youtube-for-claude` and press **Create**, then make sure it is selected.
3. In the left menu open **APIs & Services → Library**. Search for
   **YouTube Data API v3**, open it and press **Enable**.
4. Open **APIs & Services → Credentials**, press **Create credentials → API key**.
   Copy the key somewhere safe.
5. Press **Edit API key** (or the pencil next to it). Under *API restrictions* choose
   **Restrict key**, tick **YouTube Data API v3** only, and **Save**. Leave *Application
   restrictions* on **None** (the worker calls from Cloudflare, not a website).

Google gives 10,000 free "quota units" a day, resetting at midnight Pacific time. A search
costs about 100 units; everything else costs 1 to 3. That is roughly 100 searches a day,
or thousands of lookups. Every answer tells Claude how much it just spent.

### 2. Deploy the worker to Cloudflare

1. Open <https://dash.cloudflare.com/sign-up>, make a free account and confirm your
   email.
2. In the left menu open **Workers & Pages** (under *Compute*), then **Create** →
   **Start with Hello World!**.
3. Name it `youtube-for-claude` and press **Deploy**.
4. Press **Edit code**. Select everything in the editor and delete it, then paste in the
   whole of [`worker.js`](worker.js) (open it, press *Raw*, select all, copy). Press
   **Deploy**.
5. Open the worker's **Settings → Variables and Secrets** and add two **secrets**:
   - `YOUTUBE_API_KEY`: the key from step 1.
   - `CONNECTOR_PASSWORD`: a long private password you make up, letters and numbers only,
     for example `maz-yt-4q9v7k2p8x`. This keeps strangers from using up your free
     allowance. It goes in the connector address in step 3, so avoid spaces and symbols.
6. Copy the worker's address, which looks like
   `https://youtube-for-claude.<your-name>.workers.dev`.
7. Check it is running: open that address with `/check` on the end. You should see
   `"api_key_set": true`, `"password_set": true` and `"ok": true` under `youtube`. If
   not, the `error` line says what to fix in plain words.

**If Deploy says "An unknown error occurred":** confirm your email first, use a computer
rather than a phone, and give a brand-new account a few minutes. Or let Cloudflare take
the code from GitHub instead: **Workers & Pages → Create → Import a repository** →
connect GitHub → pick `Mazoo85/Maz` → set the root directory to `youtube` → **Deploy**.
[`wrangler.jsonc`](wrangler.jsonc) in this folder tells it what to build. Secrets are still
added by hand (step 5).

### 3. Connect it to Claude

1. Open <https://claude.ai/settings/connectors> (Settings → Connectors).
2. Press **Add custom connector**.
3. Name: `YouTube`. Remote MCP server URL: your worker's address plus `/mcp/` plus your
   password, for example
   `https://youtube-for-claude.<your-name>.workers.dev/mcp/maz-yt-4q9v7k2p8x`.
   Leave the OAuth fields empty.
4. Press **Add**. In a new chat, YouTube appears under the connectors menu; make sure it is
   switched on. Try: *"What's trending on YouTube in the UK right now?"*

The password is part of the address, so treat that address like a password: don't post it
anywhere. If it leaks, change the `CONNECTOR_PASSWORD` secret and update the connector.

## What Claude can do with it

| Tool | What it does | Costs |
|---|---|---|
| `search_videos` | Find videos by words, with filters for order (relevance, date, views, rating), length, date, channel and country. Creative Commons only unless told `license: any`. | ~101 units |
| `trending_videos` | The most popular videos right now in a country, optionally one category (10 Music, 20 Gaming, 24 Entertainment, 28 Science & Tech). `reusable_only` keeps just the Creative Commons ones. | 1 |
| `get_video` | Title, channel, length, views, likes, comment count, license, reusable or not, credit line. Takes an id or any YouTube link. | 1 |
| `get_channel` | Title, subscribers, video and view counts. Takes a channel id, an `@handle` or a link. | 1 |
| `list_channel_videos` | A channel's newest uploads with full details. Creative Commons only unless told `license: any`. | 3 |
| `list_playlist_videos` | The videos in a public playlist with full details, same filter. | 2 |
| `get_comments` | Top-level comments on a video, by relevance or newest first. | 1 |

Every video comes back in the same shape:

```json
{
  "id": "dQw4w9WgXcQ",
  "url": "https://www.youtube.com/watch?v=dQw4w9WgXcQ",
  "title": "…",
  "channel": { "id": "UC…", "title": "…", "url": "https://www.youtube.com/channel/UC…" },
  "published": "2026-01-02T03:04:05Z",
  "duration": { "seconds": 212, "text": "3:32" },
  "views": 1200, "likes": 34, "comments": 5,
  "license": "creativeCommon",
  "reusable": true,
  "credit": "Video by … (https://www.youtube.com/watch?v=…), licensed CC BY",
  "description": "…"
}
```

## About reusing videos

A Creative Commons mark means the owner allows others to reuse the video **with credit**
(the `credit` line is there to paste). YouTube's own Studio editor can also pull Creative
Commons videos straight into a remix. Two things to know before building a channel on it:

- Reusing someone's video unchanged, or with little added, counts as *reused content*
  under YouTube's rules: such channels are refused monetisation and can be removed. Add
  your own commentary, editing or point of view.
- Everything else on YouTube is copyrighted. Re-uploading it is infringement, YouTube's
  matching catches it within minutes, and three strikes close the channel. This tool
  cannot download videos and will not be changed to.

## Files

| File | What it is |
|---|---|
| `worker.js` | The whole server: link reading, the seven tools, the MCP handshake, the password check, `/check`. Self-contained so it can be pasted as it is. |
| `wrangler.jsonc` | So Cloudflare can deploy it straight from GitHub. |
| `setup.ps1` | The easy way on Windows: deploys to Cloudflare and stores the secrets for you. |
| `tests/worker.test.mjs` | `node youtube/tests/worker.test.mjs`, with a fake YouTube, no network or key needed. |

## Changing it

Edit `worker.js`, run the test, then paste the file into Cloudflare again (step 2.4).
The design is in `docs/superpowers/specs/2026-10-24-youtube-extension-design.md`.
