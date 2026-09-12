#!/bin/sh
# Launcher for the codebase-memory MCP server.
#
# Why this wrapper exists: the upstream @modelcontextprotocol/server-memory
# resolves a *relative* MEMORY_FILE_PATH against its own (temporary, npx) install
# directory rather than the project. We compute an absolute path anchored to this
# repo so the knowledge graph always lives at <repo>/.claude/codebase-memory.json
# and is committed alongside the code.
set -e
REPO_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
export MEMORY_FILE_PATH="$REPO_ROOT/.claude/codebase-memory.json"
exec npx -y @modelcontextprotocol/server-memory
