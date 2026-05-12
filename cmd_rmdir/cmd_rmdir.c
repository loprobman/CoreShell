#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include "cmd_rmdir.h"
#include "cmd_registry.h"
#include "argtable3.h"

/* ── argtable builder ──────────────────────────────────────────────────── */

static void build_rmdir_argtable(struct arg_lit  **help,
                                  struct arg_lit  **help_json,
                                  struct arg_lit  **json,
                                  struct arg_lit  **parents,
                                  struct arg_file **dirs,
                                  struct arg_end  **end,
                                  void           ***argtable_out)
{
    *help      = arg_lit0("h", "help",       "show this help and exit");
    *help_json = arg_lit0(NULL, "help-json",  "print argument schema as JSON and exit");
    *json      = arg_lit0(NULL, "json",        "output result as JSON");
    *parents   = arg_lit0("p", "parents",
                          "remove each parent directory that becomes empty");
    *dirs      = arg_filen(NULL, NULL, "<dir>", 1, 64,
                           "empty directories to remove");
    *end       = arg_end(20);

    static void *argtable[7];
    argtable[0] = *help;
    argtable[1] = *help_json;
    argtable[2] = *json;
    argtable[3] = *parents;
    argtable[4] = *dirs;
    argtable[5] = *end;
    argtable[6] = NULL;
    *argtable_out = argtable;
}

/* ── helpers ───────────────────────────────────────────────────────────── */

/* Remove path and its parent directories while they are empty */
static int rmdir_p(const char *path)
{
    char tmp[PATH_MAX];
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    while (strlen(tmp) > 0)
    {
        if (rmdir(tmp) != 0)
        {
            if (errno == ENOTEMPTY || errno == EBUSY)
            {
                break; /* stop when a directory is not empty */
            }
            perror(tmp);
            return 1;
        }
        char *slash = strrchr(tmp, '/');
        if (slash == NULL) break;
        *slash = '\0';
    }
    return 0;
}

/* ── run ───────────────────────────────────────────────────────────────── */

/*
 * rmdir_run - Remove empty directories.
 *
 * Implements the 'rmdir' command. Handles options for removing parent directories
 * and multiple directory arguments. Removes directories as specified.
 *
 * Input:
 *   argc - Number of command-line arguments.
 *   argv - Array of argument strings.
 *
 * Output:
 *   Returns 0 on success, nonzero on error.
 */
int rmdir_run(int argc, char **argv)
{
    struct arg_lit  *help;
    struct arg_lit  *help_json;
    struct arg_lit  *json;
    struct arg_lit  *parents;
    struct arg_file *dirs;
    struct arg_end  *end;
    void           **argtable;

    build_rmdir_argtable(&help, &help_json, &json, &parents, &dirs, &end, &argtable);

    int nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0)
    {
        arg_freetable(argtable, 6);
        rmdir_print_usage(stdout);
        return 0;
    }
    if (help_json->count > 0)
    {
        cmd_print_help_json(stdout, "rmdir",
                            "remove empty directories",
                            "Remove one or more empty directories. Use -p to also"
                            " remove parent directories if they become empty.",
                            argtable);
        arg_freetable(argtable, 6);
        return 0;
    }
    if (nerrors > 0)
    {
        arg_print_errors(stdout, end, "rmdir");
        arg_freetable(argtable, 6);
        rmdir_print_usage(stdout);
        return 1;
    }

    int ret = 0;
    for (int i = 0; i < dirs->count; i++)
    {
        if (parents->count > 0)
        {
            if (rmdir_p(dirs->filename[i]) != 0) ret = 1;
        }
        else
        {
            if (rmdir(dirs->filename[i]) != 0)
            {
                perror(dirs->filename[i]);
                ret = 1;
            }
        }
    }

    if (json->count > 0)
    {
        printf("{\n  \"status\": ");
        cmd_json_str(stdout, ret == 0 ? "ok" : "error");
        printf(",\n  \"removed\": [");
        for (int i = 0; i < dirs->count; i++)
        {
            if (i > 0) printf(", ");
            cmd_json_str(stdout, dirs->filename[i]);
        }
        printf("]\n}\n");
    }

    arg_freetable(argtable, 6);
    return ret;
}

/* ── print_usage ───────────────────────────────────────────────────────── */

void rmdir_print_usage(FILE *out)
{
    struct arg_lit  *help;
    struct arg_lit  *help_json;
    struct arg_lit  *json;
    struct arg_lit  *parents;
    struct arg_file *dirs;
    struct arg_end  *end;
    void           **argtable;

    build_rmdir_argtable(&help, &help_json, &json, &parents, &dirs, &end, &argtable);

    fprintf(out, "\nUsage: rmdir ");
    arg_print_syntax(out, argtable, "\n");
    fprintf(out, "\nRemove empty directories.\n");
    fprintf(out, "The directory must be empty; use 'rm -r' to remove non-empty directories.\n");
    fprintf(out, "With -p, each parent component that becomes empty is also removed.\n");
    fprintf(out, "\nOptions:\n");
    arg_print_glossary(out, argtable, "  %-22s %s\n");
    fprintf(out, "\nExamples:\n");
    fprintf(out, "  rmdir emptydir\n");
    fprintf(out, "  rmdir -p a/b/c\n");
    fprintf(out, "\n");

    arg_freetable(argtable, 6);
}

/* ── spec + registration ───────────────────────────────────────────────── */

cmd_spec_t cmd_rmdir_spec =
{
    .name        = "rmdir",
    .summary     = "remove empty directories",
    .long_help   = "Remove one or more empty directories. Use -p to also "
                   "remove parent directories if they become empty.",
    .run         = rmdir_run,
    .print_usage = rmdir_print_usage,
};

void register_rmdir_command(void)
{
    register_command(&cmd_rmdir_spec);
}
