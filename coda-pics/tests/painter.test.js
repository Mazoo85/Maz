/*
 * CODA PICS — the published painter surface.
 *
 *   node coda-pics/tests/painter.test.js
 *
 * coda-logic.test.js checks that CODA PICS paints. This checks the smaller
 * thing it lends to other projects, and in particular the one behaviour that
 * exists only for them: knowing when it did not understand.
 *
 * On its own page, inventing is right — a blank page is never a blank page, so
 * words CODA PICS does not know still produce a picture. Lent out, that is a
 * trap: "a police station" paints a knight in the open sky and the caller
 * cannot tell that from a picture of what it asked for. These tests pin both
 * halves: paint() always paints and says how much it understood;
 * paintIfRecognised() refuses rather than guessing.
 */
'use strict';

var path = require('path');
var PAINTER = require(path.join(__dirname, '..', 'js', 'painter.js'));
var PROMPT = require(path.join(__dirname, '..', 'js', 'prompt.js'));
var FakeContext = require(path.join(__dirname, 'fake-canvas.js')).FakeContext;

var failures = 0;
var checks = 0;

function check(cond, msg) {
  checks++;
  if (!cond) { failures++; console.log('  FAIL  ' + msg); }
}
function section(name) { console.log('\n' + name); }
function pass(msg) { console.log('  ok    ' + msg); }

/* Prompts CODA PICS genuinely knows, and prompts it genuinely does not. */
var KNOWN = [
  'a red dragon over snowy mountains at sunset',
  'a lighthouse on a stormy sea',
  'a whale in deep space',
  'a castle in the mountains at dawn'
];
var UNKNOWN = ['a police station', 'a stairwell', 'qwertyuiop', '', '   '];

/* ------------------------------------------------------------ 1. reading */
section('Reading a prompt before painting it');
(function () {
  for (var i = 0; i < KNOWN.length; i++) {
    var r = PAINTER.read(KNOWN[i], { seed: 4 });
    check(!!r, 'read returned nothing for "' + KNOWN[i] + '"');
    check(r.grounded.subject === true, '"' + KNOWN[i] + '" named a subject but was not credited with one');
    check(r.grounded.ratio > 0.3, '"' + KNOWN[i] + '" read only ' + Math.round(r.grounded.ratio * 100) + '%');
    check(typeof r.describe === 'string' && r.describe.length > 0, 'no description for "' + KNOWN[i] + '"');
  }
  pass(KNOWN.length + ' prompts it knows are reported as understood');

  for (var j = 0; j < UNKNOWN.length; j++) {
    var u = PAINTER.read(UNKNOWN[j], { seed: 4 });
    check(!!u, 'read threw or returned nothing for ' + JSON.stringify(UNKNOWN[j]));
    check(u.grounded.subject === false,
      JSON.stringify(UNKNOWN[j]) + ' was credited with naming a subject it never named');
    check(u.grounded.invented.length > 0, JSON.stringify(UNKNOWN[j]) + ' invented nothing, which cannot be right');
  }
  pass(UNKNOWN.length + ' prompts it does not know are reported as invented');

  check(PAINTER.read('a dragon', { seed: 1 }).grounded.ratio ===
        PAINTER.read('a dragon', { seed: 1 }).grounded.ratio, 'reading is not repeatable');
  pass('reading the same prompt twice reads it the same way');
})();

/* ------------------------------------------------------------ 2. grounding */
section('What the grounding summary counts');
(function () {
  var full = PROMPT.parse('a red dragon over snowy mountains at sunset', { seed: 1 }).grounded;
  check(full.parts > 0, 'no parts counted at all');
  check(full.fromWords <= full.parts, 'more parts came from the words than there were parts');
  check(Math.abs(full.ratio - full.fromWords / full.parts) < 1e-9, 'the ratio does not match the counts');
  check(full.invented.length === full.parts - full.fromWords, 'the invented list does not match the counts');
  pass('the counts, the ratio and the invented list all agree');

  var bare = PROMPT.parse('', { seed: 1 }).grounded;
  check(bare.fromWords === 0, 'an empty prompt was credited with saying something');
  check(bare.ratio === 0, 'an empty prompt scored above zero');
  pass('an empty prompt reads as nothing understood, not as an error');
})();

