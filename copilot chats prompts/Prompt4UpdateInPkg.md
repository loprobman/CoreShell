# Prompt: Add Update Support to `pkg.c`

Extend `pkg.c` to support two new subcommands:

1. `pkg check-update <name>`
2. `pkg upgrade <name>`

## Context And Compatibility

- Keep the current package DB location used by this project: `~/.CoreShell/pkgdb.txt`.
- Reuse existing install flow in `pkg.c` (the same path used by `pkg install <archive.tar.gz>`).
- Keep changes minimal and focused on update/upgrade behavior.

## Registry Behavior

- Query registry endpoint: `http://localhost:3000/packages/<name>`
- Parse JSON response and read:
  - `latestVersion`
  - `downloadUrl`
- Exact package-name matching should remain case-sensitive.

## Version Comparison Rule

- Compare versions as dotted numeric parts (`major.minor.patch...`).
- Missing parts are treated as `0`.
- Examples:
  - `1.10.0 > 1.2.0`
  - `1.2 == 1.2.0`

## Subcommand Requirements

### 1) `pkg check-update <name>`

- Read installed version for `<name>` from `~/.CoreShell/pkgdb.txt`.
- If not installed locally: print error and return exit code `1`.
- If registry returns not found: print error and return exit code `1`.
- If `latestVersion > installed`: print update available and return exit code `0`.
- If `latestVersion <= installed`: print already up-to-date and return exit code `0`.

### 2) `pkg upgrade <name>`

- First perform the same check logic as `check-update`.
- If no update is available: print no-op message and return exit code `0`.
- If update is available:
  - download archive from `downloadUrl` to a temp file in `/tmp`
  - use `curl` with fail-fast behavior (equivalent to: `--fail --location --silent --show-error --max-time 15`)
  - invoke existing install functionality with the downloaded archive path
  - remove temp file on both success and failure
- On any failure (network, bad JSON, missing fields, install failure): print clear error and return exit code `1`.

## Implementation Constraints

- Simple JSON parsing is acceptable (string search/token parsing).
- Use `popen` or similar to run/capture `curl` output.
- Avoid introducing new third-party C dependencies.

## CLI Integration

- Update subcommand dispatch so both new commands are executable.
- Update help/usage text to include `check-update` and `upgrade`.

## Output And Deliverables

- Show only new or modified functions in `pkg.c`.
- Explain in `5-10` lines how `check-update` and `upgrade` work.

## Validation Expectations

Include a brief test/verification summary covering at least:

- `check-update` when update exists
- `check-update` when already current
- `check-update` for unknown package
- `upgrade` success path
- `upgrade` no-update path
- `upgrade` failure cleanup path