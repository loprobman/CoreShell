# CoreShell — Simple Linux Shell

A minimal, interactive Unix shell implemented in C (POSIX standard).

## Building

```bash
make        # Compile the shell
make debug  # Compile with debug symbols (-g -O0) for use with gdb
make clean  # Remove all compiled objects, binaries, and test report
```

## Running

```bash
./CoreShell
```

## Testing

```bash
make test                   # Build test runner and execute all test cases
./test_runner               # Run already-built test binary directly
make test 2>&1 | tee test_output.log   # Run and save terminal output to a log file
```

`make test` compiles a standalone C test runner (`test_runner`) that links directly
against the built-in command objects, forks an isolated child per test case, and
captures stdout/stderr via pipes. On completion it prints a colour-coded terminal
summary and writes `test_report.md` to the project root.

## Debugging

```bash
make debug
gdb ./CoreShell
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

## External Commands

Any command not matched by the built-in registry is executed via `fork()` +
`execvp()`, with the parent waiting via `waitpid()`. Standard Unix utilities
(`grep`, `wc`, `sort`, etc.) work normally.

---

## Implementation Details

- **Input**: `fgets()` into a heap buffer; EOF triggers a clean exit
- **Parsing**: `strtok()` splits on space, tab, and newline
- **Dispatch**: registry lookup → `spec->run(argc, argv)`; unknown → `fork` + `execvp`
- **Signals**: `SIGINT` (Ctrl-C) sets a flag and re-displays the prompt; it does not exit
- **Argument parsing**: vendored [argtable3](argtable3/) library used by every built-in

---

## Project Structure

```
main.c              # REPL loop, signal handling, registry dispatch
Makefile            # Build configuration (all, debug, clean, test)
README.md           # This file
tests/
  test_runner.c     # Automated C test runner (57 test cases)
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
```

Each `cmd_*/` module follows the same pattern:
`build_*_argtable()` → `*_run()` → `*_print_usage()` → `cmd_*_spec` → `register_*_command()`.
