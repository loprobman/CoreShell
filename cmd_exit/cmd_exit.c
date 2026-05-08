#include <stdio.h>
#include <stdlib.h>
#include "cmd_exit.h"
#include "cmd_registry.h"
#include "argtable3.h"

/* ── argtable builder ──────────────────────────────────────────────────── */

static void build_exit_argtable(struct arg_lit **help,
                                struct arg_lit **help_json,
                                struct arg_end **end,
                                void          ***argtable_out)
{
    *help      = arg_lit0("h", "help",      "show this help and exit");
    *help_json = arg_lit0(NULL, "help-json", "print argument schema as JSON and exit");
    *end       = arg_end(20);

    static void *argtable[4];
    argtable[0] = *help;
    argtable[1] = *help_json;
    argtable[2] = *end;
    argtable[3] = NULL;
    *argtable_out = argtable;
}

/* ── run ───────────────────────────────────────────────────────────────── */

/*
 * exit_run - Exit the shell or program.
 *
 * Implements the 'exit' command. Handles help option and exits the shell
 * with a success status. No arguments are required.
 *
 * Input:
 *   argc - Number of command-line arguments.
 *   argv - Array of argument strings.
 *
 * Output:
 *   Returns 0 on success (unreachable, as exit is called).
 */
int exit_run(int argc, char **argv)
{
    struct arg_lit *help;
    struct arg_lit *help_json;
    struct arg_end *end;
    void          **argtable;

    build_exit_argtable(&help, &help_json, &end, &argtable);

    // Parse command-line arguments and handle help/errors
    int nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0)
    {
        arg_freetable(argtable, 3);
        exit_print_usage(stdout);
        return 0;
    }
    if (help_json->count > 0)
    {
        cmd_print_help_json(stdout, "exit",
                            "exit the shell",
                            "Cause the shell to exit immediately with a successful status (0). "
                            "All built-in state is discarded and the process terminates.",
                            argtable);
        arg_freetable(argtable, 3);
        return 0;
    }
    if (nerrors > 0)
    {
        arg_print_errors(stdout, end, "exit");
        arg_freetable(argtable, 3);
        exit_print_usage(stdout);
        return 1;
    }

    // Exit the shell with success status
    arg_freetable(argtable, 3);
    exit(EXIT_SUCCESS);
    return 0; /* unreachable */
}

/* ── print_usage ───────────────────────────────────────────────────────── */

void exit_print_usage(FILE *out)
{
    struct arg_lit *help;
    struct arg_lit *help_json;
    struct arg_end *end;
    void          **argtable;

    build_exit_argtable(&help, &help_json, &end, &argtable);

    fprintf(out, "\nUsage: exit ");
    arg_print_syntax(out, argtable, "\n");
    fprintf(out, "\nExit the shell immediately with a successful status (0).\n");
    fprintf(out, "All built-in state is discarded and the process terminates.\n");
    fprintf(out, "\nOptions:\n");
    arg_print_glossary(out, argtable, "  %-22s %s\n");
    fprintf(out, "\n");

    arg_freetable(argtable, 3);
}

/* ── spec + registration ───────────────────────────────────────────────── */

cmd_spec_t cmd_exit_spec =
{
    .name        = "exit",
    .summary     = "exit the shell",
    .long_help   = "Cause the shell to exit immediately with a successful status (0). "
                   "All built-in state is discarded and the process terminates.",
    .run         = exit_run,
    .print_usage = exit_print_usage,
};

void register_exit_command(void)
{
    register_command(&cmd_exit_spec);
}
