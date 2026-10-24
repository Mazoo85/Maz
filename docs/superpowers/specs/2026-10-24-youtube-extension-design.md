# YouTube extension for Claude (`youtube/`) — design

**Date:** 2026-10-24 · **Status:** approved in conversation

## What it is

A small server that lets Claude use YouTube from any session: web, phone, desktop or a cloud
session. It speaks the Model Context Protocol (MCP) over HTTPS, so it is added to claude.ai as a
**custom connector**. It runs free on Cloudflare Workers, the same way as the planes relay in
`godview/relay/`, and asks Google's official **YouTube Data API v3** with an API key Cody creates
once.

It is **read-only** and **pointed at reusable videos**: searches default to videos whose owners
marked them Creative Commons (CC BY), which may be reused with credit. Every result says whether
the video is reusable and carries a ready-made credit line. Uploading, playlists and anything else
that writes to Cody's own channel stays with the Zapier YouTube actions already enabled.

What it will not do: download videos, or help re-upload other people's videos. That is copyright
infringement and against YouTube's terms, and such a channel is terminated after three strikes.

## Decisions made

| Question | Decision |
|---|---|
| Where it runs | Cloudflare Workers (free, no card), one self-contained `worker.js` pasted into the dashboard or imported from GitHub |
| How Claude reaches it | claude.ai → Settings → Connectors → Add custom connector, with the worker's address |
| Who may use it | Only requests carrying Cody's private password, in the address (`/mcp/<password>`) or as a bearer token. Strangers get 401 and cannot use up the free quota |
| Data source | YouTube Data API v3 with an API key, stored as a Worker secret (`YOUTUBE_API_KEY`); the password is a second secret (`CONNECTOR_PASSWORD`) |
| Writes | None. Zapier covers upload / thumbnail / playlist |
| Default license filter | `creative_commons`; every tool that can filter accepts `license: "any"` to widen |
| Place in the repo | Standalone `youtube/` folder, not in MAZ ARCADE, not on the Maz engine, own CI workflow |

## Tools Claude gets

Every video in any answer has the same shape: `id`, `url`, `title`, `channel` (`{id, title, url}`),
`published`, `duration` (seconds and `h:mm:ss`), `views`, `likes`, `comments`, `license`
(`creativeCommon` or `youtube`), `reusable` (true only for Creative Commons) and, when reusable,
`credit` ("Video by <channel> (<url>), licensed CC BY").

| Tool | What it does | Inputs | API cost |
|---|---|---|---|
| `search_videos` | Find videos | `query`; optional `license` (creative_commons / any), `order` (relevance / date / views / rating), `duration` (any / short / medium / long), `published_after`, `channel_id`, `region_code`, `max_results` (1–25) | 100 + 1 |
| `trending_videos` | What is popular now | optional `region_code` (default US), `category_id`, `reusable_only` (default false, because the chart cannot be filtered server-side and is usually empty for CC), `max_results` | 1 |
| `get_video` | Details of one video | `video` (id or any YouTube link) | 1 |
| `get_channel` | Details of a channel | `channel` (id, `@handle`, or link) → id, title, url, description, subscribers, video and view counts, uploads playlist id | 1 |
| `list_channel_videos` | A channel's latest uploads | `channel`, optional `license`, `max_results` (1–50) | 1 + 1 + 1 (channel → uploads playlist → video details) |
| `list_playlist_videos` | Videos in a public playlist | `playlist` (id or link), optional `license`, `max_results` | 1 + 1 |
| `get_comments` | Top-level comments | `video`, optional `order` (relevance / time), `max_results` (1–50) | 1 |

Google's free quota is 10,000 units a day, resetting at midnight Pacific time: roughly 100 searches,
or thousands of lookups. Each answer includes `quota_used` so Claude can pace itself.

## Protocol

- `POST /mcp/<password>` (or `POST /mcp` with `Authorization: Bearer <password>`): MCP over
  Streamable HTTP, JSON-RPC 2.0, plain JSON responses (no server-sent events needed for these
  short calls). Handles `initialize`, `notifications/initialized` (202, empty), `ping`,
  `tools/list`, `tools/call`. Unknown methods get JSON-RPC error −32601; bad JSON −32700.
- Tool failures are returned as tool results with `isError: true` and a plain sentence, so Claude
  can tell Cody what happened: wrong or unrestricted key, YouTube Data API not enabled, daily
  quota used up, no such video / channel / playlist, comments turned off.
- `GET /check`: open to anyone, says whether the key and password are set and whether YouTube
  answers (one 1-unit call). Reveals no secrets.
- Anything else: 404 with a hint. `GET /mcp` is 405 (no event stream offered).

## Structure

```
youtube/
  README.md              what it is, the three setup steps, what each tool does, how to change it
  worker.js              the whole server, self-contained (pasteable), pure helpers exported for tests
  wrangler.jsonc         so Cloudflare can deploy it straight from GitHub
  tests/worker.test.mjs  node youtube/tests/worker.test.mjs — fake fetch, no network
.github/workflows/youtube.yml   runs the test on pushes touching youtube/
```

`worker.js` is split inside into pure functions (link parsing, duration parsing, result shaping,
quota arithmetic, the MCP dispatch) and one thin `fetch` handler, so the tests cover every path:
password checks, each tool's happy path and each error sentence, link forms (`watch?v=`, `youtu.be`,
`shorts/`, `live/`, `@handle`, `channel/UC…`, `playlist?list=`), and the license filter.

## Setup Cody does once (about 15 minutes)

1. **API key:** Google Cloud console → new project → *APIs & Services* → enable *YouTube Data API
   v3* → *Credentials* → *Create credentials* → *API key* → restrict it to the YouTube Data API.
2. **Deploy:** Cloudflare → Workers & Pages → Create → Hello World → paste `worker.js` → Deploy;
   then *Settings → Variables and Secrets*: add `YOUTUBE_API_KEY` and `CONNECTOR_PASSWORD` as
   secrets. Open `/check` to confirm.
3. **Connect:** claude.ai → Settings → Connectors → Add custom connector → name "YouTube", URL
   `https://<worker>.workers.dev/mcp/<password>`.

## Testing

`node youtube/tests/worker.test.mjs`, dependency-free, same style as `godview/tests/relay.test.mjs`.
The repo gates `node scripts/check-links.mjs` and `node scripts/check-exchange.mjs` must stay green.
