# CoreShell Package Management

`pkg` is a standalone binary (`pkg/pkg`) built alongside the shell. It manages command
modules that follow the CoreShell app anatomy: a directory containing `pkg.json`, one or
more `.c` source files, and a `docs/` subdirectory.

---

## Design overview

```
CoreShell workspace
├── cmd_echo/              ← one command module
│   ├── cmd_echo.c         ← implementation
│   ├── cmd_echo.h
│   ├── pkg.json           ← module metadata
│   └── docs/
│       └── echo.md        ← generated documentation
├── bin/                   ← standalone binaries produced by pkg compile
├── build/                 ← static libraries produced by pkg compile
└── pkg/
    └── pkg.c              ← package manager source
```

### `pkg.json` format

Every module must have a `pkg.json` at its root:

```json
{
  "name": "echo",
  "version": "1.0.0",
  "description": "print arguments to standard output",
  "long_description": "Echo the given string arguments to stdout, separated by spaces.",
  "files": ["bin/echo"],
  "docs": ["docs/echo.md"]
}
```

`pkg compile` keeps `name`, `version`, and `files[]` from the existing file and
overwrites `description` / `long_description` with live data read from the binary's
`--help-json` response.

### Install layout

Packages installed with `pkg install` land under `~/.CoreShell/`:

```
~/.CoreShell/
  pkgs/<name>-<version>/   # extracted module contents
  bin/<name>               # symlink to the installed executable
  pkgdb.txt                # registry: one "<name> <version>" line per package
```

At startup CoreShell prepends `~/.CoreShell/bin` to `$PATH`, so installed commands
are available immediately in every new shell session without any extra configuration.

### `pkg compile` internals

`pkg compile` automates the full build → document → metadata pipeline:

1. **Strategy A** — if a module directory contains a `Makefile`, delegate to `make -C <dir>`.
2. **Strategy B** — no `Makefile`: `pkg` compiles every `.c` file to `build/<name>_<stem>.o`,
   archives them into `build/lib<name>.a`, generates a minimal `main()` wrapper that wires
   up the module's `cmd_spec_t`, and links `bin/<name>`.
3. **Doc generation** — runs `bin/<name> --help`, splits at the `Options:` boundary, and
   writes `docs/<name>.md` with `## Usage` and `## Options` fenced-code sections.
4. **`pkg.json` refresh** — runs `bin/<name> --help-json` (which returns real `cmd_spec_t`
   fields), parses `summary` and `long_help`, and rewrites `pkg.json` in-place.

The generated wrapper is the reason `--help-json` returns real data rather than stubs: it
calls `register_<name>_command()` at startup so the `cmd_spec_t` is populated before the
JSON response is emitted.

---

## Commands

### `pkg build`

Packages a module directory into a `.tar.gz` archive ready for distribution.

```
pkg build <src-dir> <output.tar.gz>
```

- `<src-dir>` must contain a `pkg.json`.
- The archive is created with `tar -czf` and contains the directory contents, not the
  directory itself (equivalent to `tar -czf output.tar.gz -C <src-dir> .`).

**Example — package the `echo` module:**

```bash
./pkg/pkg build cmd_echo echo-1.0.0.tar.gz
```

Expected output:

```
Building: echo-1.0.0.tar.gz from cmd_echo/
Created: echo-1.0.0.tar.gz
```

---

### `pkg install`

Installs a previously built archive into `~/.CoreShell/`.

```
pkg install <archive.tar.gz>
```

Steps performed:
1. Extracts the archive to a temp directory to read `pkg.json`.
2. Creates `~/.CoreShell/pkgs/<name>-<version>/` and extracts the archive there.
3. For each entry in `files[]`, creates a symlink in `~/.CoreShell/bin/`.
4. Appends a `<name> <version>` line to `~/.CoreShell/pkgdb.txt`.

**Example — install the echo archive:**

```bash
./pkg/pkg install echo-1.0.0.tar.gz
```

Expected output:

```
Installing echo 1.0.0...
Installed echo 1.0.0 → ~/.CoreShell/pkgs/echo-1.0.0
```

---

### `pkg list`

Lists every package currently recorded in `~/.CoreShell/pkgdb.txt`.

```
pkg list
```

No arguments. Prints a table of installed packages and their versions, plus a total count.

**Example:**

```bash
./pkg/pkg list
```

Expected output (after installing `echo`):

```
Installed packages:
  echo  1.0.0
1 packages installed.
```

---

### `pkg remove`

Uninstalls a package by name.

```
pkg remove <name>
```

Steps performed:
1. Looks up the version in `pkgdb.txt`.
2. Removes each symlink under `~/.CoreShell/bin/` that points into the package directory.
3. Deletes `~/.CoreShell/pkgs/<name>-<version>/` recursively.
4. Removes the entry from `pkgdb.txt`.

**Example — remove the echo package:**

```bash
./pkg/pkg remove echo
```

Expected output:

```
Removed echo 1.0.0
```

---

### `pkg compile`

Builds one or all modules, then generates documentation and refreshes `pkg.json`.

```
pkg compile [--dry-run] [dir]
```

