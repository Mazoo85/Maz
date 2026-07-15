# maz::script — GDScript Feature-Parity Roadmap

A build plan for `maz::script`: a dynamically-typed, tree-walking scripting
language with a C++20 host-binding API, targeting **≥ GDScript** for real game
use. Header-only, under `engine/include/maz/script/`, namespace `maz::script`.

**Status:** SC1–SC4 shipped (Alpha tier complete + closures) — lexer / recursive-descent parser / tree-walking
interpreter with numbers, strings, bools, nil, the full arithmetic + comparison
+ logical operator set, `var` / assignment, `if` / `else`, `while`, C-style
`for`, user `func`s with parameters + `return`, native host functions, a small
built-in stdlib (`abs`/`min`/`max`/`floor`/`sqrt`/`str`), and clean error
reporting with line numbers; plus arrays, dictionaries, `for..in`/`range`, indexing (`a[i]` / `d[k]` / `d.k`, read + write), method calls (`arr.append`, `dict.keys`, ...), `break`/`continue`, the `in` operator; plus the string library (split/join/replace/substr/find/begins_with/ends_with/strip/to_upper/to_lower), the math library (ceil/round/pow/sin/cos/tan/clamp/lerp/sign/fmod + PI/TAU), conversions (int/float/bool/typeof), a seedable deterministic RNG (seed/randi/randf/randi_range/randf_range), and assert (all unit-tested).

Legend:
- **[GD]** — corresponds to a Godot GDScript feature.
- **[BETTER]** — where a native, embedded, custom implementation can beat GDScript.
- **[NEW]** — no direct GDScript analog; optional edge.

Guiding principle: every milestone must ship *runnable and testable* on its own.

---

## Design Invariants (apply to every milestone)

- **Zero external dependencies.** Pure C++20, header-only. [BETTER] — GDScript is
  entangled with the whole Godot runtime; ours drops into any Maz build.
- **Value model:** a tagged `Value` (`Nil, Bool, Num, Str, Native, Func`; Array /
  Dict / Object land in SC2 / SC5 / SC6).
- **Every token and runtime error carries a line number** — done from SC1 so
  diagnostics never get retrofitted.
- **Deterministic execution.** No hidden global state; a script run is a pure
  function of (source, host bindings, inputs). [BETTER] — enables replay,
  networked lockstep, and reproducible tests.
- **Sandboxed by construction.** Scripts reach the host *only* through explicitly
  registered natives — no ambient filesystem/OS/network. [BETTER].

**Block syntax:** Maz uses **explicit brace blocks** (`{ ... }`) and `;`
statement terminators rather than GDScript's significant indentation. This is a
deliberate, locked-in choice: it removes an entire class of whitespace ambiguity,
is trivial to generate/serialize from the editor, and keeps the parser simple and
robust. Familiar to anyone from a C/JS/Rust background; a thin indentation-based
front-end could be layered later if GDScript source compatibility is ever wanted.

---

## SC1 — Minimal Usable Core ✅ *(shipped)*

- **Literals:** number, string (with escapes), `true`/`false`, `nil`. [GD]
- **Operators:** `+ - * / %`, unary `-`/`!`; comparison `== != < <= > >=`;
  logical `and`/`or` (short-circuit), `!`. `+` concatenates when either side is a
  string. [GD]
- **Variables:** `var x = expr;`, assignment `x = expr;`, lexical block scoping. [GD]
- **Control flow:** `if (...) {...} else {...}`, `while (...) {...}`,
  `for (init; cond; post) {...}`. [GD] (`for..in` ranges land in SC2.)
- **Functions:** `func name(a, b) { ... return expr; }`, recursion. [GD]
- **Host natives:** `vm.registerNative("name", fn)`; `vm.setGlobal/getGlobal`;
  `vm.call("fn", args)` to invoke script functions from C++. [GD]
- **Comments:** `#` and `//` line comments.
- **Errors:** parse + runtime errors with line numbers via `run()` → false /
  `error()` / `errorLine()`; never crashes the host (division / modulo by zero,
  undefined variable/function, bad assignment target all caught).

---

## SC2 — Iteration, Collections, Indexing ✅ *(shipped)*

- `for i in <iterable>:` and `range(n)` / `range(a,b)` / `range(a,b,step)`. [GD]
- **Arrays** `[1,2,3]`, index `a[i]`, nested. [GD]
- **Dictionaries** `{"k": v}`, `d[key]`, `d.key` sugar. [GD]
- `break` / `continue`; `in` / `not in`; ternary `a if cond else b`. [GD]

