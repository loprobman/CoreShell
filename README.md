# CoreShell — Simple Linux Shell

A minimal, interactive Unix shell implemented in C (POSIX standard).

## Building

```bash
make        # Compile the shell (CoreShell), package manager (pkg/pkg), and LLM helper (coresh_llm)
make debug  # Compile with debug symbols (-g -O0) for use with gdb
make clean  # Remove all compiled objects, binaries, and test reports
```

## Running

```bash
./CoreShell
```

Once in the shell, you can use natural language commands with the `@` prefix to invoke the LLM helper:

```bash
user@CoreShell> @list all C files in this directory sorted by modification time
Suggested command: find . -type f -name '*.c'
Run this? (y/n) y
```

This feature requires the `coresh_llm` helper program to be available in your `$PATH`.

## Node Registry Server

CoreShell also includes a minimal Node.js + Express package registry server.

### Install dependencies

```bash
npm install
```

### Start the registry server

```bash
npm start
```

The server listens on port `3000` and exposes:

- `GET /packages`
- `GET /packages/:name`

### Run Node endpoint tests

```bash
npm test
```

## MCP-Compatible Service Layer

CoreShell provides two MCP-compatible servers:

- Native C server (slide-literal path): `./mcp_server` on `127.0.0.1:9000`
- Node compatibility bridge (used by Node tests): `server.js` (`createMcpServer()`)

Both use line-based JSON with `tools/list` and `tools/call`.

Build and launch the native C server with:

```bash
make mcp_server
./mcp_server
```

This runs the native C MCP server on port `9000`.

### Protocol

- One JSON object per line
- Requests use `type: "tools/list"` or `type: "tools/call"`
- Responses are JSON objects with a stable `ok` field and either `tools` or `result`
- Native C server now supports persistent TCP sessions (multiple request/response lines per connection)
- Native C server now uses structured JSON request parsing (jsmn)

Compatibility mode (for classroom MCP examples) is also supported by the native C server:

- `method: "initialize"`
- `method: "list_tools"`
- `method: "call_tool"` with tools `list_files`, `get_time`, `delete_older_than_days`

In compatibility mode, responses echo request `id` and use a notebook-style envelope (`type: "response"`).
For long-running legacy tool calls (for example `list_files`, `delete_older_than_days`), the native server may emit notification lines before the final response line.

### Tools

| Tool | Description |
|---|---|
| `registry.packages.list` | list all known packages |
| `registry.package.lookup` | look up one package by `name` |
| `shell.commands.list` | list CoreShell commands and their docs paths |
| `shell.command.help` | return the help metadata for one command |
| `shell.command.run` | run allowlisted shell commands (native C: `echo`, `pwd`, `help`; Node bridge additionally supports `ls`, `cat`, `stat`, `head`) |
| `filesystem.delete_older_than_days` | delete files older than `days` under a workspace path (supports `dryRun`) |
| `rag.docs.search` | retrieve top command-doc matches for a natural-language query |
| `rag.command.recommend` | return one grounded command recommendation with citation paths |

### Example

```bash
printf '{"type":"tools/list"}\n' | nc 127.0.0.1 9000

printf '{"type":"tools/call","tool":"registry.package.lookup","arguments":{"name":"echo"}}\n' | nc 127.0.0.1 9000

printf '{"type":"tools/call","tool":"filesystem.delete_older_than_days","arguments":{"path":"artifacts","days":30,"dryRun":true}}\n' | nc 127.0.0.1 9000

printf '{"type":"tools/call","tool":"rag.docs.search","arguments":{"query":"print working directory","topK":3}}\n' | nc 127.0.0.1 9000

printf '{"type":"tools/call","tool":"rag.command.recommend","arguments":{"query":"How do I print my working directory?"}}\n' | nc 127.0.0.1 9000
```

### Node tests

```bash
npm test
```

The Node test suite now covers both the HTTP registry and the MCP-compatible
socket interface, including the shell command catalog, help bridge, and the
read-only command runner.

All MCP request/response events are logged to `artifacts/mcp_calls.log` as
JSON lines.

## Agent Demo (Slide Assignment)

- Example agent script: `agent_mcp_example.py`
- Notebook-style legacy MCP client example: `legacy_mcp_client.py`
- Demo prompt set: `mcp_demo_queries.md`
- Captured run transcript: `mcp_demo_transcript.md`

Run the compatibility client with:

```bash
python3 legacy_mcp_client.py --path artifacts --days 30
```

