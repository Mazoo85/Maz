/*
 * SONG FORGE — the published soundtrack surface.
 *
 *   node music/tests/soundtrack.test.js
 *
 * No dependencies and no browser. WebAudio is stubbed with a recorder that
 * counts what was built and connected, which is enough to check the things
 * that matter to a game consuming this: that it works through the game's own
 * context and volume bus rather than around them, that a mood change actually
 * composes new music, and — most of all — that nothing here can throw into a
 * game's frame loop. A game must never crash over its background music.
 */
'use strict';

const fs = require('fs');
const path = require('path');
const vm = require('vm');

const JS = path.join(__dirname, '..', 'js');

let passed = 0;
const failures = [];

function test(name, fn) {
  try {
    fn();
    passed++;
    console.log('  ok   ' + name);
  } catch (e) {
    failures.push(name + ' — ' + e.message);
    console.log('  FAIL ' + name + ' — ' + e.message);
  }
}

function assert(cond, message) {
  if (!cond) throw new Error(message || 'assertion failed');
}

function eq(actual, expected, message) {
  if (actual !== expected) {
    throw new Error((message || 'values differ') + ': got ' + JSON.stringify(actual) +
      ', expected ' + JSON.stringify(expected));
  }
}

/* ------------------------------------------------------- a WebAudio stub */
/* Just enough of the API for the engine to build its graph, plus a log of what
 * happened so the tests can assert on routing rather than on sound. */
function makeAudioStub() {
  const log = { created: [], connections: [], resumed: 0, started: 0 };
  const node = (type, extra) => {
    const n = Object.assign({
      type,
      connect(dest) { log.connections.push([type, dest && dest.type]); return dest; },
      disconnect() {}
    }, extra || {});
    log.created.push(type);
    return n;
  };
  const param = (v) => ({
    value: v,
    setValueAtTime(x) { this.value = x; },
    linearRampToValueAtTime(x) { this.value = x; },
    exponentialRampToValueAtTime(x) { this.value = x; },
    cancelScheduledValues() {},
    setTargetAtTime() {}
  });
  const ctx = {
    type: 'context',
    state: 'running',
    sampleRate: 48000,
    currentTime: 0,
    destination: node('destination'),
    resume() { log.resumed++; },
    createGain: () => node('gain', { gain: param(1) }),
    createOscillator: () => node('oscillator', {
      frequency: param(440), detune: param(0),
      start() { log.started++; }, stop() {}
    }),
    createBufferSource: () => node('bufferSource', {
      buffer: null, playbackRate: param(1), loop: false,
      start() { log.started++; }, stop() {}
    }),
    createBuffer: (ch, len, rate) => ({
      length: len, sampleRate: rate, numberOfChannels: ch,
      getChannelData: () => new Float32Array(len)
    }),
    createBiquadFilter: () => node('biquad', { frequency: param(1000), Q: param(1), gain: param(0), type: 'lowpass' }),
    createDynamicsCompressor: () => node('compressor', {
      threshold: param(-24), knee: param(30), ratio: param(12), attack: param(0.003), release: param(0.25)
    }),
    createWaveShaper: () => node('shaper', { curve: null, oversample: 'none' }),
    createConvolver: () => node('convolver', { buffer: null, normalize: true }),
    createDelay: () => node('delay', { delayTime: param(0.3) }),
    createStereoPanner: () => node('panner', { pan: param(0) }),
    createAnalyser: () => node('analyser', { fftSize: 2048, frequencyBinCount: 1024, getByteFrequencyData() {}, getByteTimeDomainData() {} }),
    createChannelMerger: () => node('merger'),
    createChannelSplitter: () => node('splitter')
  };
  return { ctx, log };
}

