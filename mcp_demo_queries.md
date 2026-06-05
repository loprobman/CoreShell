# MCP Demo Queries (3 NL prompts)

## Prerequisites

1. Start server: `./mcp_server`
	(build first if needed: `make mcp_server`)
2. Run agent script: `python3 agent_mcp_example.py`

## Query 1

Prompt: `Show available MCP tools.`
Expected: list includes `registry.packages.list`, `registry.package.lookup`, `shell.commands.list`, `shell.command.help`, `shell.command.run`, `filesystem.delete_older_than_days`.

## Query 2

Prompt: `List CoreShell commands and show help for rpc.`
Expected: command catalog plus docs/metadata for `rpc`.

## Query 3

Prompt: `Delete files older than 30 days in artifacts, but do a dry run first.`
Expected: tool call to `filesystem.delete_older_than_days` with `dryRun=true` and a preview of matched files.

## Logging

All MCP request/response pairs are logged in `artifacts/mcp_calls.log` as JSON lines.

## Transcript Artifact

The captured output of these three queries is stored in `mcp_demo_transcript.md`.
