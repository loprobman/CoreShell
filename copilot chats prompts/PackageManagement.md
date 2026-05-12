# CoreShell Package Management

## Overview

`pkg` is available in two compatible forms:

- **Built-in command module** — invoke as `pkg ...` from within CoreShell
- **Standalone binary** — `./pkg/pkg ...` for direct command-line use

Both forms share the same implementation (`pkg_run` / `pkg_print_usage`) and manage command modules following the CoreShell app anatomy: a directory with `pkg.json`, source code, and documentation.

---

## Architecture

### Dual-mode design

```
CoreShell workspace
├── cmd_pkg/                ← pkg command module
│   ├── cmd_pkg.c          ← wrapper for built-in dispatch
│   ├── cmd_pkg.h          ← module interface
│   ├── pkg.json           ← module metadata
│   └── docs/
│       └── pkg.md         ← auto-generated documentation
├── pkg/
│   ├── pkg.c              ← shared implementation
│   ├── pkg.h              ← exported API (pkg_run, pkg_print_usage)
│   └── pkg.h              ← also compiled as standalone ./pkg/pkg
├── cmd_echo/              ← example command module
│   ├── cmd_echo.c         
│   ├── cmd_echo.h
│   ├── pkg.json           
│   └── docs/
│       └── echo.md        
├── bin/                   ← standalone binaries produced by pkg compile
└── build/                 ← static libraries produced by pkg compile
```

### How it works

**Built-in mode** (inside CoreShell):
1. `main.c` registers all built-in commands via `register_all_builtin_commands()`
2. `cmd_registry/cmd_registry.c` includes `cmd_pkg.h` and calls `register_pkg_command()`
3. `cmd_pkg/cmd_pkg.c` wraps `pkg_run()` and `pkg_print_usage()` from the shared `pkg/pkg.c`
4. `Makefile` compiles both `cmd_pkg/cmd_pkg.c` and `pkg/pkg.c` (with `-DPKG_NO_MAIN` flag for embedded use) into CoreShell

**Standalone mode** (direct binary):
1. `Makefile` compiles `pkg/pkg.c` separately (without `-DPKG_NO_MAIN`) 
2. A `main()` function in `pkg/pkg.c` is included (guarded by `#ifndef PKG_NO_MAIN`)
3. `./pkg/pkg` can be run independently

Both paths execute identical logic, ensuring consistency.

### Module metadata format (`pkg.json`)

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

`pkg compile` keeps `name`, `version`, and `files[]` from the existing file and overwrites `description` / `long_description` with live data from the binary's `--help-json` response.

### Install layout

Packages installed with `pkg install` land under `~/.CoreShell/`:

```
~/.CoreShell/
  pkgs/<name>-<version>/   # extracted module contents
  bin/<name>               # symlink to the installed executable
  pkgdb.txt                # registry: one "<name> <version>" line per package
```

At startup CoreShell prepends `~/.CoreShell/bin` to `$PATH`, so installed commands are available in every session without extra configuration.

### Build pipeline: `pkg compile` internals

`pkg compile` automates the full build → document → metadata pipeline:

1. **Strategy A** — if a module directory contains a `Makefile`, delegate to `make -C <dir>`.
2. **Strategy B** — no `Makefile`: `pkg` compiles every `.c` file to `build/<name>_<stem>.o`, archives them into `build/lib<name>.a`, generates a minimal `main()` wrapper, and links `bin/<name>`.
3. **Doc generation** — runs `bin/<name> --help`, splits at the `Options:` boundary, and writes `docs/<name>.md` with `## Usage` and `## Options` sections.
4. **`pkg.json` refresh** — runs `bin/<name> --help-json` (which returns real `cmd_spec_t` fields), parses `summary` and `long_help`, and rewrites `pkg.json` in-place.

---

## Commands

### `pkg build`

Packages a module directory into a `.tar.gz` archive ready for distribution.

```
pkg build <src-dir> <output.tar.gz>
```

- `<src-dir>` must contain a `pkg.json`.
- The archive is created with `tar -czf` and contains directory contents (not the directory itself).
- In this project, generated archives are kept under `artifacts/`.

**Example:**

```bash
./pkg/pkg build cmd_echo artifacts/echo-1.0.0.tar.gz
```

Expected output:

```
Building: artifacts/echo-1.0.0.tar.gz from cmd_echo/
Created: artifacts/echo-1.0.0.tar.gz
```

---

### `pkg install`

Installs a previously built archive into `~/.CoreShell/`.

```
pkg install <archive.tar.gz>
```

**Steps performed:**
1. Extracts the archive to a temp directory to read `pkg.json`
2. Creates `~/.CoreShell/pkgs/<name>-<version>/` and extracts the archive there
3. For each entry in `files[]`, creates a symlink in `~/.CoreShell/bin/`
4. Appends a `<name> <version>` line to `~/.CoreShell/pkgdb.txt`