- `dir` defaults to `.` (the current directory).
- If `dir` itself contains a `pkg.json` it is treated as a **single-module** target.
- Otherwise `pkg compile` walks immediate subdirectories and compiles every one that
  contains a `pkg.json` (**multi-module** mode).
- `--dry-run` prints every planned step without executing any build, write, or link.

**Example — compile the `echo` module with full output:**

```bash
./pkg/pkg compile cmd_echo
```

Expected output:

```
pkg compile: single module 'cmd_echo'

  [echo 1.0.0]  cmd_echo/
  strategy : gcc
  source   : cmd_echo/cmd_echo.c
  compile  : cmd_echo/cmd_echo.c → build/echo_cmd_echo.o
  archive  : build/libecho.a
  wrapper  : /tmp/pkgcompile_echo_main.c (calls echo_run)
  binary   : bin/echo
  docs     : cmd_echo/docs/echo.md
  docs     : written
  pkg.json : cmd_echo/pkg.json (refreshed)
  pkg.json : written
  [OK]

pkg compile: done
```

After this run:

- `bin/echo` is a standalone executable that behaves identically to the built-in.
- `build/libecho.a` is a static library suitable for linking into the shell.
- `cmd_echo/docs/echo.md` contains up-to-date `## Usage` and `## Options` sections.
- `cmd_echo/pkg.json` has `description` and `long_description` sourced from the live binary.

**Dry-run preview:**

```bash
./pkg/pkg compile --dry-run cmd_echo
```

**Compile all modules at once:**

```bash
./pkg/pkg compile .
```

---

## Typical development workflow

```bash
# 1. Build the shell and pkg binary
make

# 2. Compile a module, generate its docs, and refresh its metadata
./pkg/pkg compile cmd_echo

# 3. Package it for distribution
./pkg/pkg build cmd_echo echo-1.0.0.tar.gz

# 4. Install it
./pkg/pkg install echo-1.0.0.tar.gz

# 5. Verify installation
./pkg/pkg list

# 6. Uninstall when no longer needed
./pkg/pkg remove echo
```

Once installed, `echo` is available as a standalone command in any CoreShell session
because `~/.CoreShell/bin` is prepended to `$PATH` at startup.

---

## Packaging `echo` with `make`

`make` builds both `CoreShell` and `pkg/pkg` in a single step, replacing the raw
`gcc` call needed in the without-`make` path. Steps 2–5 are identical in both workflows.

> **Note:** if you have just run `make clean`, `pkg/pkg` no longer exists — always run
> `make` before any `./pkg/pkg` command.

**Step 1 — Build `CoreShell` and `pkg/pkg`:**

```bash
make
```

**Step 2 — Compile the `echo` module** (build + docs + `pkg.json` refresh):

```bash
./pkg/pkg compile cmd_echo
```

**Step 3 — Package into a distributable archive:**

```bash
./pkg/pkg build cmd_echo echo-1.0.0.tar.gz
```

**Step 4 — Install:**

```bash
./pkg/pkg install echo-1.0.0.tar.gz
```

**Step 5 — Verify:**

```bash
./pkg/pkg list
```

**Step 6 — Run the installed `echo`:**

Inside CoreShell, `~/.CoreShell/bin` is already in `$PATH`:

```bash
./CoreShell
echo Hello World
```

From your regular shell, use the full path or export `PATH` first:

```bash
~/.CoreShell/bin/echo Hello World

# or, to use it like any other command:
export PATH="$HOME/.CoreShell/bin:$PATH"
echo Hello World
```

---

## Packaging `echo` without `make`

The only step that requires a raw compiler call is building the `pkg` binary itself.
Everything after that is handled by `pkg`, which drives `gcc` internally via Strategy B.

**Step 1 — Build the `pkg` binary:**

```bash
gcc -Wall -Wextra -g -std=c99 -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 \
    -o pkg/pkg pkg/pkg.c
```

**Step 2 — Compile the `echo` module** (Strategy B: no `Makefile` needed):

```bash
./pkg/pkg compile cmd_echo
```

This single command runs `gcc` internally to produce `build/libecho.a` and `bin/echo`,
then generates `cmd_echo/docs/echo.md` and refreshes `cmd_echo/pkg.json`.

**Step 3 — Package into a distributable archive:**

```bash
./pkg/pkg build cmd_echo echo-1.0.0.tar.gz
```

**Step 4 — Install:**

```bash
./pkg/pkg install echo-1.0.0.tar.gz
```

**Step 5 — Verify:**

```bash
./pkg/pkg list
```

**Step 6 — Run the installed `echo`:**

Inside CoreShell, `~/.CoreShell/bin` is already in `$PATH`:

```bash
./CoreShell
echo Hello World
```

From your regular shell, use the full path or export `PATH` first:

```bash
~/.CoreShell/bin/echo Hello World

# or, to use it like any other command:
export PATH="$HOME/.CoreShell/bin:$PATH"
echo Hello World
```

Only step 1 is a raw `gcc` invocation. Steps 2–6 use `pkg` or the installed binary exclusively.
