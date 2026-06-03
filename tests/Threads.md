# Threading Architecture in CoreShell

## Overview

As of Week Five, CoreShell implements **embedded systems threading requirements** using POSIX threads (pthread) for built-in command execution with system pipes for inter-thread communication.

### Design Goals

1. **Built-in commands** execute in worker threads (not the main REPL thread).
2. **External commands** remain process-based (fork/exec).
3. **System pipes** are used for status communication between threads.
4. **Pipeline engine** remains process-based for shell pipelines (e.g., `ls | grep txt`).

---

## Architecture

### Threading Model

```
REPL => Read Evaluation Print loop

MAIN REPL THREAD               WORKER THREAD
===================            ==============
  |
  | (1) parse command
  | (2) spawn pthread
  +---> pthread_create() ----> builtin_thread_main()
  |                               |
  |                               +-- runs cmd->run(argc, argv)
  |                               |   (e.g., pwd_run, echo_run, etc.)
  |                               |
  |                               +-- writes exit status to pipe
  |                               |   (write(status_fd, &status, ...))
  |                               |
  |                               +-- exits (thread ends)
  |
  | (3) create pipe for status
  | (4) read status from pipe
  | (5) pthread_join() waits
  |
  +--> return status to user
```

### Code Structure

#### New Types and Thread Worker (main.c, lines ~217-230)

```c
typedef struct {
    const cmd_spec_t *spec;
    int argc;
    char **argv;
    int status_fd;         /* Write end of status pipe */
} builtin_thread_ctx_t;

static void *builtin_thread_main(void *arg)
{
    builtin_thread_ctx_t *ctx = (builtin_thread_ctx_t *)arg;
    int status = ctx->spec->run(ctx->argc, ctx->argv);
    (void)write(ctx->status_fd, &status, sizeof(status));
    close(ctx->status_fd);
    return NULL;
}
```

#### Threaded Dispatch (main.c, lines ~236-285)

The `dispatch_builtin()` function now:
1. Creates a status pipe with `pipe(status_pipe)`.
2. Prepares thread context with command spec, argc/argv, and write-end FD.
3. Calls `pthread_create()` to spawn the worker thread.
4. Reads the status from the pipe in the main thread (blocking).
5. Calls `pthread_join()` to wait for thread completion.
6. Closes pipe ends and returns the status.

#### Build Configuration

- Added `-pthread` flag to `CFLAGS` in the Makefile.
- Links pthreads library (implicit with `-pthread`).

---

## Execution Flow by Command Type

### Built-in Command (e.g., `pwd`)

```bash
$ CoreShell pwd
```

1. Main thread parses: argv = ["pwd"]
2. Calls `dispatch_builtin(1, ["pwd"])`
3. Creates status pipe: `[pipe_r, pipe_w]`
4. Spawns worker thread with context containing `cmd_pwd_spec`
5. Worker thread runs `pwd_run(1, ["pwd"])`
   - Outputs PWD to stdout (inherited from parent)
   - Returns status (e.g., 0 on success)
6. Worker writes status (4 bytes) to pipe
7. Main thread reads status from pipe (blocks until available)
8. Main thread joins thread and closes pipe
9. Shell exits with status 0

### External Command (fork-based, unchanged)

```bash
$ CoreShell /bin/ls -la
```

1. Main thread parses: argv = ["/bin/ls", "-la"]
2. Calls `dispatch_command()` → `dispatch_external()`
3. Forks child process with `fork()`
4. Child execs `/bin/ls` with `execvp()`
5. Main thread waits with `waitpid()` for child exit
6. Returns child exit status

**No threading involved in external command path.**

### Pipeline (fork-based, unchanged)

```bash
$ echo hello | grep ell
```

1. Splits pipeline on unquoted `|` → stages: ["echo hello", "grep ell"]
2. For each stage, forks a child process
3. Connects stages via Unix pipes (fd redirection with `dup2()`)
4. Each child either runs built-in (via `exec_pipeline_stage`) or execs external
5. Main REPL waits for all children

