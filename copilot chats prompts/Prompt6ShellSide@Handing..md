# Prompt 6: Shell-Side `@` Handling for LLM Integration

## Objective

Extend CoreShell to support natural language commands prefixed with `@` by integrating with an external LLM helper.

## Requirements

### Input & Detection
- If the first character of the input line is `@`, the rest is a **natural language query** (the `@` is stripped).
- Pass the query (without `@`) to the helper program.

### Helper Program Integration
- The shell calls an external helper program `coresh_llm` via `fork()` and `execvp()`.
- Pass the natural language query as the first command-line argument to `coresh_llm`.
- Communicate via a pipe: read exactly **one line** of output from `coresh_llm`'s stdout.
- This line is the suggested shell command.

### User Confirmation Flow
1. **Display the suggested command** to the user (e.g., `Suggested command: ls -lt *.c`).
2. **Ask for confirmation** with a prompt like `Run this? (y/n)`.
3. **Execute** the command using existing CoreShell command execution logic if user confirms.
4. **Skip execution** if the user declines or if `coresh_llm` fails.

### Process Communication
- The parent (shell) creates a pipe before forking.
- The child process (`coresh_llm`) inherits the write end of the pipe and redirects its stdout.
- The parent closes the write end and reads from the read end.
- Both processes handle pipe cleanup and errors gracefully.

## Implementation Deliverables

### 1. Core Function: `handle_llm_line(const char *query)`
- **Input**: `query` — the natural language text (already without the `@` prefix).
- **Output**: Executes or skips the suggested command based on user confirmation.
- **Implementation**: Uses `fork()`, `pipe()`, and `execvp()` to call `coresh_llm` and capture its output.

### 2. REPL Integration
- Sketch the location and logic for detecting `@` in the main event loop.
- Show how to strip the `@`, extract the query, and call `handle_llm_line()`.

### 3. Process Communication Explanation
- Describe the pipe setup, redirection, and cleanup.
- Explain how the parent waits for the child and retrieves the suggested command.
- Note any error handling or edge cases (e.g., malformed input, helper not found, empty response).