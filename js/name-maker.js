// Name Maker — picks a random adjective then a random noun and shows them together.
// Depends on ADJECTIVES and NOUNS (loaded from js/words.js).

function pick(arr) {
  return arr[Math.floor(Math.random() * arr.length)];
}

function capitalize(word) {
  return word.charAt(0).toUpperCase() + word.slice(1);
}

function makeName() {
  // Adjective first, then noun — in that order.
  const adjective = pick(ADJECTIVES);
  const noun = pick(NOUNS);
  return capitalize(adjective) + " " + capitalize(noun);
}

document.addEventListener("DOMContentLoaded", function () {
  const button = document.getElementById("generate");
  const result = document.getElementById("result");

  button.addEventListener("click", function () {
    result.textContent = makeName();
  });
});