## SC3 — Strings, Numbers & Core Stdlib ✅ *(shipped)*

- String methods (`length`, `substr`, `find`, `replace`, `split`, `join`,
  `to_upper/to_lower`, `begins_with`, `ends_with`, `format`, `%`). [GD]
- Math lib (`ceil`, `round`, `pow`, `sin/cos/tan`, `clamp`, `lerp`, `sign`,
  `fmod`, `PI`, `TAU`). [GD]
- Conversions (`int`, `float`, `str`, `bool`, `typeof`). [GD]
- Seedable deterministic RNG (`randi`, `randf`, `randf_range`). [BETTER — explicit
  reproducible stream for lockstep/replay].
- `assert`, `push_error`, `push_warning`. [GD]

## SC4 — Closures, Lambdas, Higher-Order ✅ *(shipped)*

- Lambdas `func(a) { return a*2; }` as expressions. [GD]
- **Real closures** capturing upvalues — heap-allocated environments kept alive by the
  capturing function, so a returned closure can read *and mutate* the locals of its defining
  scope (independent counters, memoizers, etc.). [GD]
- Callables as first-class values: passed as args, returned, stored in vars/arrays/dicts. [GD]
- Higher-order array methods: `map`, `filter`, `reduce`, `any`, `all`, `sort` (natural),
  `sort_custom` (comparator), plus `reverse` / `slice`. [GD]

## SC5 — Classes / Struct-like Objects

- `class Foo:` with fields + methods, `self`, `_init`, `Foo.new(...)`,
  `extends` / `super`, `const`, `enum`, `static`, `is`/`as`. [GD]
- **Opt-in value types** (copy semantics, no heap) for hot data. [BETTER].

## SC6 — Engine-Object Binding (Host Integration)

- Register C++ types, methods, and properties; call C++ methods and read/write
  properties from script; `bind<Transform>().method(...).prop(...)`. [GD]
- Script instances attachable to engine entities with `_ready` / `_process(dt)` /
  `_physics_process(dt)` lifecycle hooks. [GD]
- Safe handles to C++ objects (accessing a dead handle is a clean catchable
  error). [BETTER — avoids GDScript freed-object footguns].

## SC7 — Signals & Callbacks

- `signal died(score)`, `connect` / `emit`, one-shot / deferred flags. [GD]
- `await` on signals via interpreter suspension (coroutines). [GD]
- Introspectable/serializable signal graph; sync vs. queued dispatch for netcode.
  [BETTER].

## SC8 — Error Handling, Diagnostics & Safety

- Full script stack traces + source snippets. [BETTER over GDScript's terse
  errors]. Structured error propagation to the host.
- **Execution budgets** (max steps / recursion / allocation) to kill runaway
  scripts. [BETTER — a modder's infinite loop can't hang the game].
- Warnings pass (unused var, unreachable, shadowing). [GD].

## SC9 — Hot Reload

- Reload changed scripts without restart; preserve live instance state where field
  shapes match. [GD]. Keep old version live if new source fails to parse.
- [BETTER] — a pure tree-walker reloads near-instantly (recompile AST, swap, keep
  state) with no compile/link step.

## SC10 — Static Typing (Optional / Gradual)

- Type hints `var x: int`, `func f(a: int) -> String:`, typed arrays/dicts,
  inference `var x := 5`, a type-checker pass; untyped code stays dynamic. [GD]
- [BETTER] — typed regions can take a faster execution path (skip dynamic tag
  checks).

## SC11 — Modules, Tooling & Polish

- File-based modules / `preload`, `@export`-style inspector annotations,
  introspection (`has_method`, `call` by name), debugger hooks (breakpoints,
  step, locals). [GD] — a tree-walker makes stepping trivial to expose. [BETTER].

---

## Milestone Grouping

- **Alpha (playable scripting):** SC1 ✅ → SC2 ✅ → SC3 ✅  **— complete**.
- **Beta (game structure):** SC4 ✅ → SC5–SC7.
- **1.0 (production):** SC8–SC9.
- **1.x (edge over Godot):** SC10–SC11.

**Front-loaded risks:** the host-binding template API (SC6) is the engine's real
scripting interface — design it before it has many call sites; coroutines /
`await` (SC7) need interpreter suspension — prototype before committing.
