# Week 10 Assignment Verification (ShellAI in CoreShell)

Date: 2026-06-14

## How to Reproduce

```bash
make test
./CoreShell --commands-json | head -40
```

Primary test evidence is in [test_output.log](test_output.log) and implementation/tests are in [main.c](main.c), [cmd_registry/cmd_registry.c](cmd_registry/cmd_registry.c), and [tests/test_runner.c](tests/test_runner.c).

## Acceptance Check Matrix

1. @ mode returns one command suggestion line only
- Evidence:
  - @ integration test passes in [tests/at_query_test.c](tests/at_query_test.c)
  - It asserts presence of `Suggested command:` and confirmation prompt.
  - Test output shows pass in [test_output.log](test_output.log).

2. Suggested command uses known catalog commands/options
- Evidence:
  - Catalog export exists via `--commands-json` in [main.c](main.c).
  - Suggestion validation gate in [main.c](main.c) blocks:
    - commands not in registry
    - options not declared for that command
    - forbidden shell metachar syntax
  - Automated checks in [tests/test_runner.c](tests/test_runner.c):
    - `shellai: blocks non-catalog suggested command`
    - `shellai: blocks suggested command with invalid option`

3. Confirmation gate prevents accidental destructive execution
- Evidence:
  - Explicit user prompt `Run this? (y/n)` in [main.c](main.c).
  - Non-yes response cancels execution path in [main.c](main.c).
  - Existing @ test confirms prompt behavior in [tests/at_query_test.c](tests/at_query_test.c).

4. Deterministic fallback behavior when remote planner/LLM is unavailable
- Evidence:
  - Helper fallback path in [coresh_llm.c](coresh_llm.c) (`mock_llm`).
  - @ integration test accepts deterministic local outcome (`pwd` or fallback marker) in [tests/at_query_test.c](tests/at_query_test.c).

5. Command catalog export includes required structure
- Evidence:
  - `./CoreShell --commands-json` outputs top-level `commands` array from [main.c](main.c).
  - Each command includes `name`, `summary`, `description`, `usage`, `options`.
  - Option schema includes `short`, `long`, `arg`, `help` (plus existing compatibility fields) from [cmd_registry/cmd_registry.c](cmd_registry/cmd_registry.c) and [main.c](main.c).
  - Automated checks in [tests/test_runner.c](tests/test_runner.c):
    - `shellai: --commands-json includes top-level commands array`
    - `shellai: --commands-json includes usage and options keys`
    - `shellai: --commands-json includes short/long/arg/help option schema`

6. Logging of suggested/executed commands
- Evidence:
  - JSONL audit logger writes to [artifacts/shellai.log](artifacts/shellai.log).
  - Events include suggestion, decision (including blocked), executed.
  - Automated checks in [tests/test_runner.c](tests/test_runner.c):
    - `shellai log: blocked command records suggestion and blocked decision`
    - `shellai log: invalid option block reason is logged`

## Current Result Snapshot

From [test_output.log](test_output.log):
- `Results: 182 passed / 0 failed / 182 total (100%)`
- All ShellAI-specific checks pass.
