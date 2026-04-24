#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include "cmd_registry.h"

#define BUFFER_SIZE 1024
#define MAX_ARGS    64

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

static void execute_command(char *args[])
{
    if (args[0] == NULL)
    {
        return;
    }

    /* Count arguments (argv[0] is the command name) */
    int argc = 0;
    while (args[argc] != NULL)
    {
        argc++;
    }

    /* Look up in the built-in registry first */
    const cmd_spec_t *spec = find_command(args[0]);
    if (spec != NULL)
    {
        spec->run(argc, args);
        return;
    }

    /* Fall back to fork+exec for external programs */
    pid_t pid = fork();
    if (pid == 0)
    {
        execvp(args[0], args);
        perror(args[0]);
        exit(EXIT_FAILURE);
    }
    else if (pid > 0)
    {
        waitpid(pid, NULL, 0);
    }
    else
    {
        perror("fork");
    }
}

/* ── main REPL ─────────────────────────────────────────────────────────── */

int main(void)
{
    char  *input;
    char  *args[MAX_ARGS];

    signal(SIGINT, signal_handler);

    /* Populate the command registry */
    register_all_builtin_commands();

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

        parse_command(input, args);
        execute_command(args);
        free(input);
    }

    return 0; /* unreachable */
}
