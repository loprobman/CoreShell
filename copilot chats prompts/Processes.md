# CoreShell — Process Management & Shell Features (Week Five)

This document summarises every feature added during the Week-Five session and provides step-by-step examples to exercise each one interactively.

---

## 1. External Command Execution (fork / execvp)

Before this session, all commands were built-in. The shell now forks a child and calls `execvp` for any command not found in the registry.

### How it works

```
parent                   child
───────                  ─────
fork() ──────────────>  execvp("cmd", argv)
waitpid(pid, …)  <───  exit(status)
```

### Example

```bash
./CoreShell

user@CoreShell> /bin/date
Wed May 14 12:34:56 UTC 2026

user@CoreShell> /usr/bin/uptime
 12:34:56 up 3 days,  2:11, load average: 0.12, 0.08, 0.05
```

---

## 2. Pipelines

Up to 16 pipeline stages connected via `pipe()` + `dup2()`. Each stage runs in its own forked child. The parser is **quote-aware** so `|` inside quotes is never treated as a separator.

### Architecture

```
stage[0]           stage[1]           stage[2]
─────────          ─────────          ─────────
fork()      pipe   fork()      pipe   fork()
execvp() ──────>  execvp() ──────>  execvp()
				  waitpid x3 (parent)
```

### Examples

```bash
./CoreShell

# Two-stage pipeline
user@CoreShell> /bin/ls | /bin/grep main
main.c

# Three-stage pipeline
user@CoreShell> /bin/cat README.md | /bin/grep -i shell | /bin/head -n 3
# CoreShell — Simple Linux Shell
...

# Quoted pipe — NOT split on |
user@CoreShell> echo "a | b"
a | b
```

---

## 3. Redirection (`<`, `>`, `>>`, `2>`, `2>>`, `2>&1`)

- `>` write stdout to a file (overwrite)
- `>>` write stdout to a file (append)
- `<` read stdin from a file
- `2>` write stderr to a file
- `2>>` append stderr to a file
- `2>&1` make stderr go to the same place as stdout

Redirections are parsed in left-to-right order (POSIX semantics) and applied with `open()` + `dup2()` in the child. Compact (no-space) forms are supported.

### Examples

```bash
./CoreShell

# Stdout truncate
user@CoreShell> echo hello > /tmp/out.txt
user@CoreShell> /bin/cat /tmp/out.txt
hello

# Stdout append
user@CoreShell> echo world >> /tmp/out.txt
user@CoreShell> /bin/cat /tmp/out.txt
hello
world

# Stdin redirection
user@CoreShell> /bin/cat < /tmp/out.txt
hello
world

# Stderr only
user@CoreShell> /bin/ls /nonexistent 2>/tmp/err.txt
user@CoreShell> /bin/cat /tmp/err.txt
ls: cannot access '/nonexistent': No such file or directory

# Stderr + stdout to same file (POSIX left-to-right: > first, then 2>&1)
user@CoreShell> /bin/ls /nonexistent > /tmp/all.txt 2>&1
user@CoreShell> /bin/cat /tmp/all.txt
ls: cannot access '/nonexistent': No such file or directory

# Opposite order: stderr goes to terminal, stdout goes to file
user@CoreShell> /bin/ls /nonexistent 2>&1 > /tmp/stdout_only.txt
ls: cannot access '/nonexistent': No such file or directory   # printed to terminal

# Compact no-space form
user@CoreShell> echo compact>/tmp/compact.txt
user@CoreShell> /bin/cat /tmp/compact.txt
compact
```

---

## 4. Variable Expansion (`$VAR` / `${VAR}`)

Every input line is expanded before routing. Works in both the simple and pipeline paths, and inside double quotes.

### Examples

```bash
./CoreShell

user@CoreShell> echo $HOME
/home/rmlr

user@CoreShell> echo ${USER}
rmlr

# Expansion inside a pipeline
user@CoreShell> /bin/echo $PATH | /bin/grep -o home
home
home
home

# Expansion inside double quotes
user@CoreShell> echo "I am $USER on $HOSTNAME"
I am rmlr on mymachine
```

---

## 5. Background Jobs (`&`)

A trailing unquoted `&` detaches the pipeline. SIGCHLD is blocked across `fork`→`job_add` to eliminate the race where SIGCHLD fires before the pid is in the job table. When the job exits, the next REPL prompt prints `[N] Done ...`.

### Examples

```bash
./CoreShell

# Launch a background job
user@CoreShell> /bin/sleep 3 &
[1] 8490

# Launch a second one
user@CoreShell> /bin/sleep 5 &
[2] 8491

# List all active jobs
user@CoreShell> jobs
[1] Running    8490    /bin/sleep 3
[2] Running    8491    /bin/sleep 5

# (wait a few seconds — job 1 finishes)
# The NEXT prompt automatically prints:
user@CoreShell> [1] Done        /bin/sleep 3

# Background pipeline
user@CoreShell> /bin/find / -name "*.c" 2>/dev/null | /bin/wc -l &
[3] 8502
```

---

## 6. `jobs` Built-in

