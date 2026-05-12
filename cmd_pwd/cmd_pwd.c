#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "cmd_pwd.h"
#include "cmd_registry.h"
#include "argtable3.h"

#define PWD_BUF 4096

/* ── argtable builder ──────────────────────────────────────────────────── */

static void build_pwd_argtable(struct arg_lit **help,
                               struct arg_lit **help_json,
                               struct arg_lit **json,
                               struct arg_lit **logical,
                               struct arg_lit **physical,
                               struct arg_end **end,
                               void          ***argtable_out)
{
    *help      = arg_lit0("h", "help",      "show this help and exit");
    *help_json = arg_lit0(NULL, "help-json",  "print argument schema as JSON and exit");
    *json      = arg_lit0(NULL, "json",       "output result as JSON");
    *logical   = arg_lit0("L", "logical",
                          "use $PWD from environment (may contain symlinks)");
    *physical  = arg_lit0("P", "physical",
                          "print physical path, resolving all symlinks");
    *end       = arg_end(20);

    static void *argtable[7];
    argtable[0] = *help;
    argtable[1] = *help_json;
    argtable[2] = *json;
    argtable[3] = *logical;
    argtable[4] = *physical;
    argtable[5] = *end;
    argtable[6] = NULL;
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
    struct arg_lit *help_json;
    struct arg_lit *json;
    struct arg_lit *logical;
    struct arg_lit *physical;
    struct arg_end *end;
    void          **argtable;

    build_pwd_argtable(&help, &help_json, &json, &logical, &physical, &end, &argtable);

    int nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0)
    {
        arg_freetable(argtable, 6);
        pwd_print_usage(stdout);
        return 0;
    }
    if (help_json->count > 0)
    {
        cmd_print_help_json(stdout, "pwd",
                            "print working directory",
                            "Print the absolute path of the current working directory. "
                            "By default uses getcwd(3) to return the physical path with symlinks resolved. "
                            "Use -L to return the logical $PWD from the environment, which may contain symlinks.",
                            argtable);
        arg_freetable(argtable, 6);
        return 0;
    }
    if (nerrors > 0)
    {
        arg_print_errors(stdout, end, "pwd");
        arg_freetable(argtable, 6);
        pwd_print_usage(stdout);
        return 1;
    }

    /* -L: use logical $PWD (may have unresolved symlinks) */
    if (logical->count > 0 && physical->count == 0)
    {
        const char *env_pwd = getenv("PWD");
        if (env_pwd != NULL)
        {
            if (json->count > 0)
            {
                printf("{\n  \"path\": ");
                cmd_json_str(stdout, env_pwd);
                printf("\n}\n");
            }
            else
            {
                printf("%s\n", env_pwd);
            }
            arg_freetable(argtable, 6);
            return 0;
        }
    }

    /* Default / -P: use getcwd() for the physical path */
    char cwd[PWD_BUF];
    if (getcwd(cwd, sizeof(cwd)) == NULL)
    {
        perror("pwd");
        arg_freetable(argtable, 6);
        return 1;
    }

    if (json->count > 0)
    {
        printf("{\n  \"path\": ");
        cmd_json_str(stdout, cwd);
        printf("\n}\n");
    }
    else
    {
        printf("%s\n", cwd);
    }

    arg_freetable(argtable, 6);
    return 0;
}

/* ── print_usage ───────────────────────────────────────────────────────── */

void pwd_print_usage(FILE *out)
{
    struct arg_lit *help;
    struct arg_lit *help_json;
    struct arg_lit *json;
    struct arg_lit *logical;
    struct arg_lit *physical;
    struct arg_end *end;
    void          **argtable;

    build_pwd_argtable(&help, &help_json, &json, &logical, &physical, &end, &argtable);

    fprintf(out, "\nUsage: pwd ");
    arg_print_syntax(out, argtable, "\n");
    fprintf(out, "\nPrint the absolute path of the current working directory.\n");
    fprintf(out, "Default behaviour uses getcwd(3) (physical path, symlinks resolved).\n");
    fprintf(out, "Use -L to return the logical $PWD value, which may contain unresolved symlinks.\n");
    fprintf(out, "\nOptions:\n");
    arg_print_glossary(out, argtable, "  %-22s %s\n");
    fprintf(out, "\n");

    arg_freetable(argtable, 6);
}

/* ── spec + registration ───────────────────────────────────────────────── */

cmd_spec_t cmd_pwd_spec =
{
    .name        = "pwd",
    .summary     = "print working directory",
    .long_help   = "Print the absolute path of the current working directory. "
                   "By default uses getcwd(3) to return the physical path with symlinks resolved. "
                   "Use -L to return the logical $PWD from the environment, which may contain symlinks.",
    .run         = pwd_run,
    .print_usage = pwd_print_usage,
};

void register_pwd_command(void)
{
    register_command(&cmd_pwd_spec);
}