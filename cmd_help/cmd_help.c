#include <stdio.h>
#include <string.h>
#include "cmd_help.h"
#include "cmd_registry.h"
#include "argtable3.h"

/* ── argtable builder ──────────────────────────────────────────────────── */

static void build_help_argtable(struct arg_str **cmd,
                                struct arg_lit **help,
                                struct arg_end **end,
                                void          ***argtable_out)
{
    *cmd  = arg_str0(NULL, NULL, "[command]", "command to show help for");
    *help = arg_lit0("h", "help", "show this help and exit");
    *end  = arg_end(20);

    static void *argtable[4];
    argtable[0] = *cmd;
    argtable[1] = *help;
    argtable[2] = *end;
    argtable[3] = NULL;
    *argtable_out = argtable;
}

/* ── helpers ───────────────────────────────────────────────────────────── */

static void print_summary_cb(const cmd_spec_t *spec, void *userdata)
{
    (void)userdata;
    printf("  %-16s %s\n", spec->name, spec->summary);
}

/* ── run ───────────────────────────────────────────────────────────────── */

int help_run(int argc, char **argv)
{
    struct arg_str *cmd;
    struct arg_lit *help;
    struct arg_end *end;
    void          **argtable;

    build_help_argtable(&cmd, &help, &end, &argtable);

    int nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0)
    {
        arg_freetable(argtable, 3);
        help_print_usage(stdout);
        return 0;
    }
    if (nerrors > 0)
    {
        arg_print_errors(stdout, end, "help");
        arg_freetable(argtable, 3);
        help_print_usage(stdout);
        return 1;
    }

    if (cmd->count > 0)
    {
        const cmd_spec_t *spec = find_command(cmd->sval[0]);
        if (spec == NULL)
        {
            fprintf(stderr, "help: unknown command '%s'\n", cmd->sval[0]);
            arg_freetable(argtable, 3);
            return 1;
        }
        arg_freetable(argtable, 3);
        spec->print_usage(stdout);
    }
    else
    {
        arg_freetable(argtable, 3);
        printf("\nCoreShell built-in commands:\n\n");
        for_each_command(print_summary_cb, NULL);
        printf("\nType 'help <command>' for detailed help on a specific command.\n\n");
    }

    return 0;
}

/* ── print_usage ───────────────────────────────────────────────────────── */

void help_print_usage(FILE *out)
{
    struct arg_str *cmd;
    struct arg_lit *help;
    struct arg_end *end;
    void          **argtable;

    build_help_argtable(&cmd, &help, &end, &argtable);

    fprintf(out, "\nUsage: help ");
    arg_print_syntax(out, argtable, "\n");
    fprintf(out, "\nShow general help or detailed help for a specific command.\n");
    fprintf(out, "\nOptions:\n");
    arg_print_glossary(out, argtable, "  %-22s %s\n");
    fprintf(out, "\n");

    arg_freetable(argtable, 3);
}

/* ── spec + registration ───────────────────────────────────────────────── */

cmd_spec_t cmd_help_spec =
{
    .name        = "help",
    .summary     = "show help for built-in commands",
    .long_help   = "Display a list of all built-in commands, or detailed help "
                   "for a specific command.",
    .run         = help_run,
    .print_usage = help_print_usage,
};

void register_help_command(void)
{
    register_command(&cmd_help_spec);
}
