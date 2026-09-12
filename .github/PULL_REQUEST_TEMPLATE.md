<!-- Thanks for contributing to Maz! Fill in the sections below. Keep PRs focused — one logical
     change per PR is much easier to review than a mixed bag. -->

## Summary

<!-- What does this PR do, and why? One or two sentences. -->

## Changes

<!-- Bullet the notable changes. Reference files/modules where useful. -->
-

## Related issue

<!-- e.g. "Closes #123". Omit if none. -->

## How was this tested?

<!-- The exact commands you ran. New behavior should come with a test (see CONTRIBUTING.md). -->
- [ ] `cmake -S . -B build && cmake --build build`
- [ ] `ctest --test-dir build --output-on-failure`
- [ ] Added/updated a test for this change

## Checklist

- [ ] Builds clean with warnings-as-errors (`-DMAZ_WERROR=ON`)
- [ ] Follows the code style in `CONTRIBUTING.md` (header-only `maz::` modules, no new required deps)
- [ ] Docs / `docs/GODOT_GAPS_ROADMAP.md` updated if this adds or changes a capability
- [ ] No unrelated formatting churn