Lists all tracked background jobs. Done/Killed entries are consumed on display (won't reappear at the next prompt).

```
Usage: jobs [-h]
```

### Example session

```bash
./CoreShell

user@CoreShell> /bin/sleep 10 &
[1] 8600

user@CoreShell> jobs
[1] Running    8600    /bin/sleep 10

user@CoreShell> jobs
[1] Running    8600    /bin/sleep 10

# After killing it:
user@CoreShell> kill %1
user@CoreShell> jobs
[1] Killed (15)        /bin/sleep 10

user@CoreShell> jobs
No background jobs.
```

---

## 7. `kill` Built-in

Sends a signal to a process by PID or by job number (`%N`). Default is SIGTERM.

```
Usage: kill [-s SIGNAL] <pid|%jobid> ...
```

| Flag | Description |
|---|---|
| `-s SIGNAL` | Signal name (`TERM`, `KILL`, `HUP`, `INT`, `STOP`, `CONT`, `USR1`, `USR2`, …) or number |
| `<pid>` | Numeric process ID |
| `%N` | Job number from `jobs` |

### Examples

```bash
./CoreShell

# Start two background jobs
user@CoreShell> /bin/sleep 30 &
[1] 8700
user@CoreShell> /bin/sleep 30 &
[2] 8701

# Kill job 1 with default SIGTERM
user@CoreShell> kill %1

# Kill job 2 with SIGKILL (cannot be caught or ignored)
user@CoreShell> kill -s KILL %2

# Verify both are gone
user@CoreShell> jobs
[1] Killed (15)        /bin/sleep 30
[2] Killed (9)         /bin/sleep 30

# Kill by raw PID
user@CoreShell> /bin/sleep 60 &
[3] 8720
user@CoreShell> kill 8720

# Kill help
user@CoreShell> kill --help
Usage: kill [-s SIGNAL] <pid|%jobid> ...
...
```

---

## 8. SIGCHLD Signal Handling

The shell installs a `SIGCHLD` handler with `sigaction(SA_RESTART | SA_NOCLDSTOP)`. The handler:

1. Calls `waitpid(-1, &status, WNOHANG)` in a loop to reap all finished children without blocking.
2. Sets each finished child's job table entry to `JOB_DONE` or `JOB_SIGNALED`.
3. Sets a global flag `g_sigchld = 1`.

The REPL checks `g_sigchld` before each prompt and calls `notify_done_jobs()`, which prints and clears all finished entries.

### Race condition prevention

`SIGCHLD` is blocked with `sigprocmask(SIG_BLOCK)` before `fork()` and unblocked after `job_add()`. This ensures that even if a child exits instantly (e.g. `sleep 0 &`), the SIGCHLD handler will find the pid already registered.

### Demo

```bash
./CoreShell

# sleep 0 exits almost instantly
user@CoreShell> /bin/sleep 0 &
[1] 8800

# Issue any command — Done notification appears before next prompt
user@CoreShell> pwd
[1] Done        /bin/sleep 0
/home/rmlr/Development/CoreShell/CoreShell
```

---

## 9. New Files Added This Session

| File | Purpose |
|---|---|
| `cmd_jobs/cmd_jobs.h` | Public API: `bg_job_t`, `job_state_t`, `job_table()`, `job_add()`, `job_by_pid()`, `job_by_id()` |
| `cmd_jobs/cmd_jobs.c` | `jobs` built-in implementation |
| `cmd_kill/cmd_kill.h` | `register_kill_command()` declaration |
| `cmd_kill/cmd_kill.c` | `kill` built-in implementation with signal name table |

### Changes to existing files

| File | Change |
|---|---|
| `main.c` | Added job table globals, `job_add/by_pid/by_id/table/notify_done_jobs`, SIGCHLD handler via `sigaction`, `strip_trailing_ampersand`, `split_pipeline_stages` (quote-aware `\|` split), `execute_pipeline` background path with `sigprocmask` race guard, `expand_variables_line` pre-pass, `parse_word_token` `$VAR` expansion |
| `cmd_registry/cmd_registry.c` | Added `register_jobs_command()` and `register_kill_command()` calls |
| `Makefile` | Added `-Icmd_jobs -Icmd_kill` to `INCLUDES`; added `cmd_jobs/cmd_jobs.c` and `cmd_kill/cmd_kill.c` to `SRC` |
| `tests/test_runner.c` | Added `jobs`/`kill` to `s_commands[]`, added `test_jobs()` suite, wired into `main()` |
| `README.md` | Updated Implementation Details, Project Structure, Test Suites; added Pipeline/Redirection/Jobs documentation section |

---

## Week Eight Baseline (Sockets Client)

Session 8 baseline adds a new built-in command `rpc` that implements a minimal
TCP socket client with line-based protocol, explicit timeout/retry controls,
input validation, and optional `--json` output.

### Baseline checks covered

- Connect, exchange one request/response, and handle errors cleanly.
- Timeouts prevent shell hangs on unreachable endpoints.
- Human-readable output by default; stable JSON keys with `--json`.
- Help and command registry integration match existing command anatomy.
