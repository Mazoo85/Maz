/*
 * build-standalone.js — fold the app into a single self-contained HTML file.
 *
 * The multi-file version in this folder is the one to edit; this script is how
 * it becomes something you can hand to a person. It emits two things from the
 * same source, so they can never drift apart:
 *
 *   songforge.html          a complete page — save it anywhere, open it by
 *                           double-clicking, works with no network at all
 *   dist/songforge.body.html the same page without the <!doctype>/<html>/<head>
 *                           /<body> shell, for hosts that supply their own
 *
 * Run:  node music/build-standalone.js
 */
'use strict';

const fs = require('fs');
const path = require('path');

const DIR = __dirname;
const JS_ORDER = ['theory.js', 'genres.js', 'composer.js', 'synth.js', 'engine.js', 'export.js', 'app.js'];

const FONTS =
  '<link rel="preconnect" href="https://fonts.googleapis.com" />\n' +
  '<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin />\n' +
  '<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Chakra+Petch:wght@600;700' +
  '&family=IBM+Plex+Mono:wght@400;500&family=IBM+Plex+Sans:wght@400;500;600&display=swap" />';

const FAVICON =
  "<link rel=\"icon\" href=\"data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'>" +
  "<rect width='16' height='16' fill='%230b0714'/><rect x='3' y='9' width='3' height='4' fill='%23ff2d95'/>" +
  "<rect x='5' y='3' width='2' height='8' fill='%23ff2d95'/><rect x='10' y='7' width='3' height='6' fill='%2300e5ff'/>" +
  "<rect x='12' y='2' width='2' height='10' fill='%2300e5ff'/></svg>\" />";

function read(rel) {
  return fs.readFileSync(path.join(DIR, rel), 'utf8');
}

/* A literal "</script>" inside JS would close the tag early. Nothing in this
   codebase has one, but a future edit might, so neutralise it either way. */
function safeScript(src) {
  return src.replace(/<\/script>/gi, '<\\/script>');
}

function build() {
  const html = read('index.html');

  // Pull the page's own markup out of index.html: everything between <body> and
  // </body>, minus the script tags, which are inlined below instead.
  const bodyMatch = html.match(/<body[^>]*>([\s\S]*)<\/body>/i);
  if (!bodyMatch) throw new Error('index.html has no <body>');
  const markup = bodyMatch[1]
    .replace(/<script\b[\s\S]*?<\/script>/gi, '')
    .replace(/\n{3,}/g, '\n\n')
    .trim();

  const css = read(path.join('css', 'style.css'));
  const js = JS_ORDER.map(function (f) {
    return '/* ==== ' + f + ' ==== */\n' + safeScript(read(path.join('js', f)));
  }).join('\n\n');

  const title = 'SONG FORGE';
  const head = [
    '<title>' + title + '</title>',
    FONTS,
    '<style>\n' + css + '\n</style>'
  ].join('\n');

  const bodyOnly = [head, '', markup, '', '<script>\n' + js + '\n</script>', ''].join('\n');

  const standalone = [
    '<!DOCTYPE html>',
    '<html lang="en">',
    '<head>',
    '<meta charset="UTF-8" />',
    '<meta name="viewport" content="width=device-width, initial-scale=1.0, viewport-fit=cover" />',
    '<meta name="theme-color" content="#0b0714" />',
    '<meta name="description" content="SONG FORGE — a generative music maker that writes and plays complete songs in your browser. No install, no account, works offline." />',
    '<meta name="apple-mobile-web-app-capable" content="yes" />',
    '<meta name="apple-mobile-web-app-title" content="Song Forge" />',
    FAVICON,
    head,
    '</head>',
    '<body>',
    markup,
    '<script>',
    js,
    '</script>',
    '</body>',
    '</html>',
    ''
  ].join('\n');

  fs.writeFileSync(path.join(DIR, 'songforge.html'), standalone);
  fs.mkdirSync(path.join(DIR, 'dist'), { recursive: true });
  fs.writeFileSync(path.join(DIR, 'dist', 'songforge.body.html'), bodyOnly);

  const kb = function (s) { return (Buffer.byteLength(s) / 1024).toFixed(0) + ' KB'; };
  console.log('songforge.html            ' + kb(standalone) + '  (open this one)');
  console.log('dist/songforge.body.html  ' + kb(bodyOnly));
}

build();
