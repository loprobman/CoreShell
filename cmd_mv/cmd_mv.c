#include <stdio.h>
#include <errno.h>
#include "cmd_mv.h"
#include "cmd_registry.h"
#include "argtable3.h"

/* ── argtable builder ──────────────────────────────────────────────────── */

static void build_mv_argtable(struct arg_lit  **help,
                               struct arg_lit  **help_json,
                               struct arg_lit  **json,
                               struct arg_lit  **verbose,
                               struct arg_file **files,
                               struct arg_end  **end,
                               void           ***argtable_out)
{
    *help      = arg_lit0("h", "help",       "show this help and exit");
    *help_json = arg_lit0(NULL, "help-json",  "print argument schema as JSON and exit");
    *json      = arg_lit0(NULL, "json",        "output result as JSON");
    *verbose   = arg_lit0("v", "verbose",      "explain what is being done");
    *files     = arg_filen(NULL, NULL, "<src> <dst>", 2, 2,
                           "source and destination");
    *end       = arg_end(20);

    static void *argtable[7];
    argtable[0] = *help;
    argtable[1] = *help_json;
    argtable[2] = *json;
    argtable[3] = *verbose;
    argtable[4] = *files;
    argtable[5] = *end;
    argtable[6] = NULL;
    *argtable_out = argtable;
}

/* ── run ───────────────────────────────────────────────────────────────── */

/*
 * mv_run - Move or rename files and directories.
 *
 * Implements the 'mv' command. Handles options for verbose output and
 * parses source and destination arguments. Moves or renames files as specified.
 *
 * Input:
 *   argc - Number of command-line arguments.
 *   argv - Array of argument strings.
 *
 * Output:
 *   Returns 0 on success, nonzero on error.
 */
int mv_run(int argc, char **argv)
{
    struct arg_lit  *help;
    struct arg_lit  *help_json;
    struct arg_lit  *json;
    struct arg_lit  *verbose;
    struct arg_file *files;
    struct arg_end  *end;
    void           **argtable;

    build_mv_argtable(&help, &help_json, &json, &verbose, &files, &end, &argtable);

    int nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0)
    {
        arg_freetable(argtable, 6);
        mv_print_usage(stdout);
        return 0;
    }
    if (help_json->count > 0)
    {
        cmd_print_help_json(stdout, "mv",
                            "move (rename) a file or directory",
                            "Rename SOURCE to DEST using the rename(2) syscall. "
                            "SOURCE and DEST must reside on the same filesystem. "
                            "Use -v to report the operation.",
                            argtable);
        arg_freetable(argtable, 6);
        return 0;
    }
    if (nerrors > 0)
    {
        arg_print_errors(stdout, end, "mv");
        arg_freetable(argtable, 6);
        mv_print_usage(stdout);
        return 1;
    }

    const char *src = files->filename[0];
    const char *dst = files->filename[1];

    int ret = 0;
    if (rename(src, dst) != 0)
    {
        perror("mv");
        ret = 1;
    }
    else if (verbose->count > 0)
    {
        printf("'%s' -> '%s'\n", src, dst);
    }

    if (json->count > 0)
    {
        printf("{\n  \"status\": ");
        cmd_json_str(stdout, ret == 0 ? "ok" : "error");
        printf(",\n  \"src\": ");  cmd_json_str(stdout, src);
        printf(",\n  \"dst\": ");  cmd_json_str(stdout, dst);
        printf("\n}\n");
    }

    arg_freetable(argtable, 6);
    return ret;
}

/* ── print_usage ───────────────────────────────────────────────────────── */

void mv_print_usage(FILE *out)
{
    struct arg_lit  *help;
    struct arg_lit  *help_json;
    struct arg_lit  *json;
    struct arg_lit  *verbose;
    struct arg_file *files;
    struct arg_end  *end;
    void           **argtable;

    build_mv_argtable(&help, &help_json, &json, &verbose, &files, &end, &argtable);

    fprintf(out, "\nUsage: mv ");
    arg_print_syntax(out, argtable, "\n");
    fprintf(out, "\nMove (rename) SOURCE to DEST.\n");
    fprintf(out, "Uses the rename(2) syscall; SOURCE and DEST must be on the same filesystem.\n");
    fprintf(out, "Use -v to report the rename operation.\n");
    fprintf(out, "\nOptions:\n");
    arg_print_glossary(out, argtable, "  %-22s %s\n");
    fprintf(out, "\nExamples:\n");
    fprintf(out, "  mv old.txt new.txt\n");
    fprintf(out, "  mv -v src/ dst/\n");
    fprintf(out, "\n");

    arg_freetable(argtable, 6);
}

/* ── spec + registration ───────────────────────────────────────────────── */

cmd_spec_t cmd_mv_spec =
{
    .name        = "mv",
    .summary     = "move (rename) a file or directory",
    .long_help   = "Rename SOURCE to DEST using the rename(2) syscall. "
                   "SOURCE and DEST must reside on the same filesystem. "
                   "Use -v to report the operation.",
    .run         = mv_run,
    .print_usage = mv_print_usage,
};

void register_mv_command(void)
{
    register_command(&cmd_mv_spec);
}
