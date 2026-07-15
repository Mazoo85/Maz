# maz::script — GDScript Feature-Parity Roadmap

A build plan for `maz::script`: a dynamically-typed, tree-walking scripting
language with a C++20 host-binding API, targeting **≥ GDScript** for real game
use. Header-only, under `engine/include/maz/script/`, namespace `maz::script`.

**Status:** SC1–SC8 shipped (Beta complete + safety: stack traces, execution budgets, warnings; `await` deferred) — lexer / recursive-descent parser / tree-walking
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

## SC5 — Classes / Struct-like Objects ✅ *(shipped)*

- `class Foo { ... }` with `var` fields (defaulted) + `func` methods, `self`,
  the `_init` constructor, `Foo.new(...)` **and** `Foo(...)` construction. [GD]
- `extends Base` single inheritance; `super.method(...)` / `super._init(...)`
  resolving from the defining class up the chain (verified 3 levels deep). [GD]
- **Reference semantics** for instances (like arrays/dicts): assigning an object
  aliases it. Methods are first-class — `var f = obj.method` yields a bound
  callable that remembers its receiver. [GD]
- *Not yet:* `const` / `enum` / `static` members, `is` / `as`, and opt-in value
  types (tracked for a later pass).

## SC6 — Engine-Object Binding (Host Integration) ✅ *(shipped)*

- Register C++ types with a fluent API — `vm.bindClass("Sprite").property("x",
  get, set).method("move", fn)` — then read/write properties and call methods on
  live host objects from script. [GD]
- Script instances driven from the host: `vm.instantiate("Player")` builds an
  instance, `vm.objectHasMethod(o, "_process")` probes for a hook, and
  `vm.callOn(o, "_process", {dt})` invokes `_ready` / `_process(dt)` /
  `_physics_process(dt)` (or any method) safely — errors are captured, never
  thrown into the game loop. [GD]
- **Safe handles**: host objects are held weakly; touching a freed object is a
  clean catchable error (`"...freed 'Sprite' object"`), not a dangling-pointer
  crash. [BETTER — avoids GDScript freed-object footguns].
- *Not yet:* templated auto-binding of whole C++ types (the current API is
  explicit per-member); property/method access is the sandbox boundary.

## SC7 — Signals & Callbacks ✅ *(signals shipped; `await` deferred — see note)*

- Class-level `signal pressed;` declarations (per-instance signal fields) and a
  standalone `Signal("name")` builtin. [GD]
- `sig.connect(callable [, oneshot])`, `sig.disconnect`, `sig.is_connected`,
  `sig.emit(args...)`, `sig.connection_count()`, `sig.disconnect_all()`,
  `sig.get_name()`. One-shot connections auto-remove after firing. [GD]
- **Sync vs. queued dispatch**: `sig.emit(...)` fires immediately (deterministic);
  `sig.emit_deferred(...)` queues, and the host drains it with `vm.flushDeferred()`
  at a controlled point in the frame — the foundation for netcode lockstep
  ordering. [BETTER].
- **`await` is intentionally deferred.** True `await`-on-signal needs interpreter
  suspension (capturing and resuming a mid-evaluation call stack), which a
  recursive tree-walker can't do without a bytecode/fiber rewrite. Rather than
  ship a fake `await`, we cover async flows with `connect` + one-shot connections
  (the same effect, explicit). A real coroutine `await` is tracked for the VM-core
  pass alongside SC8's execution budgets.

## SC8 — Error Handling, Diagnostics & Safety ✅ *(shipped)*

- **Stack traces**: every runtime error captures the call chain (innermost-first,
  with the failing line), exposed via `vm.stackTrace()`. [BETTER over GDScript's
  terse errors]. Errors propagate to the host as `error()` / `errorLine()`, never
  as a crash.
- **Execution budget**: `vm.setStepBudget(n)` caps interpreter steps per
  `run()`/`call()`, so a runaway `while(true){}` in a mod becomes a catchable
  "execution budget exceeded" error instead of a frozen game. [BETTER — a modder's
  infinite loop can't hang the game].
- **Recursion limit**: `vm.setRecursionLimit(n)` catches runaway recursion before
  it can exhaust the native stack.
- **Warnings pass**: a static analysis over the parsed AST reports variable
  shadowing and unreachable code (after return/break/continue), surfaced via
  `vm.warnings()` — non-fatal, execution still proceeds. [GD].

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
- **Beta (game structure):** SC4 ✅ → SC5 ✅ → SC6 ✅ → SC7 ✅ (signals; `await` deferred to the VM-core pass).
- **1.0 (production):** SC8 ✅ → SC9.
- **1.x (edge over Godot):** SC10–SC11.

**Front-loaded risks:** the host-binding template API (SC6) is the engine's real
scripting interface — design it before it has many call sites; coroutines /
`await` (SC7) need interpreter suspension — prototype before committing.
