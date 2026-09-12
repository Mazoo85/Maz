<!-- Part of MAZ ARCADE — see the repo root README for every project. -->

# Codebase Memory (MCP)

This repo is wired up with a **codebase-memory MCP server** — a small helper that
gives Claude a **persistent, cross-session memory** about this project. Facts it
learns (what a module does, why a decision was made, where something lives) are
saved to a file in the repo, so a *new* Claude session can recall them instead of
re-reading everything from scratch.

You don't have to run anything by hand. When you open this repo in Claude Code (web,
desktop, or CLI), the server starts automatically.

## What's in the box

| File | Purpose |
|------|---------|
| `.mcp.json` | Registers the `codebase-memory` server so Claude Code auto-starts it. |
| `scripts/codebase-memory-server.sh` | Launcher that starts the server and pins the memory file to this repo. |
| `.claude/codebase-memory.json` | The memory itself — a small knowledge graph, committed to git. |
| `docs/CODEBASE_MEMORY.md` | This file. |

Under the hood it uses the official
[`@modelcontextprotocol/server-memory`](https://www.npmjs.com/package/@modelcontextprotocol/server-memory)
package, fetched on demand with `npx` (needs Node.js, already required by the
toolchain). Nothing to install or configure.

## How the memory is stored

It's a **knowledge graph**: a list of *entities* (things — a component, a file, a
decision) each with free-text *observations*, plus *relations* between them
(`contains`, `targets`, etc.). It's saved as newline-delimited JSON in
`.claude/codebase-memory.json`.

Because that file is committed, the memory travels with the repo and survives the
ephemeral containers used by Claude Code on the web.

### Seeded facts

The graph ships pre-seeded with the basics: the **Maz Repository** and its two
components — **Maz Engine** (C++/Vulkan/SDL3) and **ZOMBOID: ANCHORAGE** (the browser
game) — plus the ZOMBOID source layout and this tooling. Claude will add to it as you
work.

## Using it

Just talk to Claude normally. Helpful phrasings:

- *"Remember that the render backend lives in `engine/render/` and is Vulkan-only for now."* → saves a fact
- *"What do you remember about the engine's milestones?"* → recalls facts
- *"Search your memory for anything about Anchorage."* → queries the graph

Behind those requests Claude calls the server's tools: `create_entities`,
`add_observations`, `create_relations`, `search_nodes`, `read_graph`,
`delete_entities`, and so on.

## Inspecting or resetting the memory

- **See everything:** open `.claude/codebase-memory.json` — it's human-readable.
- **Start over:** empty the file (`: > .claude/codebase-memory.json`) or delete it;
  the server recreates it on the next write.

## Why the launcher script?

The upstream memory server resolves a *relative* `MEMORY_FILE_PATH` against its own
temporary `npx` install directory, not the project. `scripts/codebase-memory-server.sh`
computes an **absolute** path anchored to this repo, guaranteeing the memory always
lands at `.claude/codebase-memory.json` no matter where the server is launched from.

---

← Back to the [**MAZ ARCADE hub**](../index.html) · [repository README](../README.md)
