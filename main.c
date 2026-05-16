#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <libgen.h>
#include "cmd_registry.h"

#define BUFFER_SIZE 1024
#define MAX_ARGS    64

/* ── multicall helpers ─────────────────────────────────────────────────── */

/* Return the basename of path without modifying it.
   Uses a static buffer — suitable only for one call at a time. */
static const char *argv0_basename(const char *path)
{
    /* basename(3) may modify its argument; work on a copy */
    static char buf[256];
    strncpy(buf, path, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    /* basename() takes a file path string and returns only the last component of that path 
      — everything after the final /. */
    return basename(buf); 
}

/* Called in multicall mode when no command matches. */
static int unknown_command(const char *cmd)
{
    fprintf(stderr, "CoreShell: unknown command '%s'\n", cmd);
    fprintf(stderr, "Run 'CoreShell' with no arguments to enter the interactive shell.\n");
    return EXIT_FAILURE;
}

/* ── signal handling ───────────────────────────────────────────────────── */

/* Set by SIGINT handler; checked in the REPL loop */
static volatile sig_atomic_t g_sigint = 0;

/* SIGINT handler: does NOT exit the shell (AGENTS.md requirement).
   Uses only async-signal-safe functions (write). */
static void signal_handler(int sig)
{
    (void)sig;
    g_sigint = 1;
    /* Write a newline so the next prompt starts on a fresh line */
    write(STDOUT_FILENO, "\n", 1);
}

/* ── input ─────────────────────────────────────────────────────────────── */

/* Parse a command line into an argument array.
   Returns the number of arguments found. */
static int parse_command(char *input, char *args[])
{
    int   i     = 0;
    char *token = strtok(input, " \t\n");

    while (token != NULL && i < MAX_ARGS - 1)
    {
        args[i++] = token;
        token = strtok(NULL, " \t\n");
    }
    args[i] = NULL;
    return i;
}

/* ── input ─────────────────────────────────────────────────────────────── */

/* Read one line from stdin.
   Returns a heap-allocated string (caller must free), or NULL on Ctrl-C
   (caller should re-prompt).  Exits on Ctrl-D (EOF) or unrecoverable error. */
static char *read_input(void)
{
    char *input = malloc(BUFFER_SIZE);
    if (input == NULL)
    {
        fprintf(stderr, "malloc: out of memory\n");
        exit(EXIT_FAILURE);
    }

    if (fgets(input, BUFFER_SIZE, stdin) == NULL)
    {
        free(input);
        if (feof(stdin))
        {
            printf("\n");
            exit(EXIT_SUCCESS); /* Ctrl-D: clean exit */
        }
        if (g_sigint)
        {
            clearerr(stdin);
            return NULL; /* Ctrl-C: signal caller to re-prompt */
        }
        exit(EXIT_FAILURE);
    }

    /* Strip trailing newline */
    input[strcspn(input, "\n")] = '\0';
    return input;
}

/* ── command dispatch ──────────────────────────────────────────────────── */

/* Dispatch a built-in command by name.  Returns the command's exit code,
   or 1 if the command is not found. */
static int dispatch_builtin(int argc, char *argv[])
{
    if (argv[0] == NULL)
        return 0;

    const cmd_spec_t *spec = find_command(argv[0]);
    if (spec != NULL)
        return spec->run(argc, argv);

    fprintf(stderr, "CoreShell: '%s': command not found\n", argv[0]);
    return 1;
}

/* ── LLM @ handling ────────────────────────────────────────────────────── */

/*
 * handle_llm_line - Process a natural language query prefixed with @
 *
 * This function:
 * 1. Forks and execs the external helper program 'coresh_llm' with the query.
 * 2. Reads the suggested shell command (one line) from coresh_llm's stdout.
 * 3. Displays the suggested command to the user.
 * 4. Asks for confirmation (Run this? (y/n)).
 * 5. If confirmed, parses and executes the command via dispatch_builtin.
 *
 * Input:  query - natural language string (without the '@' prefix)
 * Output: None (side effects: may execute a command, or print error/abort)
 */
static void handle_llm_line(const char *query)
{
    int pipe_fd[2];
    pid_t pid;

    /* Create a pipe for reading the child's stdout */
    if (pipe(pipe_fd) < 0)
    {
        perror("pipe");
        return;
    }

    /* Fork child process */
    pid = fork();
    if (pid < 0)
    {
        perror("fork");
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return;
    }

    if (pid == 0)
    {
        /* Child process: set up to call coresh_llm */
        close(pipe_fd[0]); /* Close read end in child */

        /* Redirect stdout to the pipe */
        if (dup2(pipe_fd[1], STDOUT_FILENO) < 0)
        {
            perror("dup2");
            _exit(127);
        }
        close(pipe_fd[1]);

        /* Exec coresh_llm with the query as an argument */
        execvp("coresh_llm", (char *const[]){ "coresh_llm", (char *)query, NULL });

        /* If execvp fails, exit with error code */
        perror("coresh_llm");
        _exit(127);
    }

    /* Parent process: read the suggested command */
    close(pipe_fd[1]); /* Close write end in parent */

    char suggested[BUFFER_SIZE];
    ssize_t n = read(pipe_fd[0], suggested, sizeof(suggested) - 1);
    close(pipe_fd[0]);

    /* Wait for the child to complete */
    int status;
    waitpid(pid, &status, 0);

    if (n <= 0)
    {
        fprintf(stderr, "CoreShell: no response from coresh_llm\n");
        return;
    }

    /* Remove trailing newline from suggested command */
    if (n > 0 && suggested[n - 1] == '\n')
        n--;
    suggested[n] = '\0';

    if (n == 0)
    {
        fprintf(stderr, "CoreShell: coresh_llm returned empty response\n");
        return;
    }

    /* Display the suggested command and ask for confirmation */
    printf("Suggested command: %s\n", suggested);
    printf("Run this? (y/n) ");
    fflush(stdout);

    char response[16];
    if (fgets(response, sizeof(response), stdin) == NULL)
    {
        fprintf(stderr, "\nCancelled.\n");
        return;
    }

    /* Check if user confirmed with 'y' or 'yes' */
    if (response[0] != 'y' && response[0] != 'Y')
    {
        printf("Cancelled.\n");
        return;
    }

    /* Parse and execute the suggested command */
    char *suggested_copy = malloc(strlen(suggested) + 1);
    if (suggested_copy == NULL)
    {
        perror("malloc");
        return;
    }
    strcpy(suggested_copy, suggested);

    char *args[MAX_ARGS];
    int nargs = parse_command(suggested_copy, args);

    if (nargs > 0)
        dispatch_builtin(nargs, args);

    free(suggested_copy);
}

/* ── main ──────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    signal(SIGINT, signal_handler);

    /* Populate the command registry */
    register_all_builtin_commands();

    /* Prepend ~/.CoreShell/bin to PATH so installed packages are found */
    {
        const char *home = getenv("HOME");
        const char *path = getenv("PATH");
        if (home && path)
        {
            char newpath[4096];
            snprintf(newpath, sizeof(newpath), "%s/.CoreShell/bin:%s", home, path);
            setenv("PATH", newpath, 1);
        }
    }

    /* ── Multicall dispatch ────────────────────────────────────────────── *
     * Mode 1: invoked via a symlink or hardlink named after a command,    *
     *   e.g. ./ls -la — argv[0] basename is a known command → dispatch    *
     *   directly, no REPL.                                                *
     * Mode 2: invoked as ./CoreShell <cmd> [args...]                      *
     *   argv[1] is a known command → shift argv and dispatch directly.    *
     * Mode 3: ./CoreShell (no args, or argv[0] basename is "CoreShell")   *
     *   → fall through to the interactive REPL (Read-Eval-Print-Loop).    */

    const char *self = argv0_basename(argv[0]);

    /* Mode 1: symlink/hardlink invocation — *self != CoreShell */
    if (strcmp(self, "CoreShell") != 0)
    {
        /* argv[0] is already the command name; pass full argc/argv */
        const cmd_spec_t *spec = find_command(self);
        if (!spec)
            return unknown_command(self);
        return spec->run(argc, argv); /* Run Command */
    }

    /* Mode 2: ./CoreShell <cmd> [args...] */
    if (argc > 1)
    {
        const char *cmd = argv[1];
        /* Only dispatch if argv[1] matches a known built-in;
           otherwise fall through to the REPL so the user can still
           run an interactive session with an initial command typed. */
        const cmd_spec_t *spec = find_command(cmd);
        if (spec)
        {
            /* Shift: run_argv[0] = cmd name, run_argc excludes program name */
            int run_argc = argc - 1;
            char **run_argv = &argv[1];
            return spec->run(run_argc, run_argv);
        }
        /* Unknown command in multicall mode → error, no REPL */
        return unknown_command(cmd);
    }

    /* ── Mode 3: Interactive REPL ──────────────────────────────────────── */
    char  *input;
    char  *args[MAX_ARGS];

    printf("CoreShell v2.0 - Simple Linux Shell\n");
    printf("Type 'help' for available commands or 'exit' to quit.\n\n");

    for (;;)
    {
        g_sigint = 0;

        const char *user = getenv("USER");
        printf("%s@CoreShell> ", user ? user : "user");
        fflush(stdout);

        input = read_input();
        if (input == NULL)
        {
            /* Ctrl-C: re-prompt */
            continue;
        }

        /* Skip blank lines */
        if (input[0] == '\0')
        {
            free(input);
            continue;
        }

        /* Check for LLM @ prefix */
        if (input[0] == '@')
        {
            /* Extract the query (everything after '@') */
            const char *query = input + 1;
            /* Skip leading whitespace after @ */
            while (*query && (*query == ' ' || *query == '\t'))
                query++;

            if (*query != '\0')
            {
                handle_llm_line(query);
            }
            else
            {
                fprintf(stderr, "CoreShell: @ requires a query\n");
            }
            free(input);
            continue;
        }

        int nargs = parse_command(input, args);
        if (nargs > 0)
            dispatch_builtin(nargs, args);
        free(input);
    }

    return 0; /* unreachable */
}