Expected output shape (abbreviated):

```text
=== initialize ===
response:
{
  "id": 1,
  "type": "response",
  "result": {
    "server": "CoreShell MCP Server",
    "version": "1.0"
  }
}

=== list_tools ===
response:
{
  "id": 2,
  "type": "response",
  "result": {
    "tools": [
      {"name": "list_files", ...},
      {"name": "get_time", ...},
      {"name": "delete_older_than_days", ...}
    ]
  }
}

=== call_tool:get_time ===
response:
{
  "id": 4,
  "type": "response",
  "result": {
    "tool": "get_time",
    "time": "YYYY-MM-DD HH:MM:SS"
  }
}
```

## Testing

```bash
make test                   # Build CoreShell + test runner, then execute all test cases
make test-mcp-c             # Build and run native C MCP server protocol tests
./test_runner               # Run already-built test binary directly
```

`make test-mcp-c` includes strict legacy integration checks for:

- notebook-style methods (`initialize`, `list_tools`, `call_tool`)
- persistent multi-message session behavior on one socket
- notification line + final response sequencing

`make test` compiles a standalone C test runner (`test_runner`) that links directly
against the built-in command objects, forks an isolated child per test case, and
captures stdout/stderr via pipes.  On completion it produces:

| Output file | Description |
|---|---|
| `test_report.md` | Markdown table with pass/fail status, exit codes, and stdout snippets for every test |
| `test_output.log` | Plain-text log (no ANSI colour) suitable for CI or file review |

Both files are regenerated on every run and removed by `make clean`.

### Test suites (170 test cases)

| Suite | Cases | What is tested |
|---|---|---|
| `test_help` | 3 | help command listing, per-command usage, unknown command error |
| `test_exit` | 2 | `--help` output, clean exit status |
| `test_cd` | 4 | valid path, nonexistent path, no-args ($HOME), `--help` |
| `test_pwd` | 2 | prints `/`, `--help` |
| `test_echo` | 5 | no args, multi-string, `-n`, `-e` escapes, `--help` |
| `test_ls` | 6 | directory listing, `-a`, `-l`, `-la`, error, `--help` |
| `test_stat` | 3 | file size output, error, `--help` |
| `test_cat` | 4 | full content, empty file, error, `--help` |
| `test_head` | 5 | default 10 lines, `-n 2`, `-n 1`, error, `--help` |
| `test_tail` | 5 | default 10 lines, `-n 2`, `-n 1`, error, `--help` |
| `test_cp` | 3 | file copy, error, `--help` |
| `test_mv` | 3 | rename, error, `--help` |
| `test_rm` | 3 | remove, error, `--help` |
| `test_mkdir` | 3 | create, existing dir error, `--help` |
| `test_rmdir` | 3 | remove empty, error, `--help` |
| `test_touch` | 3 | create, update timestamp, `--help` |
| `test_rpc` | 6 | help/help-json, validation, timeout path, local line-protocol success, stable json keys |
| `test_cmd_spec_metadata` | 20 | all 20 commands have name, summary, long_help, run, print_usage set |
| `test_pkg_json` | 18 | all 18 `pkg.json` files contain all 6 required fields (incl. `files`) |
| `test_docs_md` | 18 | all 18 `docs/<name>.md` files contain `## Usage` and `## Options` |
| `test_multicall_dispatch` | 6 | Mode 2 (argv[1]), unknown command error, Mode 1 (symlink) |
| `test_pwd_help_format` | 2 | `--logical` and `--physical` appear in help (format bug regression) |
| `test_json_flags` | 6 | `--help-json` emits schema with `name`/`options`; `--json` emits result key |
| `test_pkg_binary` | 5 | `pkg --help`, `pkg list`, `pkg build` (missing args error), `pkg install` (bad archive), multicall dispatch |
| `test_jobs` | 4 | `jobs` with no jobs, `jobs` after background launch, `kill %N` terminates job, `kill --help` |
| `test_repl_parser` | 4 | interactive `./CoreShell` input, option tokens in pipelines, redirected file output, absolute-path redirection |

## Debugging

```bash
make debug
gdb ./CoreShell
```

## Week 8 Baseline: Socket Client Command

CoreShell includes a Week 8 baseline socket-client built-in named `rpc`.

### Goals covered

