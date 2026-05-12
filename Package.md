You are my C systems programming assistant.

Context:
- I am writing/updating a small Unix shell in C called CoreShell.
- I want a separate C program called `pkg` that manages packages for my shell.
- Packages are `.tar.gz` archives containing a `pkg.json` file plus files to install.
- Packages install under `~/.CoreShell/pkgs/<name>-<version>/` and executables go to `~/.CoreShell/bin/`.

Goal:
- Implement `pkg` with subcommands: `build`, `install`, `list`, `remove`.
- Use only standard C and POSIX APIs, plus external programs like `tar` (invoked with `fork/exec`).


Step 1:
- Propose a clean C structure for `pkg.c` (functions, data structures).
- Then generate the initial C code that:
  - Parses `argv[1]` as a subcommand.
  - Implements `pkg build <src-dir> <output-tar>` by calling `tar` using `fork` + `execlp`.
- Include clear `gcc` compile instructions.