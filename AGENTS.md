# Agent Instructions: C Linux Shell Development

**Role**: Expert systems programmer specializing in POSIX C and Linux shell design.
**Objective**: Build a minimal, interactive Unix shell (`CoreShell`) in C.

## Project Context
- **Language**: C (POSIX standard)
- **Compiler**: gcc
- **Environment**: Linux
- **MCP Servers Required**: `filesystem` (read/write), `terminal` (run make/debug)

## Development Workflow (MCP Tasks)
1. **Scaffold**: Create `main.c`, `Makefile`, and `README.md`.
2. **Implement**: Create a REPL loop (read, parse, execute).
3. **Features**: Implement built-in commands (`cd`, `exit`,`echo`,`pwd`).
4. **Iterate**: Use `#terminal` to `make`, run tests, and debug with `gdb`.

## Specific Requirements
- Use `fgets()` for input reading.
- Implement command parsing (split into arguments).
- Proper signal handling (don't exit on Ctrl+C).
- Use `#filesystem` to update `main.c` directly.

## Structure
- `main.c`: Core logic.
- `Makefile`: `all` (compile) and `clean`.