- Client API pattern: `socket() -> connect() -> send()/recv() -> close()`
- Explicit timeout and retry controls to avoid hanging shell sessions
- Line-based minimal protocol (`request\\n` -> `response\\n`)
- Input validation and bounded reads/writes
- Help/registry integration consistent with command anatomy
- Optional `--json` output with stable keys

### Usage

```bash
rpc [-h] [--help-json] [--json] [-H HOST] [-p PORT] [-t SECONDS] [-r RETRIES] <message>
```

### Examples

```bash
# Simple request to local service
rpc "ping"

# Custom host/port with timeout and retries
rpc -H 127.0.0.1 -p 5555 -t 2 -r 2 "health"

# Structured output
rpc --json -H 127.0.0.1 -p 5555 "status"
```

### JSON output schema (stable)

```json
{
  "ok": true,
  "host": "127.0.0.1",
  "port": 5555,
  "attempts": 1,
  "response": "ACK:ping",
  "error": null
}
```

On errors, `ok` is `false`, `response` is `null`, and `error` contains a readable error string.

## Threading Architecture (Week Five)

CoreShell uses **POSIX pthreads** for built-in command execution to satisfy embedded systems requirements:

- **Built-in commands** execute in worker threads with status returned via system pipes.
- **External commands** remain process-based (fork/exec).
- **Shell pipelines** (`cmd1 | cmd2`) continue to use process-based execution with Unix pipes.

### How It Works

Each built-in command (e.g., `pwd`, `echo`, `ls`) runs in its own worker thread:

1. Main REPL thread parses the command.
2. Creates a status pipe: `pipe(status_fd)`.
3. Spawns a worker thread with the command context.
4. Worker thread executes the command and writes its exit code to the pipe.
5. Main thread reads the status, waits for thread completion, and returns to the prompt.

This achieves:
- ✓ Concurrency model for internal command execution (pthread).
- ✓ Inter-thread communication via system pipes.
- ✓ External commands via separate processes (fork/exec).
- ✓ Shell pipelines remain process-safe and Unix-compatible.

### Example: Single Built-in

```bash
$ ./CoreShell pwd
/home/user/CoreShell

$ ./CoreShell echo hello
hello
```

Both commands execute in worker threads and return their status through pipes.

### Example: External Command (No Threading)

```bash
$ ./CoreShell /bin/ls -la
```

External commands are detected at runtime (not in registry) and executed via fork/exec—no threading.

### Example: Pipeline (Process-Based, Not Threaded)

```bash
$ ./CoreShell
...
user@CoreShell> echo hello | cat
hello
```

Even though `echo` and `cat` are built-ins, they execute in **separate forked processes** when in a pipeline. This preserves Unix pipe semantics and shell correctness.

### Implementation Details

- **Compiler flag**: `-pthread` added to `CFLAGS` in the Makefile.
- **Main code**: `dispatch_builtin()` in `main.c` handles thread spawning and pipe I/O.
- **Documentation**: See [tests/Threads.md](tests/Threads.md) for detailed design, architecture diagrams, and step-by-step exercises.

### Testing Threads

```bash
$ make test
Running CoreShell test suite...
...
```

The test runner internally uses the same threading mechanism. Each test case triggers the threaded dispatch, validates output, and confirms the exit status was correctly propagated through the pipe.

---

## Natural Language Commands with `@`

CoreShell supports an optional **natural language mode** that bridges human language and shell commands via an external LLM helper.

### How it works

1. Type a command starting with `@` in the interactive shell:
   ```bash
   user@CoreShell> @find all PDF files larger than 5MB

CoreShell now tries a BNFC-generated parser first for ordinary shell lines, then
falls back to the legacy tokenizer for quote-heavy or out-of-grammar input.
That keeps the interactive shell usable while the parser front end handles the
common command forms.

## Using the Shell

1. Start with a simple command.

  ```bash
  user@CoreShell> pwd
  user@CoreShell> echo hello world
  ```

2. Run commands through a pipeline.

  ```bash
  user@CoreShell> echo hello from coreshell | tr a-z A-Z
  ```

3. Redirect output to a file and read it back.

  ```bash
  user@CoreShell> echo sample > out.txt
  user@CoreShell> cat < out.txt
  ```

4. Run a job in the background.

  ```bash
  user@CoreShell> sleep 5 &
  ```

5. Use built-in commands directly.

  ```bash
  user@CoreShell> help
  user@CoreShell> cd /tmp
  ```

6. Try the optional natural-language helper.

  ```bash
  user@CoreShell> @list all C files here
  ```

Press `Ctrl+D` to exit the shell.
   ```

