#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "cmd_cd.h"
#include "cmd_registry.h"
#include "argtable3.h"

/* ── argtable builder ──────────────────────────────────────────────────── */

static void build_cd_argtable(struct arg_lit  **help,
                               struct arg_file **dir,
                               struct arg_end  **end,
                               void           ***argtable_out)
{
    *help = arg_lit0("h", "help", "show this help and exit");
    *dir  = arg_file0(NULL, NULL, "[dir]",
                      "directory to change to (default: $HOME)");
    *end  = arg_end(20);

    static void *argtable[4];
    argtable[0] = *help;
    argtable[1] = *dir;
    argtable[2] = *end;
    argtable[3] = NULL;
    *argtable_out = argtable;
}

/* ── run ───────────────────────────────────────────────────────────────── */

/*
 * cd_run - Change the current working directory.
 *
 * Implements the 'cd' command. Parses the target directory argument
 * (or defaults to $HOME), attempts to change the directory, and
 * handles errors and help output.
 *
 * Input:
 *   argc - Number of command-line arguments.
 *   argv - Array of argument strings.
 *
 * Output:
 *   Returns 0 on success, nonzero on error.
 */
int cd_run(int argc, char **argv)
{
    struct arg_lit  *help;
    struct arg_file *dir;
    struct arg_end  *end;
    void           **argtable;

    build_cd_argtable(&help, &dir, &end, &argtable);

    // Parse command-line arguments and handle help/errors
    int nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0)
    {
        arg_freetable(argtable, 3);
        cd_print_usage(stdout);
        return 0;
    }
    if (nerrors > 0)
    {
        arg_print_errors(stdout, end, "cd");
        arg_freetable(argtable, 3);
        cd_print_usage(stdout);
        return 1;
    }

    // Determine target directory (argument or $HOME)
    const char *target = (dir->count > 0) ? dir->filename[0] : getenv("HOME");
    if (target == NULL)
    {
        target = "/";
    }

    if (chdir(target) != 0)
    {
        perror("cd");
        arg_freetable(argtable, 3);
        return 1;
    }

    arg_freetable(argtable, 3);
    return 0;
}

/* ── print_usage ───────────────────────────────────────────────────────── */

void cd_print_usage(FILE *out)
{
    struct arg_lit  *help;
    struct arg_file *dir;
    struct arg_end  *end;
    void           **argtable;

    build_cd_argtable(&help, &dir, &end, &argtable);

    fprintf(out, "\nUsage: cd ");
    arg_print_syntax(out, argtable, "\n");
    fprintf(out, "\nChange the current working directory.\n");
    fprintf(out, "With no argument, changes to $HOME.\n");
    fprintf(out, "\nOptions:\n");
    arg_print_glossary(out, argtable, "  %-22s %s\n");
    fprintf(out, "\n");

    arg_freetable(argtable, 3);
}

/* ── spec + registration ───────────────────────────────────────────────── */

cmd_spec_t cmd_cd_spec =
{
    .name        = "cd",
    .summary     = "change the current directory",
    .long_help   = "Change the current working directory to the specified path, "
                   "or $HOME if no path is given.",
    .run         = cd_run,
    .print_usage = cd_print_usage,
};

void register_cd_command(void)
{
    register_command(&cmd_cd_spec);
}
