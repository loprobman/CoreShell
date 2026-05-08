#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <utime.h>
#include <errno.h>
#include "cmd_touch.h"
#include "cmd_registry.h"
#include "argtable3.h"

/* ── argtable builder ──────────────────────────────────────────────────── */

static void build_touch_argtable(struct arg_lit  **help,
                                  struct arg_lit  **no_create,
                                  struct arg_file **files,
                                  struct arg_end  **end,
                                  void           ***argtable_out)
{
    *help      = arg_lit0("h", "help",
                          "show this help and exit");
    *no_create = arg_lit0("c", "no-create",
                          "do not create any files");
    *files     = arg_filen(NULL, NULL, "<file>", 1, 64,
                           "files to create or update");
    *end       = arg_end(20);

    static void *argtable[5];
    argtable[0] = *help;
    argtable[1] = *no_create;
    argtable[2] = *files;
    argtable[3] = *end;
    argtable[4] = NULL;
    *argtable_out = argtable;
}

/* ── run ───────────────────────────────────────────────────────────────── */

/*
 * touch_run - Change file timestamps or create files.
 *
 * Implements the 'touch' command. Handles options for not creating files and
 * multiple file arguments. Updates timestamps or creates files as specified.
 *
 * Input:
 *   argc - Number of command-line arguments.
 *   argv - Array of argument strings.
 *
 * Output:
 *   Returns 0 on success, nonzero on error.
 */
int touch_run(int argc, char **argv)
{
    struct arg_lit  *help;
    struct arg_lit  *no_create;
    struct arg_file *files;
    struct arg_end  *end;
    void           **argtable;

    build_touch_argtable(&help, &no_create, &files, &end, &argtable);

    // Parse command-line arguments and handle help/errors
    int nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0)
    {
        arg_freetable(argtable, 4);
        touch_print_usage(stdout);
        return 0;
    }
    if (nerrors > 0)
    {
        arg_print_errors(stdout, end, "touch");
        arg_freetable(argtable, 4);
        touch_print_usage(stdout);
        return 1;
    }

    int ret = 0;
    for (int i = 0; i < files->count; i++)
    {
        const char *path = files->filename[i];

        /* Try to update timestamp first */
        if (utime(path, NULL) == 0)
        {
            continue; /* success: file existed and timestamp updated */
        }

        if (errno == ENOENT)
        {
            if (no_create->count > 0)
            {
                continue; /* -c: silently skip non-existent files */
            }
            /* Create the file */
            int fd = open(path, O_CREAT | O_WRONLY, 0644);
            if (fd < 0)
            {
                perror(path);
                ret = 1;
            }
            else
            {
                close(fd);
            }
        }
        else
        {
            perror(path);
            ret = 1;
        }
    }

    arg_freetable(argtable, 4);
    return ret;
}

/* ── print_usage ───────────────────────────────────────────────────────── */

void touch_print_usage(FILE *out)
{
    struct arg_lit  *help;
    struct arg_lit  *no_create;
    struct arg_file *files;
    struct arg_end  *end;
    void           **argtable;

    build_touch_argtable(&help, &no_create, &files, &end, &argtable);

    fprintf(out, "\nUsage: touch ");
    arg_print_syntax(out, argtable, "\n");
    fprintf(out, "\nUpdate file timestamps, or create the file if it does not exist.\n");
    fprintf(out, "\nOptions:\n");
    arg_print_glossary(out, argtable, "  %-22s %s\n");
    fprintf(out, "\n");

    arg_freetable(argtable, 4);
}

/* ── spec + registration ───────────────────────────────────────────────── */

cmd_spec_t cmd_touch_spec =
{
    .name        = "touch",
    .summary     = "create a file or update its timestamp",
    .long_help   = "Update the access and modification timestamps of each file. "
                   "Create the file if it does not exist, unless -c is specified.",
    .run         = touch_run,
    .print_usage = touch_print_usage,
};

void register_touch_command(void)
{
    register_command(&cmd_touch_spec);
}
