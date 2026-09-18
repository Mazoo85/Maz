#!/usr/bin/env node
// Command-line Name Maker — no browser needed.
//
// Usage:
//   node name-maker-cli.js        # print one random name
//   node name-maker-cli.js 5      # print 5 random names

const { ADJECTIVES, NOUNS } = require("./js/words.js");

function pick(arr) {
  return arr[Math.floor(Math.random() * arr.length)];
}

function capitalize(word) {
  return word.charAt(0).toUpperCase() + word.slice(1);
}

function makeName() {
  // Adjective first, then noun — in that order.
  return capitalize(pick(ADJECTIVES)) + " " + capitalize(pick(NOUNS));
}

const count = Math.max(1, parseInt(process.argv[2], 10) || 1);
for (let i = 0; i < count; i++) {
  console.log(makeName());
}