**Built-ins in pipelines do not use threads; they run directly in forked children.**

---

## System Pipe Details

### Single-Integer Status Communication

The pipe is used to transport one 32-bit (or 64-bit pointer-sized) integer:

```c
int status = 0;  /* Exit code from the command */
write(status_fd, &status, sizeof(status));
```

The main thread reads it:

```c
ssize_t n = read(status_pipe[0], &status, sizeof(status));
if (n == (ssize_t)sizeof(status))
    return status;
else
    return 1;  /* Read error or incomplete read */
```

### Why a Pipe?

- **Synchronization**: Main thread blocks on `read()` until worker writes status.
- **Simplicity**: No mutex or condition variable needed for a single integer.
- **POSIX-compliant**: System pipes are portable and part of the assignment rubric.

---

## Limitations and Design Decisions

### 1. **Thread-Safe I/O (stdout/stderr)**

Built-in commands write to global `stdout` and `stderr` (shared with main thread).

**Implication**: If multiple built-ins run concurrently (via `-&` or other multi-threading), output may interleave.

**Current design**: Worker threads are serialized by the REPL. Each built-in completes before the next prompt, so interleaving is not an issue in practice.

### 2. **No Built-in-to-Built-in Pipes**

Pipeline stages are not threaded. `pwd | grep usr` still uses `fork()` and process pipes, not thread pipes.

**Rationale**: Thread isolation is minimal; they share memory. Process isolation is safer for pipeline semantics.

### 3. **State-Modifying Built-ins (cd, exit)**

- `cd` calls `chdir()`, which affects the whole process (and all threads).
- `exit()` terminates the process immediately (all threads die).

**Current design**: These still work correctly because they're called in the worker thread, which is the only place where they have an effect visible to the shell.

### 4. **Signal Handlers**

- SIGINT and SIGCHLD handlers remain main-thread-only.
- Threads inherit signal masks; no special handling needed in this design.

---

## Step-by-Step Exercise: Testing Threads

### Exercise 1: Simple Built-in via Thread

**Goal**: Verify that `pwd` executes in a thread and returns the correct status.

```bash
$ ./CoreShell pwd
/home/rmlr/Development/CoreShell/CoreShell
# Print status of last command
$ echo $?
0
```

**What happens**:
1. Main thread spawns worker thread.
2. Worker calls `pwd_run()`.
3. Worker writes status 0 to pipe.
4. Main thread reads 0 and returns it.

### Exercise 2: Built-in Error Handling

**Goal**: Verify thread status propagation for errors.

```bash
$ ./CoreShell cd /nonexistent
cd: No such file or directory
$ echo $?
1
```

**What happens**:
1. Worker thread calls `cd_run()`.
2. `chdir()` fails (returns -1).
3. Worker prints error and writes status 1 to pipe.
4. Main thread reads 1 and returns it.

### Exercise 3: Multiple Built-ins in REPL

**Goal**: Verify serial thread dispatch across multiple commands.

```bash
$ ./CoreShell
CoreShell v2.0 - Simple Linux Shell
Type 'help' for available commands or 'exit' to quit.

rmlr@CoreShell> pwd
/home/rmlr/Development/CoreShell/CoreShell
rmlr@CoreShell> echo hello
hello
rmlr@CoreShell> pwd
/home/rmlr/Development/CoreShell/CoreShell
rmlr@CoreShell> exit
```

**What happens**:
1. First `pwd` → thread executes → write/read pipe → returns to prompt.
2. `echo hello` → new thread spawned → writes/reads pipe → returns to prompt.
3. Second `pwd` → another thread → same process, different command.
4. `exit()` in worker → process terminates (no more prompts).

### Exercise 4: External Commands (No Threading)

**Goal**: Verify external commands still use fork/exec (not threads).

