/*
 * COMPANION — the smart mind (optional)
 * -------------------------------------
 * Pure logic (no DOM, no network) so it runs in the browser AND in a headless
 * test. The page does the actual call to Claude; this file decides what to
 * tell Claude and how to read its answer.
 *
 * The trainable brain (brain.js) always answers commands, lessons and
 * memories itself: it's instant, free and works offline. When the player has
 * added a Claude API key, plain conversation goes to Claude instead, which
 * talks back in character and can still pick actions for the companion.
 *
 * Claude sees everything the brain has learned (names, facts, taught phrases)
 * and what is in the world right now, so both minds share one memory.
 *
 * Claude answers in a fixed JSON shape (structured output), so a reply is
 * always { say: '...', actions: [{ type, target }] } and never free text the
 * game has to guess at.
 *
 * Exposed as: window.CompanionMind (also module.exports for testing).
 */
(function (root) {
  'use strict';

  var Brain = root.CompanionBrain || (typeof require === 'function' ? require('./brain.js') : null);

  var MODEL = 'claude-opus-5-5';
  var MAX_HISTORY = 30;   // messages (player + companion) sent with each request
  var MAX_ACTIONS = 4;
  var MAX_SAY = 400;      // characters; a speech bubble, not an essay

  // What Claude must answer with. `target` is '' when an action needs none.
  var REPLY_SCHEMA = {
    type: 'object',
    additionalProperties: false,
    required: ['say', 'actions'],
    properties: {
      say: { type: 'string', description: 'What the companion says out loud. One to three short sentences.' },
      actions: {
        type: 'array',
        description: 'Things the companion does, in order. Empty if it just talks.',
        items: {
          type: 'object',
          additionalProperties: false,
          required: ['type', 'target'],
          properties: {
            type: { type: 'string', enum: Brain.ACTION_TYPES },
            target: { type: 'string', description: 'For fetch: an item. For goto: a place or left/right/up/down. Otherwise empty.' }
          }
        }
      }
    }
  };

  // The standing instructions: who the companion is and what it can do. Kept
  // the same between turns; what changes (the world, memories) goes in
  // `situation`, so the instructions stay cacheable.
  function system(brain) {
    return [
      'You are ' + brain.name + ', a friendly, upbeat companion character in a small cozy adventure game. ' +
      'The player talks to you in a chat box, and you talk back and act in the game world.',
      '',
      'How to talk:',
      '- Stay in character as ' + brain.name + '. Warm, playful, a little goofy, and loyal. Keep it family-friendly.',
      '- Keep replies short: one to three sentences, like a speech bubble.',
      '- You live inside the game. Talk about the meadow, the player and your adventures, not about being an AI.',
      '- Only mention things that are really in the world right now (listed in the situation).',
      '',
      'How to act (the "actions" list, done in order):',
      '- follow: walk with the player.  stay: stop and wait where you are.  come: walk to the player.',
      '- fetch (target = an item): go pick it up and bring it to the player.',
      '- goto (target = a place, or left/right/up/down): walk there.',
      '- jump, dance, sit, wander (explore on your own), sleep: no target.',
      '- If the player asks you to do something, include the matching actions. If they are just chatting, actions can be empty, or a small reaction like jump or dance.',
      '- Use targets exactly as they are written in the situation. If the player asks for something that is not there, say so kindly and do not invent it.',
      '',
      'The player can also teach you new words and tell you things to remember; that part is handled by your built-in memory, which is shown in the situation. Use it naturally.'
    ].join('\n');
  }

  // What is true right now. self: { mode, carrying } from the game.
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
        return '"' + k + '" = ' + (p.reply ? 'say "' + p.reply + '"' : Brain.describe(p.actions));
      }).join('; ') + '.');
    }
    var items = countOf(world.items || []);
    lines.push('Items you can see: ' + (items || 'none') + '.');
    lines.push('Places: ' + ((world.places || []).join(', ') || 'none') + '.');
    if (self.mode) lines.push('What you are doing: ' + self.mode + (self.carrying ? ', carrying a ' + self.carrying : '') + '.');
    if (world.collected) lines.push('The player\'s bag: ' + world.collected + '.');
    return lines.join('\n');
  }

  function countOf(list) {
    var n = {};
    list.forEach(function (x) { n[x] = (n[x] || 0) + 1; });
    return Object.keys(n).map(function (k) { return n[k] > 1 ? k + ' (' + n[k] + ')' : k; }).join(', ');
  }

  // The conversation so far, as Claude expects it: strictly player, companion,
  // player, companion... starting with the player.
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
        format: { type: 'json_schema', schema: REPLY_SCHEMA }
      },
      // If a safety check ever declines a line, let the API retry it on the
      // model it recommends instead of leaving the companion speechless.
      betas: ['server-side-fallback-2026-07-01'],
      fallbacks: 'default'
    };
  }

  // Claude's response -> { say, actions } the game can trust: actions it can
  // perform, with targets that exist. Anything off is dropped, not guessed.
  function read(response, world) {
    world = world || {};
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
    var items = world.items || [];
    var places = (world.places || []).concat(Brain.DIRECTIONS);
    var actions = (Array.isArray(data.actions) ? data.actions : []).filter(function (a) {
      if (!a || Brain.ACTION_TYPES.indexOf(a.type) === -1) return false;
      if (a.type === 'fetch') return items.indexOf(a.target) !== -1;
      if (a.type === 'goto') return places.indexOf(a.target) !== -1;
      return true;
    }).slice(0, MAX_ACTIONS).map(function (a) {
      return a.type === 'fetch' || a.type === 'goto' ? { type: a.type, target: a.target } : { type: a.type };
    });
    var say = data.say.trim().slice(0, MAX_SAY) || '...';
    return { say: say, actions: actions, ok: true };
  }

  var api = {
    MODEL: MODEL,
    REPLY_SCHEMA: REPLY_SCHEMA,
    History: History,
    system: system,
    situation: situation,
    request: request,
    read: read
  };

  root.CompanionMind = api;
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
})(typeof window !== 'undefined' ? window : this);