2. CoreShell strips the `@` and sends your query to an external helper program `coresh_llm`.

3. The helper returns a concrete shell command (e.g., `find . -name "*.pdf" -size +5M`).

4. CoreShell displays the suggested command and asks for confirmation:
   ```
   Suggested command: find . -name "*.pdf" -size +5M
   Run this? (y/n) 
   ```

5. If you confirm with `y`, the command executes immediately.

### Architecture

- **Shell side** (`main.c`):
  - Detects `@` prefix in input lines.
  - Calls `coresh_llm` via `fork()` + `execvp()`.
  - Communicates via pipes: passes the query to the helper's stdin, reads the suggested command from its stdout.
  - Asks user for confirmation before executing.

- **Helper side** (`coresh_llm`):
  - Receives the natural language query as a command-line argument.
  - Implements LLM logic (e.g., OpenAI API, local model, rule-based mapping).
  - Returns a single line: the suggested shell command.
  - Should handle errors gracefully (empty output, network failures, etc.).

### Requirements

- `coresh_llm` must be available in your `$PATH`.
- The helper program should accept the query as its first argument: `coresh_llm "your query here"`.

The test runner also drives a few end-to-end REPL cases through the real
`./CoreShell` binary. Those cases verify that the BNFC front end accepts the
common shell syntax before the legacy tokenizer handles anything outside that
subset.
- It should output exactly one line to stdout (the suggested command).

### Extending the LLM Helper

The included `coresh_llm.c` is a template with basic rule-based pattern matching. To extend it:

1. **Mock/Rule-based approach** (current):
   - edit `coresh_llm.c` and update the `mock_llm()` function with more patterns.
   - Rebuild: `make coresh_llm` (or just `make`).

2. **OpenAI API approach**:
   - Replace `mock_llm()` with code that calls OpenAI's API (using `curl` or `libcurl`).
   - Example prompt: `"Suggest the simplest shell command that achieves: <query>"`.
   - Handle API key via environment variable: `OPENAI_API_KEY`.

3. **Local LLM approach** (e.g., `ollama`, `llama.cpp`):
   - Call the local model via HTTP or shell invocation.
   - Ensure the helper runs within reasonable time (a few seconds max).

### Example: Testing with the mock helper

```bash
# Build the helper (built by default with 'make')
make

# Run CoreShell
./CoreShell

# In the shell, try natural language queries:
user@CoreShell> @list all C files
Suggested command: find . -type f -name '*.c'
Run this? (y/n) y

user@CoreShell> @find PDF files
Suggested command: find . -type f
Run this? (y/n) n
Cancelled.

user@CoreShell> @check disk usage
Suggested command: df -h
Run this? (y/n) y
```

## Week 10 Submission Quick Reference

For grading-ready verification details, see
[artifacts/week10_assignment_verification.md](artifacts/week10_assignment_verification.md).

Quick verification commands:

```bash
make test
./CoreShell --commands-json | head -40
```

Expected test summary (from `test_output.log`):

```text
Results: 182 passed / 0 failed / 182 total (100%)
```

---

## Built-in Commands

All built-in commands accept `-h` / `--help` to print a usage summary.
Exit code `0` indicates success; non-zero indicates error.

---

### `help [-h] [command]`

Display the list of all built-in commands, or detailed usage for one command.

```
help          # print summary table of every built-in
help ls       # print usage for the ls command
```

---

### `exit [-h]`

Terminate the shell with exit status `0`.

---

### `cd [-h] [dir]`

Change the current working directory.

| Argument | Description |
|---|---|
| `[dir]` | Directory to change to (optional) |

- Omitting `dir` navigates to `$HOME`; if `$HOME` is unset, falls back to `/`.
- Returns `1` on `chdir()` failure.

```
cd /tmp
cd              # go to $HOME
cd --help
```

---

### `pwd [-h] [-L] [-P]`

Print the current working directory.

| Flag | Description |
|---|---|
| `-L` / `--logical` | Use the `$PWD` environment variable (may contain symlinks) |
| `-P` / `--physical` | Resolve all symlinks via `getcwd()` (default) |

- `-P` takes precedence when both flags are given.

```
pwd
pwd -L
pwd -P
```

---

### `echo [-h] [-n] [-e] [string ...]`

Print arguments to standard output.

