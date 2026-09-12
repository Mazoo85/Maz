#!/usr/bin/env python3
"""Generate docs/API.md — a browsable API reference — straight from the engine headers.

The Maz headers each carry a rich module-level doc comment (the paragraph next to their
`namespace maz::…` line) plus clearly-named public types and free functions. This script harvests
those, groups them by subsystem (core, render, game, …), and emits one Markdown reference with a
table of contents. It needs no external tools (unlike Doxygen), so it runs anywhere and in CI, and
the output is diff-reviewable. Run: python3 tools/gen_api_docs.py
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
INC = ROOT / "engine" / "include" / "maz"
OUT = ROOT / "docs" / "API.md"

# Friendly names for each subsystem directory.
SUBSYSTEMS = {
    "core": "Core — foundation (time, jobs, events, RNG, resources, config)",
    "math": "Math — vectors, matrices, transforms, curves, geometry",
    "platform": "Platform — window, input, filesystem, crash handling",
    "render": "Render — Vulkan renderer, sprites, meshes, shapes, cameras",
    "scene": "Scene — node tree, transforms, serialization",
    "script": "Script — the maz::script language VM + engine binding",
    "ecs": "ECS — entity/component/system world",
    "game": "Game — physics, collision, AI, pathfinding, tilemaps",
    "anim": "Anim — skeletons, clips, blending, tweening, curves",
    "audio": "Audio — mixer, DSP effects, spatialization, synthesis",
    "ui": "UI — controls, layout, theming, text",
    "fx": "FX — particles and force fields",
    "io": "IO — JSON, config, serialization, resource packs",
    "input": "Input — action maps, analog helpers",
    "editor": "Editor — scene model, gizmos, inspector",
}

# A type declaration at column 0, optionally preceded by a `template<…>` on the same line.
TYPE_RE = re.compile(r"^(?:template\s*<[^>]*>\s*)?(class|struct|enum class)\s+([A-Za-z_]\w*)")
# A top-level (column 0) inline free function: `inline <ret> name(...)`.
FUNC_RE = re.compile(r"^inline\s+.*?\b([A-Za-z_]\w*)\s*\(")


def extract_module_doc(lines):
    """The module summary: the // comment block adjacent to the `namespace maz::` line."""
    ns_idx = next((i for i, ln in enumerate(lines) if ln.startswith("namespace maz")), None)
    if ns_idx is None:
        return ""
    # Prefer the comment block immediately ABOVE the namespace line.
    up = []
    i = ns_idx - 1
    while i >= 0 and lines[i].strip().startswith("//"):
        up.append(lines[i].strip()[2:].strip())
        i -= 1
    if up:
        up.reverse()
        return " ".join(up).strip()
    # Otherwise the block immediately BELOW it.
    down = []
    j = ns_idx + 1
    while j < len(lines) and not lines[j].strip():
        j += 1
    while j < len(lines) and lines[j].strip().startswith("//"):
        down.append(lines[j].strip()[2:].strip())
        j += 1
    return " ".join(down).strip()


def parse_header(path):
    text = path.read_text(encoding="utf-8", errors="replace")
    lines = text.splitlines()
    doc = extract_module_doc(lines)
    types, funcs = [], []
    for ln in lines:
        m = TYPE_RE.match(ln)
        if m:
            name = m.group(2)
            if not name.endswith(";"):  # skip forward decls handled below
                kind = m.group(1)
                # Skip pure forward declarations like `class Foo;`.
                if ln.rstrip().endswith(";"):
                    continue
                types.append((kind, name))
            continue
        fm = FUNC_RE.match(ln)
        if fm:
            sig = ln.split("{")[0].rstrip().rstrip(";").strip()
            funcs.append(sig)
    # De-dup, preserve order.
    seen = set()
    types = [(k, n) for (k, n) in types if not (n in seen or seen.add(n))]
    return doc, types, funcs


def main():
    if not INC.is_dir():
        print(f"error: {INC} not found", file=sys.stderr)
        return 1

    # Group headers by subsystem directory.
    groups = {}
    for hpp in sorted(INC.rglob("*.hpp")):
        rel = hpp.relative_to(INC)
        sub = rel.parts[0] if len(rel.parts) > 1 else "(root)"
        groups.setdefault(sub, []).append(hpp)

    out = []
    out.append("# Maz Engine — API Reference\n")
    out.append(
        "> Auto-generated from the engine headers by `tools/gen_api_docs.py`. Each module's summary "
        "is its header's own doc comment; the type and function lists are its public surface. This "
        "is a map — read the header for full signatures and semantics.\n"
    )
    total_headers = sum(len(v) for v in groups.values())
    out.append(f"_{total_headers} headers across {len(groups)} subsystems._\n")

    # Table of contents in the friendly order, then any leftovers.
    order = [s for s in SUBSYSTEMS if s in groups] + [s for s in groups if s not in SUBSYSTEMS]
    out.append("## Contents\n")
    for sub in order:
        title = SUBSYSTEMS.get(sub, f"`{sub}`")
        anchor = sub.replace("(", "").replace(")", "")
        out.append(f"- [{title}](#{anchor})")
    out.append("")

    for sub in order:
        title = SUBSYSTEMS.get(sub, sub)
        out.append(f'<a name="{sub.replace("(", "").replace(")", "")}"></a>')
        out.append(f"## {title}\n")
        for hpp in groups[sub]:
            doc, types, funcs = parse_header(hpp)
            rel = hpp.relative_to(ROOT)
            out.append(f"### `{hpp.stem}`")
            out.append(f"<sub>`{rel}`</sub>\n")
            if doc:
                out.append(doc + "\n")
            if types:
                names = ", ".join(f"`{n}`" for _, n in types)
                out.append(f"**Types:** {names}\n")
            if funcs:
                shown = funcs[:12]
                items = "\n".join(f"- `{s}`" for s in shown)
                out.append("**Functions:**\n")
                out.append(items)
                if len(funcs) > len(shown):
                    out.append(f"- _…and {len(funcs) - len(shown)} more_")
                out.append("")
        out.append("")

    OUT.write_text("\n".join(out) + "\n", encoding="utf-8")
    # Report a small summary for CI logs / humans.
    ntypes = sum(len(parse_header(h)[1]) for g in groups.values() for h in g)
    print(f"wrote {OUT.relative_to(ROOT)}: {total_headers} headers, {len(groups)} subsystems, "
          f"{ntypes} public types documented")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
