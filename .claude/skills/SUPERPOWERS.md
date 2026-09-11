# Superpowers skills

The following skills in this directory are vendored from the **Superpowers**
plugin by Jesse Vincent (obra):

- brainstorming
- dispatching-parallel-agents
- executing-plans
- finishing-a-development-branch
- receiving-code-review
- requesting-code-review
- subagent-driven-development
- systematic-debugging
- test-driven-development
- using-git-worktrees
- using-superpowers
- verification-before-completion
- writing-plans
- writing-skills

Source: https://github.com/obra/superpowers
Version: 6.1.1
License: MIT (Copyright (c) 2025 Jesse Vincent)

They are placed under `.claude/skills/` so Claude Code auto-discovers and loads
them for this repo (the same mechanism the existing `crew` skill uses). Start
with the `using-superpowers` skill.

## Updating

To pull a newer version:

```sh
git clone --depth 1 https://github.com/obra/superpowers.git /tmp/superpowers
cp -R /tmp/superpowers/skills/* .claude/skills/
```
