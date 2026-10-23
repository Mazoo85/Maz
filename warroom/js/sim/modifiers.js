/*
 * The modifier table: every number that ideas, laws, technologies, advisors,
 * weather and terrain adjust, flattened once per day into
 * { [tag]: { [key]: number } } so every subsystem reads the same values.
 *
 * Stage 1 has nothing that produces modifiers, so the table is empty, but the
 * function and the lookup are the contract later stages build on.
 */
export function buildModifiers(state) {
  const table = {};
  for (const tag of Object.keys(state.countries)) table[tag] = {};
  return table;
}

export function mod(table, tag, key, fallback = 0) {
  const t = table[tag];
  if (!t || t[key] === undefined) return fallback;
  return t[key];
}