| Flag | Description |
|---|---|
| `-n` / `--no-newline` | Suppress the trailing newline |
| `-e` / `--escape` | Interpret backslash escapes (`\n \t \r \\ \a \b \v \f`) |

- Up to 64 string arguments accepted; output is space-separated.

```
echo hello world
echo -n "no newline"
echo -e "tab\there"
```

---

### `ls [-h] [-a] [-l] [path]`

List directory contents.

| Flag | Description |
|---|---|
| `-a` / `--all` | Include hidden entries (names starting with `.`) |
| `-l` / `--long` | Long format: mode, links, owner, group, size, mtime, name |
| `[path]` | Directory to list (defaults to `.`) |

- Long format uses `lstat()`; owner/group fall back to numeric UID/GID if the system database lookup fails.
- Returns `1` if `opendir()` fails.

```
ls
ls -la /tmp
ls --help
```

---

### `stat [-h] <file> [<file> ...]`

Display file status information.

| Argument | Description |
|---|---|
| `<file>` | One or more files to stat (required, up to 64) |

- Prints: path, size, octal mode with symbolic string, link count, UID/GID, access/modify/change timestamps.
- Continues to the next file on error; returns `1` if any `stat()` call fails.

```
stat README.md
stat file1.txt file2.txt
```

---

### `cat [-h] [-n] <file> [<file> ...]`

Concatenate and print file contents.

| Flag | Description |
|---|---|
| `-n` / `--number` | Prefix each output line with its line number |
| `<file>` | One or more files to print (required, up to 64) |

- Line numbering is continuous across all files.
- Continues to the next file on `fopen()` failure; returns `1` if any file fails.

```
cat README.md
cat -n file1.txt file2.txt
```

---

### `head [-h] [-n N] <file>`

Print the first N lines of a file.

| Flag | Description |
|---|---|
| `-n N` / `--lines N` | Number of lines to print (default: `10`) |
| `<file>` | Input file (required, exactly 1) |

```
head README.md
head -n 5 README.md
```

---

### `tail [-h] [-n N] <file>`

Print the last N lines of a file.

| Flag | Description |
|---|---|
| `-n N` / `--lines N` | Number of lines from the end to print (default: `10`) |
| `<file>` | Input file (required, exactly 1) |

- Uses a two-pass algorithm: counts lines on the first pass, rewinds and skips on the second.
- If `N` exceeds the total line count, all lines are printed.

```
tail README.md
tail -n 3 log.txt
```

---

### `cp [-h] [-r] [-v] <src> <dst>`

Copy a file or directory.

| Flag | Description |
|---|---|
| `-r` / `--recursive` | Copy directories recursively |
| `-v` / `--verbose` | Print `'src' -> 'dst'` for each item copied |
| `<src> <dst>` | Source and destination paths (both required) |

- Without `-r`, performs a plain binary file copy.
- With `-r`, destination directory is created automatically; `.` and `..` are skipped.

```
cp file.txt backup.txt
cp -rv src_dir/ dst_dir/
```

---

### `mv [-h] [-v] <src> <dst>`

Move or rename a file.

| Flag | Description |
|---|---|
| `-v` / `--verbose` | Print `'src' -> 'dst'` |
| `<src> <dst>` | Source and destination paths (both required) |

- Uses `rename(2)` — works only within the same filesystem.
- Returns `1` on `rename()` failure.

```
mv old.txt new.txt
mv -v file.txt /tmp/file.txt
```

---

### `rm [-h] [-r] [-f] [-v] <file> [<file> ...]`

Remove files or directories.

| Flag | Description |
|---|---|
| `-r` / `--recursive` | Remove directories and their contents recursively |
| `-f` / `--force` | Ignore nonexistent files; suppress `ENOENT` errors |
| `-v` / `--verbose` | Print `removed 'path'` for each item |
| `<file>` | One or more paths to remove (required, up to 64) |

- Without `-r`, uses `unlink(2)` only (cannot remove directories).
- With `-r`, performs a depth-first recursive removal.

```
rm temp.txt
rm -rf build/
rm -fv *.log
```

---

### `mkdir [-h] [-p] [-v] <dir> [<dir> ...]`

Create one or more directories.

| Flag | Description |
|---|---|
| `-p` / `--parents` | Create intermediate parent directories as needed; ignore `EEXIST` |
| `-v` / `--verbose` | Print `created directory 'path'` |
| `<dir>` | One or more directories to create (required, up to 64) |

- Without `-p`, fails if the directory already exists or a parent is missing.

