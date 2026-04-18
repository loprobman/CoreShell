# CoreShell - Simple Linux Shell

A minimal, interactive Unix shell implementation in C (POSIX standard).

## Features

### Built-in Commands
- `help [command]` - Show general or command-specific help
- `exit` - Exit the shell
- `cd [path]` - Change directory
- `pwd` - Print working directory
- `echo [args]` - Print arguments

### External Commands
- Fork and execute external programs using `execvp()`
- Proper process management with `waitpid()`
- External commands (like `ls`, `cat`, etc.) support their own `--help` or `-h` flags

# Help System

- Use `help` to display a list of built-in commands.
- Use `help [command]` for detailed help on a specific built-in command.
- Built-in commands also support `--help` and `-h` flags (e.g., `cd --help`).
- For external commands, use their standard help flags (e.g., `ls --help`).

## Building

```bash
make        # Compile the shell
make clean  # Remove compiled files
```

## Running

```bash
./CoreShell
```

## Implementation Details

- **Input**: Uses `fgets()` for reading command lines
- **Parsing**: Tokenizes commands into arguments
- **Execution**: Implements fork/exec pattern for external commands
- **Signals**: Handles SIGINT to prevent accidental exit
- **Help**: Centralized help for built-in commands via `help` command and flags

## Project Structure

- `main.c` - Core shell logic
- `help.c`, `help.h` - Built-in help system
- `Makefile` - Build configuration
- `README.md` - This file
