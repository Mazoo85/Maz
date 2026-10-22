/*
 * COMPANION — the smart mind (optional)
 * -------------------------------------
 * Hands plain conversation to Claude, for any game that uses the brain.
 *
 * The trainable brain (brain.js) always answers commands, lessons and
 * memories itself: it's instant, free and works offline. When the player has
 * added a Claude API key, plain conversation goes to Claude instead, which
 * talks back in character and can still pick actions for the companion.
 *
 * Claude is told the brain's character (name, personality, setting, actions),
 * everything the brain has learned, and what is in the world right now, so
 * both minds share one memory and one body.
 *
 * Claude answers in a fixed JSON shape (structured output), so a reply is
 * always { say: '...', actions: [{ type, target }] }, and read() drops any
 * action the brain says the game can't do. The game never has to guess.
 *
 * Most of this file is pure (no DOM, no network) and tested in Node. Only
 * connect() touches the network: it loads the official Anthropic SDK from a
 * CDN the first time smart chat is used.
 *
 *   var claude = await CompanionMind.connect(apiKey);
 *   var history = new CompanionMind.History();
 *   var reply = await CompanionMind.ask(claude, brain, history, text, world, self);
 *   // reply: { say, actions, ok }
 *
 * Exposed as: window.CompanionMind (also module.exports for testing).
 */
