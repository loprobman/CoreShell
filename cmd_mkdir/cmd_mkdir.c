#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <limits.h>
#include "cmd_mkdir.h"
#include "cmd_registry.h"
#include "argtable3.h"

/* ── argtable builder ──────────────────────────────────────────────────── */

static void build_mkdir_argtable(struct arg_lit  **help,
                                  struct arg_lit  **parents,
                                  struct arg_lit  **verbose,
                                  struct arg_file **dirs,
                                  struct arg_end  **end,
                                  void           ***argtable_out)
{
    *help    = arg_lit0("h", "help",    "show this help and exit");
    *parents = arg_lit0("p", "parents", "create parent directories as needed");
    *verbose = arg_lit0("v", "verbose", "print each created directory");
    *dirs    = arg_filen(NULL, NULL, "<dir>", 1, 64, "directories to create");
    *end     = arg_end(20);

    static void *argtable[6];
    argtable[0] = *help;
    argtable[1] = *parents;
    argtable[2] = *verbose;
    argtable[3] = *dirs;
    argtable[4] = *end;
    argtable[5] = NULL;
    *argtable_out = argtable;
}

/* ── helpers ───────────────────────────────────────────────────────────── */

/* Create all components of path, like `mkdir -p` */
static int mkdir_p(const char *path, mode_t mode, int verbose)
{
    char tmp[PATH_MAX];
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    for (char *p = tmp + 1; *p; p++)
    {
        if (*p == '/')
        {
            *p = '\0';
            if (mkdir(tmp, mode) != 0 && errno != EEXIST)
            {
                perror(tmp);
                return 1;
            }
            *p = '/';
        }
    }
    if (mkdir(tmp, mode) != 0 && errno != EEXIST)
    {
        perror(tmp);
        return 1;
    }
    if (verbose) printf("created directory '%s'\n", path);
    return 0;
}

/* ── run ───────────────────────────────────────────────────────────────── */

/*
 * mkdir_run - Create directories.
 *
 * Implements the 'mkdir' command. Handles options for creating parent directories,
 * verbose output, and multiple directory arguments. Creates directories as specified.
 *
 * Input:
 *   argc - Number of command-line arguments.
 *   argv - Array of argument strings.
 *
 * Output:
 *   Returns 0 on success, nonzero on error.
 */
int mkdir_run(int argc, char **argv)
{
    struct arg_lit  *help;
    struct arg_lit  *parents;
    struct arg_lit  *verbose;
    struct arg_file *dirs;
    struct arg_end  *end;
    void           **argtable;

    build_mkdir_argtable(&help, &parents, &verbose, &dirs, &end, &argtable);

    // Parse command-line arguments and handle help/errors
    int nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0)
    {
        arg_freetable(argtable, 5);
        mkdir_print_usage(stdout);
        return 0;
    }
    if (nerrors > 0)
    {
        arg_print_errors(stdout, end, "mkdir");
        arg_freetable(argtable, 5);
        mkdir_print_usage(stdout);
        return 1;
    }

    int ret = 0;
    for (int i = 0; i < dirs->count; i++)
    {
        if (parents->count > 0)
        {
            if (mkdir_p(dirs->filename[i], 0755, verbose->count) != 0)
            {
                ret = 1;
            }
        }
        else
        {
            if (mkdir(dirs->filename[i], 0755) != 0)
            {
                perror(dirs->filename[i]);
                ret = 1;
            }
            else if (verbose->count)
            {
                printf("created directory '%s'\n", dirs->filename[i]);
            }
        }
    }

    arg_freetable(argtable, 5);
    return ret;
}

/* ── print_usage ───────────────────────────────────────────────────────── */

void mkdir_print_usage(FILE *out)
{
    struct arg_lit  *help;
    struct arg_lit  *parents;
    struct arg_lit  *verbose;
    struct arg_file *dirs;
    struct arg_end  *end;
    void           **argtable;

    build_mkdir_argtable(&help, &parents, &verbose, &dirs, &end, &argtable);

    fprintf(out, "\nUsage: mkdir ");
    arg_print_syntax(out, argtable, "\n");
    fprintf(out, "\nCreate one or more directories.\n");
    fprintf(out, "\nOptions:\n");
    arg_print_glossary(out, argtable, "  %-22s %s\n");
    fprintf(out, "\n");

    arg_freetable(argtable, 5);
}

/* ── spec + registration ───────────────────────────────────────────────── */

cmd_spec_t cmd_mkdir_spec =
{
    .name        = "mkdir",
    .summary     = "create directories",
    .long_help   = "Create one or more directories. Use -p to create parent "
                   "directories as needed.",
    .run         = mkdir_run,
    .print_usage = mkdir_print_usage,
};

void register_mkdir_command(void)
{
    register_command(&cmd_mkdir_spec);
}