/* Load SONG FORGE and the soundtrack surface into one sandbox. */
function load(audio) {
  const sandbox = {
    console, Math, Date, performance,
    setInterval: () => 0, clearInterval: () => {},
    setTimeout: () => 0, clearTimeout: () => {},
    AudioContext: function () { return audio ? audio.ctx : makeAudioStub().ctx; },
    OfflineAudioContext: function () { return makeAudioStub().ctx; }
  };
  sandbox.window = sandbox;
  sandbox.global = sandbox;
  sandbox.globalThis = sandbox;
  vm.createContext(sandbox);
  for (const f of ['theory.js', 'genres.js', 'composer.js', 'synth.js', 'engine.js', 'soundtrack.js']) {
    vm.runInContext(fs.readFileSync(path.join(JS, f), 'utf8'), sandbox, { filename: f });
  }
  return sandbox;
}

console.log('\nWHAT A GAME CAN ASK FOR');

test('options() lists real genres and moods', () => {
  const { MazSoundtrack } = load();
  const o = MazSoundtrack.options();
  assert(o.genres.length >= 8, 'expected SONG FORGE\'s genres, got ' + o.genres.length);
  assert(o.moods.length >= 5, 'expected SONG FORGE\'s moods, got ' + o.moods.length);
  assert(o.genres.includes('chiptune'), 'chiptune is missing — ZOMBOID and DEAD SECTOR both ask for it');
});

test('an unknown genre or mood falls back instead of throwing', () => {
  // A game must never crash over its background music.
  const { MazSoundtrack } = load();
  const track = MazSoundtrack.create({ genre: 'polka', mood: 'furious' });
  const now = track.current();
  assert(MazSoundtrack.options().genres.includes(now.genre), 'fell back to a genre that does not exist');
  assert(MazSoundtrack.options().moods.includes(now.mood), 'fell back to a mood that does not exist');
});

console.log('\nPLAYING THROUGH THE GAME\'S OWN AUDIO');

test('it plays into the context and bus the game hands it', () => {
  // This is the whole point of the surface: the game's existing mute button and
  // volume slider must keep working, and its sound effects must share one
  // context with the music rather than fighting a second one.
  const audio = makeAudioStub();
  const { MazSoundtrack } = load(audio);
  const bus = audio.ctx.createGain();
  bus.type = 'gameMusicBus';
  const track = MazSoundtrack.create({ context: audio.ctx, destination: bus, genre: 'chiptune' });

  eq(track.start(), true, 'it did not start');
  assert(track.isPlaying(), 'it reports not playing after a successful start');
  assert(audio.log.connections.some(([, to]) => to === 'gameMusicBus'),
    'nothing was connected to the game\'s bus — the game\'s volume control would do nothing');
});

test('it composes a real song and says what it is', () => {
  const audio = makeAudioStub();
  const { MazSoundtrack } = load(audio);
  const track = MazSoundtrack.create({ context: audio.ctx, genre: 'synthwave', mood: 'dark' });
  track.start();
  const now = track.current();
  eq(now.genre, 'synthwave');
  eq(now.mood, 'dark');
  assert(now.title && now.title.length > 0, 'the track has no title');
  assert(now.bpm > 20 && now.bpm < 250, 'implausible tempo: ' + now.bpm);
});

test('starting twice does not start two soundtracks', () => {
  const audio = makeAudioStub();
  const { MazSoundtrack } = load(audio);
  const track = MazSoundtrack.create({ context: audio.ctx });
  track.start();
  const after = audio.log.created.length;
  eq(track.start(), true, 'the second start should be a no-op that reports success');
  eq(audio.log.created.length, after, 'the second start built a second graph');
});

test('stop really stops, and start after stop composes again', () => {
  const audio = makeAudioStub();
  const { MazSoundtrack } = load(audio);
  const track = MazSoundtrack.create({ context: audio.ctx, genre: 'lofi' });
  track.start();
  const firstTitle = track.current().title;
  track.stop();
  assert(!track.isPlaying(), 'still playing after stop');
  track.start();
  assert(track.isPlaying(), 'did not restart');
  assert(track.current().title !== null, 'restarting produced no song');
  assert(typeof firstTitle === 'string');
});

console.log('\nFOLLOWING THE GAME');