/* ------------------------------------------------------------ 3. painting */
section('Painting');
(function () {
  var ctx = new FakeContext(320, 200);
  var out = PAINTER.paint(ctx, 320, 200, KNOWN[0], { seed: 5 });
  check(!!out && out.painted === true, 'paint did not report painting');
  check(ctx.calls > 20, 'almost nothing was drawn (' + ctx.calls + ' calls)');
  check(out.grounded.subject === true, 'the grounding was lost on the way through paint');
  pass('a known prompt paints a picture and reports what it understood');

  var ctx2 = new FakeContext(320, 200);
  var invented = PAINTER.paint(ctx2, 320, 200, 'a police station', { seed: 5 });
  check(invented.painted === true, 'paint refused — paint() must always paint, that is what it is for');
  check(invented.grounded.subject === false, 'paint claimed to have understood a police station');
  check(ctx2.calls > 20, 'the invented picture was not actually painted');
  pass('an unknown prompt still paints, and says it was invented');

  var ctx3 = new FakeContext(320, 200);
  var styled = PAINTER.paint(ctx3, 320, 200, KNOWN[1], { seed: 5, style: 'pixel' });
  check(styled.style === 'pixel', 'the requested style was ignored (got ' + styled.style + ')');
  pass('a caller can ask for a particular art style');

  var ctx4 = new FakeContext(320, 200);
  var bogus = PAINTER.paint(ctx4, 320, 200, KNOWN[1], { seed: 5, style: 'not-a-style' });
  check(bogus && bogus.painted === true, 'an unknown style stopped it painting instead of being ignored');
  pass('an unknown style is ignored rather than fatal');

  check(PAINTER.paint(null, 320, 200, 'a dragon') === null, 'no context should return null, not throw');
  check(PAINTER.paint(new FakeContext(320, 200), 0, 0, 'a dragon') === null, 'a zero-sized canvas should return null');
  pass('a bad call returns null rather than throwing into the caller');

  check(PAINTER.styles().length > 5, 'only ' + PAINTER.styles().length + ' styles are offered');
  pass('every art style CODA PICS has is offered to a consumer');
})();

/* -------------------------------------------------- 4. refusing to guess */
section('Refusing rather than guessing');
(function () {
  for (var i = 0; i < KNOWN.length; i++) {
    var ctx = new FakeContext(320, 200);
    var ok = PAINTER.paintIfRecognised(ctx, 320, 200, KNOWN[i], { seed: 6 });
    check(ok.painted === true, 'refused a prompt it understood: "' + KNOWN[i] + '"');
    check(ctx.calls > 20, 'said it painted "' + KNOWN[i] + '" but drew nothing');
  }
  pass(KNOWN.length + ' understood prompts are painted');

  for (var j = 0; j < UNKNOWN.length; j++) {
    var c = new FakeContext(320, 200);
    var no = PAINTER.paintIfRecognised(c, 320, 200, UNKNOWN[j], { seed: 6 });
    check(no.painted === false, 'painted ' + JSON.stringify(UNKNOWN[j]) + ' anyway');
    check(c.calls === 0,
      'refused ' + JSON.stringify(UNKNOWN[j]) + ' but drew on the canvas regardless — a caller ' +
      'falling back to its own artwork would get both on top of each other');
    check(!!no.grounded, 'a refusal must still say why');
  }
  pass(UNKNOWN.length + ' prompts it does not understand are refused, with the canvas untouched');

  var loose = new FakeContext(320, 200);
  var forced = PAINTER.paintIfRecognised(loose, 320, 200, 'a police station',
    { seed: 6, requireSubject: false, minRatio: 0 });
  check(forced.painted === true, 'a caller that explicitly accepts a guess was still refused');
  pass('a caller who wants a picture regardless can say so');
})();

console.log('');
if (failures) {
  console.log('✗ ' + failures + ' of ' + checks + ' checks failed\n');
  process.exit(1);
}
console.log('All ' + checks + ' checks passed\n');