```
mkdir new_dir
mkdir -p a/b/c
mkdir -pv logs/2026/04
```

---

### `rmdir [-h] [-p] <dir> [<dir> ...]`

Remove empty directories.

| Flag | Description |
|---|---|
| `-p` / `--parents` | Also remove each parent directory that becomes empty after the removal |
| `<dir>` | One or more directories to remove (required, up to 64) |

- Uses `rmdir(2)` — fails if the directory is not empty.
- With `-p`, walks up the path stripping the last component; stops silently on `ENOTEMPTY` or `EBUSY`.

```
rmdir empty_dir
rmdir -p a/b/c
```

---

### `touch [-h] [-c] <file> [<file> ...]`

Create files or update timestamps.

| Flag | Description |
|---|---|
| `-c` / `--no-create` | Do not create files that do not exist |
| `<file>` | One or more file paths (required, up to 64) |

- Calls `utime(path, NULL)` to set atime and mtime to the current time.
- If the file does not exist and `-c` is not set, the file is created with mode `0644`.
- Returns `1` if any operation fails.

```
touch newfile.txt
touch -c maybe_exists.txt
touch file1.txt file2.txt
```

---

### `pkg <subcommand> [args...]`

Manage CoreShell packages.

| Subcommand | Description |
|---|---|
| `build <src-dir> <output.tar.gz>` | package a module directory into a distributable archive |
| `install <archive.tar.gz>` | install an archive into `~/.CoreShell` |
| `list` | list installed packages from `~/.CoreShell/pkgdb.txt` |
| `remove <name>` | uninstall a package |
| `check-update <name>` | query registry and compare local vs latest version |
| `upgrade <name>` | download and install latest version when available |
| `compile [--dry-run] [dir]` | compile modules and refresh docs/metadata |

- `pkg` is available both as:
  - a built-in CoreShell command module (`./CoreShell pkg ...`), and
  - a standalone binary (`./pkg/pkg ...`).

---

## Help System

Every built-in command accepts `-h` / `--help` to print its own usage summary.
The `help` command provides the same information from outside the command:

```
help          # list all built-in commands
help cp       # show usage for cp
cp --help     # equivalent: show usage for cp directly
```

For external commands (launched via `fork`/`execvp`), use that program's own
help flag, e.g. `grep --help`.

---

## Multicall Dispatch

CoreSell supports **BusyBox-style multicall** operation. The single binary can be
invoked in three modes:

| Mode | Invocation | Behaviour |
|---|---|---|
| **Symlink / Hardlink** | `./ls -la` (symlink or hardlink to `CoreShell`) | `argv[0]` basename is used as the command name |
| **Explicit** | `./CoreShell ls -la` | `argv[1]` is used as the command name; `argv` is shifted |
| **Interactive** | `./CoreShell` | Falls through to the REPL |

The dispatch pattern in `main()` (from the slides):

```c
const char *cmd = (argc > 1) ? argv[1] : argv0_basename(argv[0]);
int run_argc = (argc > 1) ? argc - 1 : argc;
char **run_argv = (argc > 1) ? &argv[1] : argv;

const cmd_spec_t *spec = find_command(cmd);
if (!spec) return unknown_command(cmd);
return spec->run(run_argc, run_argv);
```