test('setMood composes new music in the same genre', () => {
  const audio = makeAudioStub();
  const { MazSoundtrack } = load(audio);
  const track = MazSoundtrack.create({ context: audio.ctx, genre: 'chiptune', mood: 'chill' });
  track.start();
  const before = track.current();
  eq(track.setMood('driving'), true, 'the mood change was refused');
  const after = track.current();
  eq(after.mood, 'driving');
  eq(after.genre, before.genre, 'the genre changed too — a game asked for a mood, not a new style');
  assert(track.isPlaying(), 'the music stopped instead of changing');
});

test('setMood to the mood already playing is a no-op', () => {
  // A game calls this from its own state every frame. It must be free to.
  const audio = makeAudioStub();
  const { MazSoundtrack } = load(audio);
  const track = MazSoundtrack.create({ context: audio.ctx, mood: 'dark' });
  track.start();
  const built = audio.log.created.length;
  eq(track.setMood('dark'), false, 'it claimed to change to the mood already playing');
  eq(audio.log.created.length, built, 'it rebuilt the graph for a mood it was already in');
});

test('setMood to an unknown mood is refused, not obeyed', () => {
  const audio = makeAudioStub();
  const { MazSoundtrack } = load(audio);
  const track = MazSoundtrack.create({ context: audio.ctx, mood: 'chill' });
  track.start();
  eq(track.setMood('apocalyptic'), false);
  eq(track.current().mood, 'chill', 'it took a mood that does not exist');
});

test('setMood before start just remembers it', () => {
  const audio = makeAudioStub();
  const { MazSoundtrack } = load(audio);
  const track = MazSoundtrack.create({ context: audio.ctx, mood: 'chill' });
  eq(track.setMood('dark'), true);
  eq(track.current().mood, 'dark');
  assert(!track.isPlaying(), 'it started playing on its own');
  track.start();
  eq(track.current().mood, 'dark', 'the remembered mood was lost on start');
});

test('setGenre changes the style and keeps playing', () => {
  const audio = makeAudioStub();
  const { MazSoundtrack } = load(audio);
  const track = MazSoundtrack.create({ context: audio.ctx, genre: 'chiptune' });
  track.start();
  eq(track.setGenre('ambient'), true);
  eq(track.current().genre, 'ambient');
  assert(track.isPlaying(), 'changing genre stopped the music');
});

test('setVolume clamps to 0..1 and survives nonsense', () => {
  const audio = makeAudioStub();
  const { MazSoundtrack } = load(audio);
  const track = MazSoundtrack.create({ context: audio.ctx });
  track.start();
  eq(track.setVolume(2), 1);
  eq(track.setVolume(-1), 0);
  eq(track.setVolume(0.25), 0.25);
  eq(track.setVolume('loud'), 0, 'a non-number should clamp, not become NaN');
});

console.log('\nWHEN THERE IS NO SOUND AT ALL');

test('with no WebAudio every method is a quiet no-op', () => {
  // Headless CI, an old browser, a locked-down device. The game still runs.
  const sandbox = load();
  delete sandbox.AudioContext;
  delete sandbox.webkitAudioContext;
  const track = sandbox.MazSoundtrack.create({});
  eq(track.start(), false, 'it claimed to start with no audio available');
  eq(track.isPlaying(), false);
  track.setMood('dark');
  track.setGenre('lofi');
  track.setVolume(0.5);
  track.stop();
  assert(true, 'nothing threw');
});

test('a suspended context is resumed on start', () => {
  // Browsers start audio suspended until the person interacts. A game calling
  // start() from its PLAY button must actually get sound.
  const audio = makeAudioStub();
  audio.ctx.state = 'suspended';
  const { MazSoundtrack } = load(audio);
  MazSoundtrack.create({ context: audio.ctx }).start();
  assert(audio.log.resumed > 0, 'the context was left suspended, so nothing would be heard');
});

console.log('');
if (failures.length) {
  console.log('✗ ' + failures.length + ' failed, ' + passed + ' passed\n');
  failures.forEach((f) => console.log('  - ' + f));
  process.exit(1);
}
console.log('✓ ' + passed + ' tests passed\n');
