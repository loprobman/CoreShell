#include <stdio.h>
#include <string.h>
#include "cmd_help.h"
#include "cmd_registry.h"
#include "argtable3.h"

/* ── argtable builder ──────────────────────────────────────────────────── */

static void build_help_argtable(struct arg_str **cmd,
                                struct arg_lit **help,
                                struct arg_lit **help_json,
                                struct arg_lit **json,
                                struct arg_end **end,
                                void          ***argtable_out)
{
    *cmd       = arg_str0(NULL, NULL, "[command]", "command to show help for");
    *help      = arg_lit0("h", "help",      "show this help and exit");
    *help_json = arg_lit0(NULL, "help-json", "print argument schema as JSON and exit");
    *json      = arg_lit0(NULL, "json",       "output command list as JSON");
    *end       = arg_end(20);

    static void *argtable[6];
    argtable[0] = *cmd;
    argtable[1] = *help;
    argtable[2] = *help_json;
    argtable[3] = *json;
    argtable[4] = *end;
    argtable[5] = NULL;
    *argtable_out = argtable;
}

/* ── helpers ───────────────────────────────────────────────────────────── */

static void print_summary_cb(const cmd_spec_t *spec, void *userdata)
{
    (void)userdata;
    printf("  %-16s %s\n", spec->name, spec->summary);
}

typedef struct { int first; } json_list_ctx_t;

static void print_json_summary_cb(const cmd_spec_t *spec, void *userdata)
{
    json_list_ctx_t *ctx = (json_list_ctx_t *)userdata;
    if (!ctx->first) printf(",\n");
    printf("    {\"name\": ");
    cmd_json_str(stdout, spec->name);
    printf(", \"summary\": ");
    cmd_json_str(stdout, spec->summary);
    printf("}");
    ctx->first = 0;
}

/* ── run ───────────────────────────────────────────────────────────────── */

int help_run(int argc, char **argv)
{
    struct arg_str *cmd;
    struct arg_lit *help;
    struct arg_lit *help_json;
    struct arg_lit *json;
    struct arg_end *end;
    void          **argtable;

    build_help_argtable(&cmd, &help, &help_json, &json, &end, &argtable);

    int nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0)
    {
        arg_freetable(argtable, 5);
        help_print_usage(stdout);
        return 0;
    }
    if (help_json->count > 0)
    {
        cmd_print_help_json(stdout, "help",
                            "show help for built-in commands",
                            "Display a list of all built-in commands, or detailed help"
                            " for a specific command.",
                            argtable);
        arg_freetable(argtable, 5);
        return 0;
    }
    if (nerrors > 0)
    {
        arg_print_errors(stdout, end, "help");
        arg_freetable(argtable, 5);
        help_print_usage(stdout);
        return 1;
    }

    if (json->count > 0)
    {
        if (cmd->count > 0)
        {
            const cmd_spec_t *spec = find_command(cmd->sval[0]);
            if (spec == NULL)
            {
                fprintf(stderr, "help: unknown command '%s'\n", cmd->sval[0]);
                arg_freetable(argtable, 5);
                return 1;
            }
            printf("{\n  \"name\": ");      cmd_json_str(stdout, spec->name);
            printf(",\n  \"summary\": ");   cmd_json_str(stdout, spec->summary);
            printf(",\n  \"long_help\": "); cmd_json_str(stdout, spec->long_help);
            printf("\n}\n");
        }
        else
        {
            printf("{\n  \"commands\": [\n");
            json_list_ctx_t ctx = { .first = 1 };
            for_each_command(print_json_summary_cb, &ctx);
            printf("\n  ]\n}\n");
        }
        arg_freetable(argtable, 5);
        return 0;
    }

    if (cmd->count > 0)
    {
        const cmd_spec_t *spec = find_command(cmd->sval[0]);
        if (spec == NULL)
        {
            fprintf(stderr, "help: unknown command '%s'\n", cmd->sval[0]);
            arg_freetable(argtable, 5);
            return 1;
        }
        arg_freetable(argtable, 5);
        spec->print_usage(stdout);
    }
    else
    {
        arg_freetable(argtable, 5);
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
    struct arg_lit *help_json;
    struct arg_lit *json;
    struct arg_end *end;
    void          **argtable;

    build_help_argtable(&cmd, &help, &help_json, &json, &end, &argtable);

    fprintf(out, "\nUsage: help ");
    arg_print_syntax(out, argtable, "\n");
    fprintf(out, "\nShow general help or detailed help for a specific built-in command.\n");
    fprintf(out, "Without an argument, lists all built-in commands with their one-line summaries.\n");
    fprintf(out, "With a command name, prints that command's full usage text.\n");
    fprintf(out, "\nOptions:\n");
    arg_print_glossary(out, argtable, "  %-22s %s\n");
    fprintf(out, "\nExamples:\n");
    fprintf(out, "  help\n");
    fprintf(out, "  help ls\n");
    fprintf(out, "\n");

    arg_freetable(argtable, 5);
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
