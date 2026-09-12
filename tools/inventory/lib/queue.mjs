/*
 * The work queue — every failing check and every opportunity, collapsed into a
 * ranked list of tasks that each name the files they touch.
 *
 * Two shapes of task come out of here, because 690 engine modules and 161 apps
 * cannot each get their own line without burying the six things that actually
 * matter. Artifacts the repo has a handful of (browser projects, Python tools,
 * CI gates) produce one task per failing check. Artifacts it has hundreds of
 * produce one task per *check*, listing the artifacts that fail it — which is
 * also how the work is really done: you add --headless to a batch of apps in an
 * afternoon, not to one app a night.
 */

// Which failing checks are urgent, which are coverage, which are polish.
// Anything unlisted is polish.
const P1 = new Set([
  'app:registered', 'app:cmake',
  'gate:ci', 'gate:tests',
  'py-tool:ci', 'py-tool:tests',
  'web-app:tests'
]);
const P2 = new Set([
  'engine-module:tested', 'engine-module:demoed',
  'app:headless', 'app:golden', 'app:uses-engine',
  'web-app:exchange', 'web-app:nav',
  'py-tool:readme', 'web-app:readme'
]);

// Kinds small enough that every artifact deserves its own task line.
const ITEMISED = new Set(['web-app', 'py-tool', 'gate', 'build-tool']);

// How many subjects to name inline in a grouped task's text.
const NAMED_SUBJECTS = 12;

function priorityOf(kind, checkId) {
  const key = `${kind}:${checkId}`;
  if (P1.has(key)) return 1;
  if (P2.has(key)) return 2;
  return 3;
}

export function buildQueue(model) {
  const tasks = [];

  // --- itemised kinds: one task per artifact per failing check -------------
  for (const a of model.all) {
    if (!ITEMISED.has(a.kind)) continue;
    for (const c of a.checks) {
      if (c.ok) continue;
      tasks.push({
        id: `${a.id}#${c.id}`,
        priority: priorityOf(a.kind, c.id),
        kind: a.kind,
        check: c.id,
        title: `${a.name}: ${c.label}`,
        detail: c.fix,
        paths: [a.path],
        source: `inventory:${a.id}:${c.id}`
      });
    }
  }

  // --- grouped kinds: one task per check, listing who fails it -------------
  const grouped = new Map();
  for (const a of model.all) {
    if (ITEMISED.has(a.kind)) continue;
    for (const c of a.checks) {
      if (c.ok) continue;
      const key = `${a.kind}:${c.id}`;
      if (!grouped.has(key)) grouped.set(key, { kind: a.kind, check: c, members: [] });
      grouped.get(key).members.push(a);
    }
  }

  // An opportunity that sets `covers` is the *reasoning* behind a grouped check —
  // "38 apps cannot run without a display" explains why `app:headless` matters.
  // Emitting both would put the same job in the queue twice, once with the
  // reasoning and once with the member list, so they are merged into one task
  // that carries both.
  const explained = new Map(
    model.opportunities.filter((o) => o.covers).map((o) => [o.covers, o])
  );
  // Only an opportunity a grouped task actually absorbed may be dropped below.
  // An itemised kind (one task per browser project, say) produces no group for
  // it to merge into, and silently swallowing it would lose the reasoning.
  const absorbed = new Set();

  for (const [key, g] of grouped) {
    const names = g.members.map((m) => m.name);
    const shown = names.slice(0, NAMED_SUBJECTS).join(', ');
    const more = names.length > NAMED_SUBJECTS ? `, and ${names.length - NAMED_SUBJECTS} more` : '';
    const one = g.members.length === 1;
    const why = explained.get(key);
    if (why) absorbed.add(why.id);
    tasks.push({
      id: `group:${key}`,
      priority: why ? (why.value >= 3 ? 1 : 2) : priorityOf(g.kind, g.check.id),
      kind: g.kind,
      check: g.check.id,
      title: why
        ? why.title
        : `${g.members.length} ${label(g.kind)}${one ? '' : 's'} ${one ? 'fails' : 'fail'} "${g.check.label}"`,
      // The fix text of the first member is written per-artifact, so it reads
      // as a worked example of what each of the others needs.
      detail: `${why ? why.detail + ' — ' : ''}${shown}${more}. Example: ${g.check.fix}`,
      paths: dedupe(g.members.map((m) => m.path)).slice(0, 40),
      members: g.members.map((m) => m.id),
      source: `inventory:${key}`
    });
  }

  // --- opportunities that do not map onto a check --------------------------
  for (const o of model.opportunities) {
    if (absorbed.has(o.id)) continue;   // already merged into its grouped task above
    tasks.push({
      id: `opportunity:${o.id}`,
      priority: o.value >= 3 ? 1 : 2,
      kind: 'opportunity',
      check: '',
      title: o.title,
      detail: o.detail,
      paths: [],
      source: `inventory:opportunity:${o.id}`
    });
  }

  // --- declared pairings --------------------------------------------------
  for (const p of model.pairings) {
    tasks.push({
      id: `pairing:${p.id}`,
      priority: p.value >= 3 ? 1 : 2,
      kind: 'pairing',
      check: '',
      title: p.title,
      detail: p.detail,
      paths: dedupe([...p.from, ...p.to].map((id) => pathFor(model, id)).filter(Boolean)),
      source: `inventory:pairing:${p.id}`
    });
  }

  tasks.sort((a, b) => a.priority - b.priority || (a.id < b.id ? -1 : 1));
  return tasks;
}

function pathFor(model, id) {
  const hit = model.all.find((a) => a.id === id);
  return hit ? hit.path : null;
}

function dedupe(list) {
  return [...new Set(list)];
}

function label(kind) {
  return {
    app: 'app',
    'engine-module': 'engine module',
    doc: 'doc',
    'cpp-test': 'test'
  }[kind] || kind;
}
