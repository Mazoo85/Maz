# Archive shorts

**Turns public-domain and openly licensed footage into YouTube Shorts.** It searches the
[Internet Archive](https://archive.org/details/movies) and
[Wikimedia Commons](https://commons.wikimedia.org/) (old films, newsreels, NASA, NOAA and
other US government footage), keeps only videos whose license allows editing and
re-uploading, and cuts the scenes you choose into a vertical short with captions. It also
writes the upload details, with a credit for every source added automatically.

Part of [YouTube for Claude](../README.md). Claude does the creative part (picking a topic
that is popular on YouTube right now, choosing scenes, writing captions and the title);
this does the mechanical part.

## Which footage it will use

| License | Used? | Why |
|---|---|---|
| Public domain, CC0, Public Domain Mark | Yes | Free for anyone, for anything |
| CC BY, CC BY-SA | Yes | Free with credit, which it adds |
| CC BY-NC (NonCommercial) | No | A channel may earn money |
| CC BY-ND (NoDerivatives) | No | A short is an edit |
| No license stated | No | That means all rights reserved |

Videos on YouTube itself are never used: YouTube's terms forbid downloading them.

## Using it

Needs Node.js 18+ and ffmpeg.

```sh
node youtube/shorts/cli.mjs search "eiffel tower"         # usable videos, with license and length
node youtube/shorts/cli.mjs make plan.json --out out       # out/short.mp4 and out/upload.json
```

A plan names the sources, the scenes and the words. A source can also be any other
public-domain video, as long as its page is given for the credit:

```json
{
  "hook": "THE TOWER PARIS HATED",
  "end": "WOULD YOU HAVE BUILT IT?",
  "sources": {
    "fair": { "file": "https://archive.org/download/<id>/<file>.mp4", "title": "Paris Exposition",
              "year": "1900", "license": "public domain", "source": "Internet Archive",
              "page": "https://archive.org/details/<id>" }
  },
  "scenes": [
    { "source": "fair", "start": 12.5, "length": 4, "caption": "In 1887, 300 workers began" }
  ],
  "upload": { "title": "The Tower Paris Hated #shorts", "description": "…",
              "tags": ["eiffel tower", "history"], "category": "Education" }
}
```

The short is 1080×1920: each clip sits on a blurred, full-height copy of itself so old
4:3 film fills a phone screen, with its caption below. A hook card opens it and a question
card closes it. The film's own sound is dropped. Shorts are capped at 60 seconds and each
scene at 15.

`upload.json` holds everything the upload needs: title, description with credits, tags,
category, language, made for kids (no), privacy (private, so it can be checked first),
license and notifications.

## Tests

`node youtube/shorts/tests/shorts.test.mjs` checks the license rules, both sites' answers,
the plan checks and the upload details with no network, and makes a real short end to end
from a generated test clip when ffmpeg is installed.