**Example:**

```bash
./pkg/pkg install echo-1.0.0.tar.gz
```

Expected output:

```
Installing echo 1.0.0...
Installed echo 1.0.0 → ~/.CoreShell/pkgs/echo-1.0.0
```
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

**Steps performed:**
1. Looks up the version in `pkgdb.txt`
2. Removes each symlink under `~/.CoreShell/bin/` that points into the package directory
3. Deletes `~/.CoreShell/pkgs/<name>-<version>/` recursively
4. Removes the entry from `pkgdb.txt`

**Example:**

```bash
./pkg/pkg remove echo
```

Expected output:

```
Removed echo 1.0.0
```

---

### `pkg check-update` *(New)*

Checks if a newer version of a package is available in a remote registry.

```
pkg check-update <name>
```

**How it works:**
1. Looks up the currently installed version in `pkgdb.txt`
2. Queries a registry endpoint (environment variable `REGISTRY_URL`, defaults to a fallback) to get the latest available version
3. Compares versions using semantic versioning (dotted numeric comparison)
4. Reports whether an update is available

**Example:**

```bash
./pkg/pkg check-update echo
```

Expected output (if 1.1.0 is available):

```
echo: current version 1.0.0, latest available 1.1.0
Update available!
```

Or if already up-to-date:

```
echo: version 1.0.0 is up-to-date
```

**Environment variable:**

```bash
export REGISTRY_URL="http://registry.example.com"
./pkg/pkg check-update echo
```

---

### `pkg upgrade` *(New)*

Downloads and installs the latest available version of a package, removing the old version.

```
pkg upgrade <name>
```

**How it works:**
1. Calls `pkg check-update` internally to verify a newer version exists
2. Queries the registry for the download URL of the latest version
3. Downloads the package to `/tmp/` using `curl`
4. Calls `pkg install` with the downloaded archive
5. Removes the old version via `pkg remove`
6. Cleans up temporary files

**Example:**

```bash
./pkg/pkg upgrade echo
```

Expected output:

```
echo: checking for updates...
echo: current version 1.0.0, latest available 1.1.0
echo: downloading https://registry.example.com/echo-1.1.0.tar.gz...
echo: installing from /tmp/echo-1.1.0.tar.gz...
Installing echo 1.1.0...
Installed echo 1.1.0 → ~/.CoreShell/pkgs/echo-1.1.0
echo: removing old version 1.0.0...
Removed echo 1.0.0
Upgrade complete: echo 1.0.0 → 1.1.0
```

---

### `pkg compile`

Builds one or all modules, then generates documentation and refreshes `pkg.json`.

```
pkg compile [--dry-run] [dir]
```

- `dir` defaults to `.` (the current directory)
- If `dir` itself contains a `pkg.json`, it is treated as a **single-module** target
- Otherwise `pkg compile` walks immediate subdirectories and compiles every one that contains a `pkg.json` (**multi-module** mode)
- `--dry-run` prints every planned step without executing any build, write, or link

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
- `bin/echo` is a standalone executable that behaves identically to the built-in
- `build/libecho.a` is a static library suitable for linking into the shell
- `cmd_echo/docs/echo.md` contains up-to-date `## Usage` and `## Options` sections
- `cmd_echo/pkg.json` has `description` and `long_description` sourced from the live binary

**Dry-run preview:**

```bash
./pkg/pkg compile --dry-run cmd_echo
```

**Compile all modules at once:**

```bash
./pkg/pkg compile .
```

---

## Testing the Package Management System

### Prerequisites

1. **Build CoreShell:**
   ```bash
   make
   ```
   This produces:
   - `./CoreShell` (the shell with pkg as a built-in command)
   - `./pkg/pkg` (the standalone pkg binary)

2. **Set up a mock registry** (optional, for testing check-update/upgrade):
   ```bash
   npm start  # Starts the Node.js registry on port 3000
   ```

### Test 1: Basic `pkg` workflow

#### Verify the binary exists and shows help:

```bash
./pkg/pkg --help
```

Expected output includes:
```
Usage: pkg <subcommand> [args...]

Subcommands:
  build <src-dir> <output.tar.gz>   package a directory into a .tar.gz
  install <archive.tar.gz>           install a package
  list                               list installed packages
  remove <name>                      remove an installed package
  check-update <name>                check for a newer package version
  upgrade <name>                     download and install latest version
  compile [--dry-run] [dir]          build modules in dir (default: .)
```

#### Verify the built-in command works too:

```bash
./CoreShell
user@CoreShell> pkg --help
```

Should produce the same help output.

---

### Test 2: Compile and package a module

#### Step-by-step:

