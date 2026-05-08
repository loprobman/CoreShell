#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "cmd_pwd.h"
#include "cmd_registry.h"
#include "argtable3.h"

#define PWD_BUF 4096

/* ── argtable builder ──────────────────────────────────────────────────── */

static void build_pwd_argtable(struct arg_lit **help,
                               struct arg_lit **logical,
                               struct arg_lit **physical,
                               struct arg_end **end,
                               void          ***argtable_out)
{
    *help     = arg_lit0("h", "help",
                         "show this help and exit");
    *logical  = arg_lit0("L", "logical",
                         "use $PWD from environment (may contain symlinks)");
    *physical = arg_lit0("P", "physical",
                         "print physical path, resolving all symlinks");
    *end      = arg_end(20);

    static void *argtable[5];
    argtable[0] = *help;
    argtable[1] = *logical;
    argtable[2] = *physical;
    argtable[3] = *end;
    argtable[4] = NULL;
    *argtable_out = argtable;
}

/* ── run ───────────────────────────────────────────────────────────────── */

/*
 * pwd_run - Print the current working directory.
 *
 * Implements the 'pwd' command. Handles options for logical and physical path
 * resolution, then prints the current directory path.
 *
 * Input:
 *   argc - Number of command-line arguments.
 *   argv - Array of argument strings.
 *
 * Output:
 *   Returns 0 on success, nonzero on error.
 */
int pwd_run(int argc, char **argv)
{
    struct arg_lit *help;
    struct arg_lit *logical;
    struct arg_lit *physical;
    struct arg_end *end;
    void          **argtable;

    build_pwd_argtable(&help, &logical, &physical, &end, &argtable);

    // Parse command-line arguments and handle help/errors
    int nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0)
    {
        arg_freetable(argtable, 4);
        pwd_print_usage(stdout);
        return 0;
    }
    if (nerrors > 0)
    {
        arg_print_errors(stdout, end, "pwd");
        arg_freetable(argtable, 4);
        pwd_print_usage(stdout);
        return 1;
    }

    /* -L: use logical $PWD (may have unresolved symlinks) */
    if (logical->count > 0 && physical->count == 0)
    {
        const char *env_pwd = getenv("PWD");
        if (env_pwd != NULL)
        {
            printf("%s\n", env_pwd);
            arg_freetable(argtable, 4);
            return 0;
        }
    }

    /* Default / -P: use getcwd() for the physical path */
    char cwd[PWD_BUF];
    if (getcwd(cwd, sizeof(cwd)) == NULL)
    {
        perror("pwd");
        arg_freetable(argtable, 4);
        return 1;
    }
    printf("%s\n", cwd);

    arg_freetable(argtable, 4);
    return 0;
}

/* ── print_usage ───────────────────────────────────────────────────────── */

void pwd_print_usage(FILE *out)
{
    struct arg_lit *help;
    struct arg_lit *logical;
    struct arg_lit *physical;
    struct arg_end *end;
    void          **argtable;

    build_pwd_argtable(&help, &logical, &physical, &end, &argtable);

    fprintf(out, "\nUsage: pwd ");
    arg_print_syntax(out, argtable, "\n");
    fprintf(out, "\nPrint the absolute path of the current working directory.\n");
    fprintf(out, "\nOptions:\n");
    arg_print_glossary(out, argtable, "  %%-22s %s\n");
    fprintf(out, "\n");

    arg_freetable(argtable, 4);
}

/* ── spec + registration ───────────────────────────────────────────────── */

cmd_spec_t cmd_pwd_spec =
{
    .name        = "pwd",
    .summary     = "print working directory",
    .long_help   = "Displays the absolute path of the current directory.",
    .run         = pwd_run,
    .print_usage = pwd_print_usage,
};

void register_pwd_command(void)
{
    register_command(&cmd_pwd_spec);
}