(function (root) {
  'use strict';

  var MODEL = 'claude-opus-5-5';
  var SDK_URL = 'https://cdn.jsdelivr.net/npm/@anthropic-ai/sdk@0.132.1/+esm';
  var MAX_HISTORY = 30;   // messages (player + companion) sent with each request
  var MAX_ACTIONS = 4;
  var MAX_SAY = 400;      // characters; a speech bubble, not an essay

  // What Claude must answer with, for this brain's character. `target` is ''
  // when an action needs none.
  function schema(brain) {
    return {
      type: 'object',
      additionalProperties: false,
      required: ['say', 'actions'],
      properties: {
        say: { type: 'string', description: 'What the character says out loud. One to three short sentences.' },
        actions: {
          type: 'array',
          description: 'Things the character does, in order. Empty if it just talks.',
          items: {
            type: 'object',
            additionalProperties: false,
            required: ['type', 'target'],
            properties: {
              type: { type: 'string', enum: brain.character.actions.map(function (a) { return a.type; }) },
              target: { type: 'string', description: 'The target, for actions that take one (see the list). Otherwise empty.' }
            }
          }
        }
      }
    };
  }

  // The standing instructions: who the character is and what it can do. Kept
  // the same between turns; what changes (the world, memories) goes in
  // `situation`, so the instructions stay cacheable.
  function system(brain) {
    var ch = brain.character;
    var lines = [
      'You are ' + ch.name + ', a companion character in ' + ch.setting + '. ' +
      'The player talks to you in a chat box, and you talk back and act in the game world.',
      '',
      'How to talk:',
      '- Stay in character. Your personality: ' + ch.personality + '. Keep it family-friendly.',
      '- Keep replies short: one to three sentences, like a speech bubble.',
      '- You live inside the game. Talk about the game world, the player and your adventures, not about being an AI.',
      '- Only mention things that are really in the world right now (listed in the situation).',
      '- The player may have given you a new name; the situation says what you are called now.',
      '',
      'How to act (the "actions" list, done in order):'
    ];
    ch.actions.forEach(function (a) {
      var target = a.target ? ' (target = one of the ' + a.target + (a.also.length ? ', or ' + a.also.join('/') : '') + ')' : '';
      lines.push('- ' + a.type + target + ': ' + a.help + '.');
    });
    lines.push(
      '- If the player asks you to do something, include the matching actions. If they are just chatting, actions can be empty, or a small fitting reaction.',
      '- Use targets exactly as they are written in the situation. If the player asks for something that is not there, say so kindly and do not invent it.',
      '',
      'The player can also teach you new words and tell you things to remember; that part is handled by your built-in memory, which is shown in the situation. Use it naturally.'
    );
    return lines.join('\n');
  }

  // What is true right now. world: the game's target lists, plus optional
  // `notes` (a string or list of strings) for anything else Claude should
  // know: "It is night.", "The player's bag: 2 apples.". self: { mode, carrying }.
  function situation(brain, world, self) {
    world = world || {};
    self = self || {};
    var lines = [];
    lines.push('Your name: ' + brain.name + '.');
    lines.push('The player\'s name: ' + (brain.playerName || 'unknown (you could ask)') + '.');
    if (brain.facts.length) lines.push('Things the player asked you to remember: ' + brain.facts.join('; ') + '.');
    var taught = Object.keys(brain.phrases);
    if (taught.length) {
      lines.push('Phrases the player taught you: ' + taught.map(function (k) {
        var p = brain.phrases[k];
        return '"' + k + '" = ' + (p.reply ? 'say "' + p.reply + '"' : brain.describe(p.actions));
      }).join('; ') + '.');
    }
    targetLists(brain).forEach(function (key) {
      lines.push(capitalize(key) + ' around you: ' + (countOf(world[key] || []) || 'none') + '.');
    });
    if (self.mode) lines.push('What you are doing: ' + self.mode + (self.carrying ? ', carrying a ' + self.carrying : '') + '.');
    var notes = world.notes ? [].concat(world.notes) : [];
    notes.forEach(function (n) { if (n) lines.push(String(n)); });
    return lines.join('\n');
  }

  // The world lists the character's actions draw targets from: items, places...
  function targetLists(brain) {
    var keys = [];
    brain.character.actions.forEach(function (a) { if (a.target && keys.indexOf(a.target) === -1) keys.push(a.target); });
    return keys;
  }

  function countOf(list) {
    var n = {};
    list.forEach(function (x) { n[x] = (n[x] || 0) + 1; });
    return Object.keys(n).map(function (k) { return n[k] > 1 ? k + ' (' + n[k] + ')' : k; }).join(', ');
  }

  function capitalize(s) {
    return s.charAt(0).toUpperCase() + s.slice(1);
  }

  // The conversation so far, as Claude expects it: strictly player, companion,
  // player, companion... starting with the player. Add every exchange, the
  // ones the brain answered too, so Claude knows what was said.
  function History() {
    this.turns = [];
  }

  History.prototype.add = function (playerSaid, companionSaid) {
    this.turns.push({ role: 'user', content: String(playerSaid) });
    this.turns.push({ role: 'assistant', content: String(companionSaid || '...') });
    while (this.turns.length > MAX_HISTORY) this.turns.splice(0, 2);
  };

  // The full request body for one line from the player.
  function request(brain, history, text, world, self) {
    return {
      model: MODEL,
      max_tokens: 4000,
      system: system(brain),
      messages: history.turns.concat([
        { role: 'user', content: '[Situation]\n' + situation(brain, world, self) + '\n\n[The player says]\n' + text }
      ]),
      output_config: {
        effort: 'low', // quick, chatty replies; this is conversation, not a hard problem
        format: { type: 'json_schema', schema: schema(brain) }
      },
      // If a safety check ever declines a line, let the API retry it on the
      // model it recommends instead of leaving the companion speechless.
      betas: ['server-side-fallback-2026-07-01'],
      fallbacks: 'default'
    };
  }

  // Claude's response -> { say, actions } the game can trust: actions it can
  // perform, with targets that exist. Anything off is dropped, not guessed.
  function read(response, brain, world) {
    if (!response || response.stop_reason === 'refusal') {
      return { say: 'Hmm, let\'s talk about something else!', actions: [], ok: false };
    }
    var text = '';
    (response.content || []).forEach(function (b) { if (b.type === 'text') text += b.text; });
    var data;
    try { data = JSON.parse(text); } catch (e) { data = null; }
    if (!data || typeof data.say !== 'string') {
      return { say: text ? text.slice(0, MAX_SAY) : '...', actions: [], ok: false };
    }
    var actions = (Array.isArray(data.actions) ? data.actions : []).filter(function (a) {
      return brain.canDo(a, world);
    }).slice(0, MAX_ACTIONS).map(function (a) {
      return brain.action(a.type).target ? { type: a.type, target: a.target } : { type: a.type };
    });
    var say = data.say.trim().slice(0, MAX_SAY) || '...';
    return { say: say, actions: actions, ok: true };
  }

  // One line from the player, through Claude. `claude` is what connect() gave
  // (or any object with the SDK's beta.messages.create, for tests).
  async function ask(claude, brain, history, text, world, self) {
    var response = await claude.beta.messages.create(request(brain, history, text, world, self));
    return read(response, brain, world);
  }

  // Browser only: load the Anthropic SDK and make a client for this key. The
  // SDK is loaded once, the first time; connect() again for a new key.
  var sdk = null;
  async function connect(apiKey) {
    if (!sdk) {
      var mod = await import(SDK_URL);
      sdk = mod.default || mod.Anthropic;
    }
    return new sdk({ apiKey: apiKey, dangerouslyAllowBrowser: true });
  }

  // A sentence for the player when ask() fails, so the game can fall back to
  // the brain's own answer and say why.
  function explain(e) {
    if (sdk && e instanceof sdk.AuthenticationError) return 'Claude didn\'t accept that API key.';
    if (sdk && e instanceof sdk.PermissionDeniedError) return 'That API key isn\'t allowed to use this model.';
    if (sdk && e instanceof sdk.RateLimitError) return 'Claude is busy (too many messages); try again in a moment.';
    if (sdk && e instanceof sdk.APIConnectionError) return 'Couldn\'t reach Claude (are you online?).';
    if (sdk && e instanceof sdk.APIError) return 'Claude had a problem (' + (e.status || 'error') + ').';
    return 'Couldn\'t load smart chat (are you online?).';
  }

  var api = {
    MODEL: MODEL,
    SDK_URL: SDK_URL,
    History: History,
    schema: schema,
    system: system,
    situation: situation,
    request: request,
    read: read,
    ask: ask,
    connect: connect,
    explain: explain
  };

  root.CompanionMind = api;
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
})(typeof window !== 'undefined' ? window : this);