```bash
$ ./CoreShell /bin/echo external-test
external-test
```

**What happens**:
1. Main thread detects `/bin/echo` not in registry.
2. Calls `dispatch_external()` (not `dispatch_builtin()`).
3. Forks child, child execs `/bin/echo`.
4. Main thread waits for child exit.
5. Returns child exit status.

**Note**: No thread is created; full process isolation applies.

### Exercise 5: Pipeline (Mixed Built-in + External, Fork-based)

**Goal**: Verify pipelines don't use threads, even with built-ins.

```bash
$ ./CoreShell
...
rmlr@CoreShell> echo hello | cat
hello
```

**What happens**:
1. Parser detects `|` → `execute_pipeline()` called (not `dispatch_command()`).
2. Splits into 2 stages: `["echo hello", "cat"]`.
3. For stage 1 (echo):
   - Forks child_1.
   - Child_1 calls `exec_pipeline_stage()` (direct call, no thread).
   - `echo_run()` executes directly in child_1 memory space.
   - Writes to pipe (stdout redirected via `dup2()`).
4. For stage 2 (cat):
   - Forks child_2.
   - Child_2 inherits pipe read end from child_1.
   - `cat_run()` executes in child_2.
   - Reads from stdin (pipe) and writes to parent's stdout.
5. Main thread waits for both children.

**Note**: Even though `echo` and `cat` are built-ins, they run in forked children (not threads) in the pipeline context. This preserves Unix pipe semantics.

### Exercise 6: Testing Thread Safety

**Goal**: Verify threads don't interfere with shell state (e.g., PWD, exit status).

```bash
$ ./CoreShell
...
rmlr@CoreShell> cd /tmp
rmlr@CoreShell> pwd
/tmp
rmlr@CoreShell> cd /
rmlr@CoreShell> pwd
/
```

**What happens**:
1. Each `cd` runs in a worker thread.
2. `chdir()` is process-global; all threads see the new PWD.
3. Each subsequent `pwd` in a new thread reads the current PWD and prints it.
4. Working directory state is correctly maintained across thread boundaries.

---

## Verification: make test

The test runner validates threaded dispatch and status propagation:

```bash
$ make test
Running CoreShell test suite...
...
test_pwd: PASS
test_echo: PASS (5 cases)
test_cd: PASS (4 cases)
...
Test report written to: test_report.md
```

Each test forks a child process to call the built-in command handler, which internally spawns a worker thread. The test suite captures the output and validates exit codes—verifying thread status propagation end-to-end.

---

## Implementation Details

### File: main.c

- Lines 11: `#include <pthread.h>`
- Lines 217-230: `builtin_thread_ctx_t` and `builtin_thread_main()`.
- Lines 236-285: Updated `dispatch_builtin()` with thread spawning and pipe I/O.
- Line 1291: Multicall mode also uses threaded dispatch (`dispatch_builtin()`).

### File: Makefile

- Line 2: `CFLAGS` includes `-pthread`.

### Compilation

```bash
$ make
gcc ... -pthread ... -o CoreShell ...
```

The `-pthread` flag:
- Defines `_REENTRANT` macro (for thread-safe libc).
- Links against `libpthread`.
- Ensures destructor ordering and cleanup.

---

## Summary: Three Different Execution Models

| Model | Mechanism | When | Data Flow |
|-------|-----------|------|-----------|
| **Built-in (threaded)** | `pthread_create()` + pipe | Simple commands in REPL | Status via pipe (1 int) |
| **External (fork/exec)** | `fork()` + `execvp()` | External binaries | Inherited stdout/stderr |
| **Pipeline (fork+pipe)** | `fork()` + `pipe()` + `dup2()` | Multi-stage commands | Unix pipes (streaming data) |

Built-ins in pipelines use the **pipeline (fork+pipe)** model, not the **built-in (threaded)** model. This preserves shell correctness and pipe semantics.
