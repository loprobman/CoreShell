#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "cmd_cd.h"
#include "cmd_registry.h"
#include "argtable3.h"

/* ── argtable builder ──────────────────────────────────────────────────── */

static void build_cd_argtable(struct arg_lit  **help,
                               struct arg_lit  **help_json,
                               struct arg_lit  **json,
                               struct arg_file **dir,
                               struct arg_end  **end,
                               void           ***argtable_out)
{
    *help      = arg_lit0("h", "help",       "show this help and exit");
    *help_json = arg_lit0(NULL, "help-json",   "print argument schema as JSON and exit");
    *json      = arg_lit0(NULL, "json",        "output result as JSON");
    *dir       = arg_file0(NULL, NULL, "[dir]",
                           "directory to change to (default: $HOME)");
    *end       = arg_end(20);

    static void *argtable[6];
    argtable[0] = *help;
    argtable[1] = *help_json;
    argtable[2] = *json;
    argtable[3] = *dir;
    argtable[4] = *end;
    argtable[5] = NULL;
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
    struct arg_lit  *help_json;
    struct arg_lit  *json;
    struct arg_file *dir;
    struct arg_end  *end;
    void           **argtable;

    build_cd_argtable(&help, &help_json, &json, &dir, &end, &argtable);

    int nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0)
    {
        arg_freetable(argtable, 5);
        cd_print_usage(stdout);
        return 0;
    }
    if (help_json->count > 0)
    {
        cmd_print_help_json(stdout, "cd",
                            "change the current directory",
                            "Change the current working directory to the specified path, "
                            "or $HOME if no path is given. If $HOME is unset, falls back to '/'.",
                            argtable);
        arg_freetable(argtable, 5);
        return 0;
    }
    if (nerrors > 0)
    {
        arg_print_errors(stdout, end, "cd");
        arg_freetable(argtable, 5);
        cd_print_usage(stdout);
        return 1;
    }

    const char *target = (dir->count > 0) ? dir->filename[0] : getenv("HOME");
    if (target == NULL)
    {
        target = "/";
    }

    if (chdir(target) != 0)
    {
        perror("cd");
        arg_freetable(argtable, 5);
        return 1;
    }

    if (json->count > 0)
    {
        char newcwd[4096];
        if (getcwd(newcwd, sizeof(newcwd)) == NULL)
            snprintf(newcwd, sizeof(newcwd), "%s", target);
        printf("{\n  \"path\": ");
        cmd_json_str(stdout, newcwd);
        printf("\n}\n");
    }

    arg_freetable(argtable, 5);
    return 0;
}

/* ── print_usage ───────────────────────────────────────────────────────── */

void cd_print_usage(FILE *out)
{
    struct arg_lit  *help;
    struct arg_lit  *help_json;
    struct arg_lit  *json;
    struct arg_file *dir;
    struct arg_end  *end;
    void           **argtable;

    build_cd_argtable(&help, &help_json, &json, &dir, &end, &argtable);

    fprintf(out, "\nUsage: cd ");
    arg_print_syntax(out, argtable, "\n");
    fprintf(out, "\nChange the current working directory.\n");
    fprintf(out, "With no argument, changes to the directory named in $HOME.\n");
    fprintf(out, "If $HOME is not set, falls back to '/' .\n");
    fprintf(out, "\nOptions:\n");
    arg_print_glossary(out, argtable, "  %-22s %s\n");
    fprintf(out, "\nExamples:\n");
    fprintf(out, "  cd /tmp\n");
    fprintf(out, "  cd          # goes to $HOME\n");
    fprintf(out, "\n");

    arg_freetable(argtable, 5);
}

/* ── spec + registration ───────────────────────────────────────────────── */

cmd_spec_t cmd_cd_spec =
{
    .name        = "cd",
    .summary     = "change the current directory",
    .long_help   = "Change the current working directory to the specified path, "
                   "or $HOME if no path is given. If $HOME is unset, falls back to '/'.",
    .run         = cd_run,
    .print_usage = cd_print_usage,
};

void register_cd_command(void)
{
    register_command(&cmd_cd_spec);
}
