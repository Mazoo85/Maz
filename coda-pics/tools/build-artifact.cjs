/* Assembles the CODA PICS artifact straight from the real app: the same body,
   the same stylesheet, the same engine and the same app.js. The only additions
   are the two hooks the app already offers a host — no worker script to load
   here, and downloads go through the viewer's own save prompt — plus an opening
   picture, because a page that shows what it does beats one that asks first. */
const fs = require('fs'), path = require('path');
const R = path.join(__dirname, '..');
const read = p => fs.readFileSync(path.join(R, p), 'utf8');

const css = read('css/style.css');
const engine = ['js/lexicon.js','js/photo.js','js/prompt.js','js/subjects.js','js/paint.js','js/finish.js','js/app.js']
  .map(f => `/* ---- ${f} ---- */\n` + read(f)).join('\n');

/* The page body, minus the arcade nav and the service worker: neither exists
   inside the viewer. The footer's relative links become absolute. */
let html = read('index.html');
let body = html.slice(html.indexOf('<div id="app">'), html.indexOf('</body>'));
body = body.replace(/<script[\s\S]*?<\/script>/g, '');
body = body.replace('href="../index.html"', 'href="https://mazoo85.github.io/Maz/" target="_blank" rel="noopener"');
body = body.replace('href="README.md"', 'href="https://github.com/Mazoo85/Maz/tree/main/coda-pics" target="_blank" rel="noopener"');

/* The picture leads here: inside a gallery the first frame is the whole
   impression, and a column of controls above it shows nothing of what the page
   makes. */
{
  const stageStart = body.indexOf('<section class="stage"');
  const stageEnd = body.indexOf('<section class="gallery-wrap"');
  const panelStart = body.indexOf('<section class="panel">');
  const panelEnd = body.indexOf('<main>');
  if (stageStart > 0 && stageEnd > stageStart && panelStart > 0 && panelEnd > panelStart) {
    const stage = body.slice(stageStart, stageEnd);
    const panel = body.slice(panelStart, panelEnd);
    body = body.slice(0, panelStart) + body.slice(panelEnd);
    const at = body.indexOf('<section class="gallery-wrap"');
    body = body.slice(0, at) + panel + body.slice(at);
    const from = body.indexOf('<section class="stage"');
    const to = body.indexOf('<section class="panel">');
    body = body.slice(0, from) + body.slice(from + stage.length);
    body = body.slice(0, body.indexOf('<main>') + '<main>'.length) + '\n' + stage +
      body.slice(body.indexOf('<main>') + '<main>'.length);
  }
}

/* The Google Photos box cannot work here and must not pretend to: an artifact
   is sandboxed, so the page may not reach accounts.google.com at all, and a
   button that silently does nothing is worse than no button. It is replaced
   with a line saying where it does work. Choosing a photo off the device is
   untouched — that never needed a network. */
{
  const from = body.indexOf('<details id="gphotos"');
  const to = body.indexOf('</details>', from);
  if (from > 0 && to > from) {
    body = body.slice(0, from) +
      '<p class="photo-privacy">Photos from <b>Google Photos</b> can be brought in too, but only on ' +
      'the real page — this preview is sandboxed and cannot reach Google. ' +
      '<a href="https://mazoo85.github.io/Maz/coda-pics/" target="_blank" rel="noopener">Open CODA PICS</a> ' +
      'to use it.</p>' +
      body.slice(to + '</details>'.length);
  } else {
    throw new Error('the Google Photos box was not found — index.html changed shape');
  }
}

const extra = fs.readFileSync(path.join(__dirname, 'artifact-extra.css'), 'utf8');
const adapter = fs.readFileSync(path.join(__dirname, 'artifact-adapter.js'), 'utf8');

const out = `<title>CODA PICS</title>
<style>
${css}
${extra}
</style>

${body}

<script>
${adapter}
</script>
<script>
${engine}
</script>
`;
const dest = process.argv[2];
if (!dest) {
  console.error('usage: node coda-pics/tools/build-artifact.cjs <out.html>');
  process.exit(2);
}
fs.writeFileSync(dest, out);
console.log('wrote', dest, (out.length/1024).toFixed(0)+'KB');
