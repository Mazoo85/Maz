# Contributing to Maz

Thanks for your interest in improving Maz. This guide covers how to build, test, and submit changes.
By participating you agree to abide by our [Code of Conduct](CODE_OF_CONDUCT.md).

## Quick start

```bash
# Configure + build (Ninja optional but recommended)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel

# Run the full test suite
ctest --test-dir build --output-on-failure

# Run one test while iterating
ctest --test-dir build -R gpu_particles --output-on-failure
```

Dependencies (GLM, SDL3, stb, cgltf, …) are fetched automatically by CMake — you don't install them by
hand. On Linux you need the system dev packages the CI installs; see `.github/workflows/ci.yml` for the
exact `apt-get` list.

## How the project is organized

- `engine/include/maz/…` — the engine, as **header-only `maz::` modules** grouped by namespace
  (`render::`, `core::`, `game::`, `audio::`, `anim::`, `ui::`, `io::`, `net::`, `platform::`, `ecs::`,
  `fx::`, `scene::`, `ext::`, `math::`). Most new features are a single self-contained header.
- `engine/src/…` — the few compiled units (Vulkan renderer, SDL platform/audio glue).
- `apps/…` — small sample games/demos, one folder each; the best place to learn the API.
- `tests/…` — the ctest suite; every logic change should land with a test here.
- `tools/…` — build/packaging/docs scripts.
- `docs/…` — architecture, roadmap, guides.

## Coding style

- **C++20**, header-only where practical. Prefer adding a new `maz::<area>::Thing.hpp` over expanding a
  monolith.
- **No new required third-party dependencies** without discussion — a core value of Maz is that it builds
  from source with almost nothing preinstalled.
- Code must compile clean under the project's warnings-as-errors:
  `-Wall -Wextra -Wconversion -Wshadow -Wshorten-64-to-32 -Werror` (configure with `-DMAZ_WERROR=ON`).
  `.clang-tidy` runs in CI as a lint gate over `engine/src`.
- Match the surrounding code: 4-space indent, braces on the same line, `m_` member prefix in classes,
  `snake_case` files named after the type, doc-comment blocks at the top of each header explaining *what*
  and *why* (see any existing header for the house style).
- Keep functions honest and small; name things by what they do.

## Testing

- Add a test under `tests/<area>/` and register it in `tests/CMakeLists.txt` (copy an existing 3-line
  block: `add_executable` + `target_link_libraries(... maz::maz maz_warnings)` + `add_test`).
- Prefer **verifiable, self-checking** tests: assert against hand-computed values or invariants, not just
  "it ran". Rendering/audio features that need a GPU/device should test the CPU-side math/data and note
  the visual step as owner-verified.
- Run `ctest --test-dir build` locally before pushing.

## Submitting a pull request

1. Fork and branch from the default branch.
2. Make one focused change. Keep unrelated formatting out of the diff.
3. Ensure `cmake --build build` (with `-DMAZ_WERROR=ON`) and `ctest` both pass.
4. Update `docs/` (and `docs/GODOT_GAPS_ROADMAP.md`) if you added or changed a capability.
5. Open the PR using the template; describe what and why, and how you tested it.

CI runs the full Linux build + test suite, a clang-tidy lint gate, sanitizers (ASan/UBSan), and
macOS/Windows compile coverage. All must be green to merge.

## Reporting bugs / requesting features

Use the issue templates (Bug report / Feature request). A minimal reproducer and the exact commands you
ran make bugs far faster to fix.

## Releases

Tagging `v<major>.<minor>.<patch>` triggers `.github/workflows/release.yml`, which builds, runs the
tests, packages the sample games with `tools/package.sh`, and attaches the bundles to a GitHub Release.
See [docs/RELEASING.md](docs/RELEASING.md).