```bash
# 1. Compile the echo module
./pkg/pkg compile cmd_echo

# Verify bin/echo was created and works:
./bin/echo "Hello, World!"
# Output: Hello, World!

# 2. Package it for distribution
./pkg/pkg build cmd_echo echo-test-1.0.0.tar.gz

# 3. Verify the archive was created:
ls -lh echo-test-1.0.0.tar.gz
tar -tzf echo-test-1.0.0.tar.gz | head -10
```

---

### Test 3: Install, list, and remove

#### Step-by-step:

```bash
# 1. Install the package:
./pkg/pkg install echo-test-1.0.0.tar.gz

# Expected output:
# Installing echo 1.0.0...
# Installed echo 1.0.0 → ~/.CoreShell/pkgs/echo-1.0.0

# 2. Verify installation by listing packages:
./pkg/pkg list

# Expected output:
# Installed packages:
#   echo  1.0.0
# 1 packages installed.

# 3. Verify the installed binary is in PATH:
~/.CoreShell/bin/echo "Test from installed package"
# Output: Test from installed package

# 4. Remove the package:
./pkg/pkg remove echo

# Expected output:
# Removed echo 1.0.0

# 5. Verify it's gone:
./pkg/pkg list
# Expected output:
# Installed packages:
# 0 packages installed.
```

---

### Test 4: Check-update and upgrade flow

#### Prerequisites for this test:

(Kill the old server)
pkill -f "node server.js"

1. **Start the mock registry:**
   ```bash
   npm start &
   export REGISTRY_URL="http://127.0.0.1:3000"
   ```
   or 
   npm start > /dev/null 2>&1 &
   export REGISTRY_URL="http://127.0.0.1:3000"
   verify is running
   curl http://127.0.0.1:3000/packages/echo

   ```bash
   # Version 1.0.0 (already created above)
   ./pkg/pkg build cmd_echo echo-1.0.0.tar.gz
   
   # Create version 1.1.0 (modify pkg.json, recompile, rebuild)
   # For demo purposes, just create another copy:
   cp -r cmd_echo cmd_echo_v2
   sed -i 's/"1.0.0"/"1.1.0"/g' cmd_echo_v2/pkg.json
   ./pkg/pkg compile cmd_echo_v2
   ./pkg/pkg build cmd_echo_v2 echo-1.1.0.tar.gz
   ```

#### Step-by-step upgrade test:

```bash
# 1. Install version 1.0.0:
./pkg/pkg install echo-1.0.0.tar.gz

# 2. Verify version 1.0.0 is installed:
./pkg/pkg list
# Output shows: echo  1.0.0

# 3. Check if an update is available:
./pkg/pkg check-update echo

# Expected output (if registry has 1.1.0):
# echo: current version 1.0.0, latest available 1.1.0
# Update available!

# 4. Perform the upgrade:
./pkg/pkg upgrade echo

# Expected output:
# echo: checking for updates...
# echo: current version 1.0.0, latest available 1.1.0
# echo: downloading package...
# Installing echo 1.1.0...
# Installed echo 1.1.0 → ~/.CoreShell/pkgs/echo-1.1.0
# echo: removing old version 1.0.0...
# Removed echo 1.0.0
# Upgrade complete: echo 1.0.0 → 1.1.0

# 5. Verify the new version is installed:
./pkg/pkg list
# Output shows: echo  1.1.0

# 6. Check for updates again (should show up-to-date):
./pkg/pkg check-update echo
# Output: echo: version 1.1.0 is up-to-date
```

---

### Test 5: Using pkg from within the shell (built-in mode)

#### Step-by-step:

```bash
# 1. Start CoreShell:
./CoreShell

# 2. Compile a module using the built-in command:
user@CoreShell> pkg compile cmd_echo
# Full output as shown in Test 2

# 3. Build and install:
user@CoreShell> pkg build cmd_echo echo-shell-1.0.0.tar.gz
user@CoreShell> pkg install echo-shell-1.0.0.tar.gz

# 4. List installed packages:
user@CoreShell> pkg list

# 5. Test the newly installed package:
user@CoreShell> ~/.CoreShell/bin/echo "Test from shell"

# 6. Check for updates:
user@CoreShell> pkg check-update echo

# 7. Upgrade if available:
user@CoreShell> pkg upgrade echo

# 8. Clean up:
user@CoreShell> pkg remove echo
user@CoreShell> exit
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

# 6. Check for updates
./pkg/pkg check-update echo

# 7. Uninstall when no longer needed
./pkg/pkg remove echo
```

Once installed, `echo` is available as a standalone command in any CoreShell session because `~/.CoreShell/bin` is prepended to `$PATH` at startup.

---

## Using pkg inside CoreShell

Inside `CoreShell`, you can run the same workflow commands directly as built-ins. This allows for interactive package management without exiting the shell:

```bash
pkg compile cmd_echo
pkg build cmd_echo echo-1.0.0.tar.gz
pkg install echo-1.0.0.tar.gz
pkg list
pkg check-update echo
pkg upgrade echo
pkg remove echo
```

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

