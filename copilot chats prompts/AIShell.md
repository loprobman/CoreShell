# CoreShell ShellAI Design and Test Guide

## Scope

This document describes the Week 10 ShellAI implementation in CoreShell:

- Natural-language mode via @ input
- Command catalog export via --commands-json
- Confirmation gate before execution
- Grounding and safety validation of suggestions
- Audit logging of suggestion and execution events
- Automated tests for all the above

It also provides a step-by-step test procedure to verify behavior manually and through the test suite.

## Architecture Overview

### High-level flow

1. User enters a natural-language request beginning with @ in CoreShell.
2. CoreShell extracts the query and calls the helper process coresh_llm.
3. coresh_llm returns exactly one suggested command line.
4. CoreShell prints Suggested command and asks for confirmation.
5. If confirmed, CoreShell validates the suggestion against safety and catalog rules.
6. If validation passes, CoreShell executes through internal dispatch.
7. CoreShell writes audit log events to artifacts/shellai.log.

### Components

#### 1) CoreShell main runtime

Responsibilities:

- Detect @ lines in interactive and argv modes.
- Spawn helper process and capture one-line response.
- Show confirmation prompt.
- Validate the suggested command.
- Execute only if valid and confirmed.
- Emit structured audit logs.
- Export command catalog JSON.

Key behavior:

- --commands-json outputs commands metadata including usage and options.
- Suggested commands are blocked if:
	- command is not in CoreShell registry,
	- option is not valid for that command,
	- forbidden shell syntax is present.

#### 2) Helper process coresh_llm

Responsibilities:

- Receive natural-language query.
- Try planner endpoint when available.
- Fallback deterministically to local rule-based mapping.
- Print one command suggestion line.

#### 3) Command registry and JSON help bridge

Responsibilities:

- Register all built-in commands.
- Provide command metadata used for grounding and docs.
- Emit option metadata fields used by assignment schema.

Current option schema includes:

- short
- long
- arg
- help

Compatibility fields are also still present.

## Safety and Grounding Rules

The validator enforces all of the following before execution:

1. Suggestion is non-empty and parseable.
2. Command exists in CoreShell command registry.
3. Suggested options appear in command usage/help metadata.
4. Forbidden metacharacter patterns are rejected.

If a rule fails:

- command is blocked,
- user sees a block reason,
- decision is logged as blocked.

## Logging Model

Log file:

- artifacts/shellai.log

Format:

- JSON Lines (one JSON object per event)

Typical events:

- suggestion
- decision (cancelled or blocked)
- executed
- suggestion_error

Event fields:

- ts
- event
- query
- suggested
- decision
- status
- message

## Commands Catalog Output

Run:

```bash
./CoreShell --commands-json
```

Top-level shape:

```json
{
	"commands": [
		{
			"name": "...",
			"summary": "...",
			"description": "...",
			"usage": "...",
			"options": [
				{
					"short": "...",
					"long": "...",
					"arg": "...",
					"help": "..."
				}
			]
		}
	]
}
```

## Step-by-Step Testing Guide

### A) Build and baseline tests

1. Build:

```bash
make
```

2. Run full suite:

```bash
make test
```

3. Confirm ShellAI tests pass in test output log:

- shellai: --commands-json includes top-level commands array
- shellai: --commands-json includes usage and options keys
- shellai: --commands-json includes short/long/arg/help option schema
- shellai: blocks non-catalog suggested command
- shellai: blocks suggested command with invalid option
- shellai log checks

### B) Validate catalog export manually

1. Print catalog header and first entries:

```bash
./CoreShell --commands-json | head -40
```

2. Verify each command entry includes:

- name
- summary
- description
- usage
- options with short/long/arg/help

### C) Validate happy-path @ execution

1. Start shell:

```bash
./CoreShell
```

2. Enter:

```text
@where am i
```

3. Confirm prompt appears:

```text
Suggested command: ...
Run this? (y/n)
```

4. Enter y and confirm command executes.

5. Check last log lines:

```bash
tail -n 5 artifacts/shellai.log
```

Expected: suggestion and executed events.

### D) Validate cancellation path

1. In shell:

```text
@where am i
```

2. At prompt enter n.

3. Confirm command is not executed.

4. Check log:

```bash
tail -n 5 artifacts/shellai.log
```

Expected: decision event with cancelled.

### E) Validate blocked non-catalog command path

1. In shell enter a query that tends to produce external-only commands, for example:

```text
@find files in this directory
```

2. Enter y when prompted.

3. Confirm CoreShell prints blocked suggested command with reason.

4. Check log for blocked decision:

```bash
tail -n 10 artifacts/shellai.log
```

Expected: suggestion plus decision blocked.

### F) Validate blocked invalid-option path

This is best covered via automated test (planner mock returning an invalid option).

1. Run test suite:

```bash
make test
```

2. Confirm pass lines:

- shellai: blocks suggested command with invalid option
- shellai log: invalid option block reason is logged

## Troubleshooting

### No response from helper

- Ensure coresh_llm is built:

```bash
make
```

- Ensure it is executable and discoverable from CoreShell runtime location.

### Catalog output missing fields

- Rebuild after latest source updates:

```bash
make clean && make
```

- Re-run:

```bash
./CoreShell --commands-json | head -60
```

### Logs not appearing

- Ensure artifacts directory is writable.
- Trigger one @ query and inspect:

```bash
tail -n 20 artifacts/shellai.log
```

## References

- artifacts/week10_assignment_verification.md
- test_output.log
- tests/test_runner.c
- tests/at_query_test.c
