# The planes relay

Every free flight-data service (adsb.lol, adsb.fi, OpenSky, …) answers a server
but refuses a web page: their answers carry no permission (CORS header) for
anyone else's site. So GOD'S EYE VIEW's PLANES layer asks this relay instead. It
runs free on Cloudflare Workers, asks **adsb.fi** for the aircraft around a point
(**adsb.lol** if adsb.fi fails), and passes the answer back with permission for
`https://mazoo85.github.io` only — other sites are refused, so nobody else can
use up your free allowance.

It was found by asking each service from GitHub Actions the way a phone would
(21 Oct 2026): adsb.lol, adsb.fi and OpenSky all returned live aircraft but no
permission for our site; airplanes.live and adsb.one refused outright.

## Put it online (about 10 minutes, free, no card)

1. Open <https://dash.cloudflare.com/sign-up>, make a free account and confirm
   your email.
2. In the left menu open **Workers & Pages** (under *Compute*), then
   **Create** → **Start with Hello World!**.
3. Name it `godview-planes` and press **Deploy**.
4. Press **Edit code**. Select everything in the editor and delete it, then paste
   in the whole of [`worker.js`](worker.js) (open it, press *Raw*, select all,
   copy). Press **Deploy**.
5. Copy the address Cloudflare shows, which looks like
   `https://godview-planes.<your-name>.workers.dev`.
6. Check it is running: open that address with `/planes?lat=40.6&lng=-73.8`
   added on the end. You should see
   `{"error":"this relay only serves GOD'S EYE VIEW"}` — that is right: a browser
   tab is not the GOD'S EYE VIEW page, so it is turned away.
7. Put the address into `planesRelay` in [`../js/config.js`](../js/config.js)
   (or send it to Claude to do).

**If Deploy says "An unknown error occurred":** confirm your email first (Cloudflare
will not deploy for an unconfirmed account), use a computer rather than a phone
(the dashboard's code editor misbehaves on phones), and give a brand-new account a
few minutes. Or skip the editor altogether and let Cloudflare take the code from
GitHub: **Workers & Pages** → **Create** → **Import a repository** → connect GitHub
→ pick `Mazoo85/Maz` → set the root directory to `godview/relay` → **Deploy**.
[`wrangler.jsonc`](wrangler.jsonc) in this folder tells it what to build.

The free Workers plan allows 100,000 requests a day; one phone with PLANES on
uses about 240 an hour.

## Changing it

The code is one self-contained file so it can be pasted as it is. Its tests run
without a network: `node godview/tests/relay.test.mjs`. After changing it, paste
it into Cloudflare again (step 4).
