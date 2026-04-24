#include <stdio.h>
#include <errno.h>
#include "cmd_mv.h"
#include "cmd_registry.h"
#include "argtable3.h"

/* ── argtable builder ──────────────────────────────────────────────────── */

static void build_mv_argtable(struct arg_lit  **help,
                               struct arg_lit  **verbose,
                               struct arg_file **files,
                               struct arg_end  **end,
                               void           ***argtable_out)
{
    *help    = arg_lit0("h", "help",    "show this help and exit");
    *verbose = arg_lit0("v", "verbose", "explain what is being done");
    *files   = arg_filen(NULL, NULL, "<src> <dst>", 2, 2,
                         "source and destination");
    *end     = arg_end(20);

    static void *argtable[5];
    argtable[0] = *help;
    argtable[1] = *verbose;
    argtable[2] = *files;
    argtable[3] = *end;
    argtable[4] = NULL;
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
    struct arg_lit  *verbose;
    struct arg_file *files;
    struct arg_end  *end;
    void           **argtable;

    build_mv_argtable(&help, &verbose, &files, &end, &argtable);

    // Parse command-line arguments and handle help/errors
    int nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0)
    {
        arg_freetable(argtable, 4);
        mv_print_usage(stdout);
        return 0;
    }
    if (nerrors > 0)
    {
        arg_print_errors(stdout, end, "mv");
        arg_freetable(argtable, 4);
        mv_print_usage(stdout);
        return 1;
    }

    const char *src = files->filename[0];
    const char *dst = files->filename[1];

    if (rename(src, dst) != 0)
    {
        perror("mv");
        arg_freetable(argtable, 4);
        return 1;
    }

    if (verbose->count > 0)
    {
        printf("'%s' -> '%s'\n", src, dst);
    }

    arg_freetable(argtable, 4);
    return 0;
}

/* ── print_usage ───────────────────────────────────────────────────────── */

void mv_print_usage(FILE *out)
{
    struct arg_lit  *help;
    struct arg_lit  *verbose;
    struct arg_file *files;
    struct arg_end  *end;
    void           **argtable;

    build_mv_argtable(&help, &verbose, &files, &end, &argtable);

    fprintf(out, "\nUsage: mv ");
    arg_print_syntax(out, argtable, "\n");
    fprintf(out, "\nMove (rename) SOURCE to DEST.\n");
    fprintf(out, "\nOptions:\n");
    arg_print_glossary(out, argtable, "  %-22s %s\n");
    fprintf(out, "\n");

    arg_freetable(argtable, 4);
}

/* ── spec + registration ───────────────────────────────────────────────── */

cmd_spec_t cmd_mv_spec =
{
    .name        = "mv",
    .summary     = "move (rename) a file or directory",
    .long_help   = "Rename SOURCE to DEST, moving it if they are on the same "
                   "filesystem.",
    .run         = mv_run,
    .print_usage = mv_print_usage,
};

void register_mv_command(void)
{
    register_command(&cmd_mv_spec);
}