To install all commands as hardlinks in `~/.local/bin/` (recommended — survives
rename/moves of the binary's parent directory is not an issue with hardlinks):

```bash
BINARY="$(pwd)/CoreShell"
BINDIR="$HOME/.local/bin"
for cmd in help exit cd pwd echo ls stat cat head tail cp mv rm mkdir rmdir touch pkg; do
    ln "$BINARY" "$BINDIR/$cmd"
done
```

Alternatively, create symlinks (must be updated if the binary is moved):

```bash
for cmd in help exit cd pwd echo ls stat cat head tail cp mv rm mkdir rmdir touch pkg; do
    ln -sf "$(pwd)/CoreShell" "$HOME/.local/bin/$cmd"
done
```

Ensure `~/.local/bin` is on your `PATH` (add `export PATH="$HOME/.local/bin:$PATH"` to your `~/.bashrc` if needed).

---

## Implementation Details

- **Input**: `fgets()` into a heap buffer; EOF triggers a clean exit
- **Input**: `fgets()` into a heap buffer; EOF triggers a clean exit
- **Parsing**: quote-aware token scanner supporting single/double quotes and backslash escapes
- **Variable expansion**: `$VAR` and `${VAR}` expanded on every input line before routing, including inside double quotes
- **Dispatch**: multicall check → registry lookup → `spec->run(argc, argv)`; unknown → external fork/execvp
- **External commands**: `fork()` + `execvp()` with `waitpid()` in the foreground path
- **Pipelines**: up to 16 stages connected by `pipe()` + `dup2()`; quote-aware `|` splitting so `echo "a | b"` is not split
- **Redirection** (applied left-to-right, POSIX semantics): `<`, `>`, `>>`, `2>`, `2>>`, `2>&1`; compact no-space forms supported
- **Background jobs** (`&`): trailing `&` detaches the pipeline; SIGCHLD blocked around fork→job_add to prevent races; `[Done]` notifications at next REPL prompt
- **Signals**:
  - `SIGINT` (Ctrl-C): sets a flag and re-displays the prompt; does not exit
  - `SIGCHLD`: `sigaction` handler reaps finished children with `waitpid(-1, WNOHANG)`; marks job table entries Done/Killed
- **Job table**: 64-slot `bg_job_t` array in `main.c`, shared via `cmd_jobs.h` API
- **Argument parsing**: vendored [argtable3](argtable3/) library used by every built-in

---

## Project Structure

```
main.c              # REPL loop, signal handling, job table, pipeline executor, registry dispatch
Makefile            # Build configuration (all, debug, clean, test)
README.md           # This file
tests/
  test_runner.c     # Automated C test runner
test_report.md      # Generated test report (created by make test)
argtable3/          # Vendored argument-parsing library
cmd_spec/           # cmd_spec_t typedef (header only)
cmd_registry/       # Command registry (register, find, iterate)
cmd_help/           # help built-in
cmd_exit/           # exit built-in
cmd_cd/             # cd built-in
cmd_pwd/            # pwd built-in
cmd_echo/           # echo built-in
cmd_ls/             # ls built-in
cmd_stat/           # stat built-in
cmd_cat/            # cat built-in
cmd_head/           # head built-in
cmd_tail/           # tail built-in
cmd_cp/             # cp built-in
cmd_mv/             # mv built-in
cmd_rm/             # rm built-in
cmd_mkdir/          # mkdir built-in
cmd_rmdir/          # rmdir built-in
cmd_touch/          # touch built-in
cmd_pkg/            # pkg command module (built-in)
cmd_jobs/           # jobs built-in (list background jobs)
cmd_kill/           # kill built-in (send signal to pid or %jobid)
pkg/                # pkg core + standalone wrapper binary
  pkg.c             # shared pkg implementation + standalone main wrapper
  pkg.h             # pkg_run / pkg_print_usage public interface
```

Each `cmd_*/` directory contains:

```
cmd_<name>/
  cmd_<name>.c    # implementation: run(), print_usage(), spec, register()
  cmd_<name>.h    # public header
  pkg.json        # package metadata
  docs/
    <name>.md     # reference documentation
```

Code pattern per module: `build_*_argtable()` → `*_run()` → `*_print_usage()` → `cmd_*_spec` → `register_*_command()`.

---

## Packaging Metadata

Each command module ships a `pkg.json` file that makes it self-describing:

```json
{
  "name": "ls",
  "version": "1.0.0",
  "description": "list directory contents",
  "long_description": "List information about entries in the specified directory (default: current directory).",
  "files": ["bin/ls"],
  "docs": ["docs/ls.md"]
}
```

The fields map directly to the command's `cmd_spec_t` struct:

| `pkg.json` field | Source |
|---|---|
| `name` | `cmd_spec_t.name` |
| `description` | `cmd_spec_t.summary` |
| `long_description` | `cmd_spec_t.long_help` |
| `files` | list of installed binary paths (e.g. `["bin/ls"]`) |
| `docs` | list of documentation paths (e.g. `["docs/ls.md"]`) |

The `docs/<name>.md` file is generated from the live `--help` output of each command
and contains the usage line, options table, and examples. It is the canonical reference
documentation for each built-in.

---

## Package Manager (`pkg`)

`make` builds a standalone `pkg/pkg` binary alongside `CoreShell`. It manages packages
that follow the `pkg.json` + `.tar.gz` format used by every built-in command module.

### Subcommands

| Subcommand | Description |
|---|---|
| `pkg build <src-dir> <output.tar.gz>` | Package a directory tree into a `.tar.gz` archive (requires `pkg.json` in `<src-dir>`) |
| `pkg install <archive.tar.gz>` | Extract and install a package; symlink executables into `~/.CoreShell/bin/` |
| `pkg list` | List all installed packages from `~/.CoreShell/pkgdb.txt` |
| `pkg remove <name>` | Remove a package, its symlinks, and its install directory |
| `pkg check-update <name>` | Query registry and compare installed vs latest version |
| `pkg upgrade <name>` | Download and install latest version when available |
| `pkg compile [--dry-run] [dir]` | Build modules in `dir` (default: `.`); produces `bin/<name>` + `build/lib<name>.a`; regenerates `docs/<name>.md` and refreshes `pkg.json` |

### Install layout

```
~/.CoreShell/
  pkgs/<name>-<version>/   # extracted package contents
  bin/<name>               # symlink(s) to installed executables
  pkgdb.txt                # one line per installed package: "<name> <version>"
```

### Example workflow

```bash
# Package the echo command module
./pkg/pkg build cmd_echo echo-1.0.0.tar.gz

# Install it
./pkg/pkg install echo-1.0.0.tar.gz

# List installed packages
./pkg/pkg list

# Remove it
./pkg/pkg remove echo
```

At startup, CoreShell automatically prepends `~/.CoreShell/bin` to `$PATH` so that
installed package executables are immediately available without manual shell configuration.

### Compile: Documentation and Metadata Generation

`pkg compile` goes beyond producing a binary — after a successful build it automatically:

1. **Generates `docs/<name>.md`** by running `bin/<name> --help`, splitting the output at the `Options:` boundary, and writing a Markdown file with `## Usage` and `## Options` fenced-code sections.

2. **Refreshes `pkg.json`** by running `bin/<name> --help-json`, extracting the `summary` and `long_help` fields from the command's `cmd_spec_t`, and rewriting `pkg.json` with up-to-date `description` and `long_description` values while preserving `version` and `files[]`.

Use `--dry-run` to preview the planned steps without executing any build or write:

```bash
./pkg/pkg compile --dry-run cmd_echo   # preview only
./pkg/pkg compile cmd_echo             # full build + docs + pkg.json
./pkg/pkg compile .                    # compile all 16 modules under .
```

The `bin/<name> --help-json` output comes from the upgraded wrapper binary that `pkg compile` generates: it calls `register_<name>_command()` at startup so the `cmd_spec_t` is populated, then emits a real JSON object when `--help-json` is requested.

---

## Pipeline, Redirection, and Background Jobs

### Pipelines

```bash
# Two-stage pipeline
/bin/ls | /bin/grep main

# Three-stage pipeline
/bin/cat README.md | /bin/grep -i shell | /bin/head -n 5

# Quoted | is NOT treated as a pipe separator
echo "a | b"
```

### Redirection

```bash
echo hello > /tmp/out.txt        # stdout truncate
echo world >> /tmp/out.txt       # stdout append
/bin/ls /bad 2>/tmp/err.txt      # stderr truncate
/bin/ls /bad 2>>/tmp/err.txt     # stderr append
/bin/ls /bad > /tmp/all.txt 2>&1 # stdout+stderr to file (POSIX left-to-right)
/bin/cat < /tmp/out.txt          # stdin redirection
```

### Background jobs & process management

```bash
/bin/sleep 5 &          # launch in background → [1] <pid>
jobs                    # list active jobs
kill %1                 # send SIGTERM to job 1
kill -s KILL %1         # send SIGKILL to job 1
# When job finishes, next prompt shows:
# [1] Done        /bin/sleep 5
```

### Variable expansion

```bash
echo $HOME              # expands to /home/you
echo ${USER}            # braced form
/bin/echo "$PATH" | /bin/grep -o bin
```

### `jobs` built-in

```
Usage: jobs
```

Prints each background job: `[N] Running|Done|Killed  <pid>  <cmd>`. Done/Killed entries are consumed on display so they won't reappear.

### `kill` built-in

```
Usage: kill [-s SIGNAL] <pid|%jobid> ...
```

| Flag / Arg | Description |
|---|---|
| `-s SIGNAL` | Signal name (`TERM`, `KILL`, `HUP`, etc.) or number |
| `<pid>` | Numeric process ID |
| `%N` | Job number from `jobs` output |

```bash
kill 1234           # SIGTERM to pid 1234
kill %2             # SIGTERM to job 2
kill -s KILL %1     # SIGKILL to job 1
```
