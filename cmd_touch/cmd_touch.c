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
                                  struct arg_lit  **help_json,
                                  struct arg_lit  **json,
                                  struct arg_lit  **no_create,
                                  struct arg_file **files,
                                  struct arg_end  **end,
                                  void           ***argtable_out)
{
    *help      = arg_lit0("h", "help",
                          "show this help and exit");
    *help_json = arg_lit0(NULL, "help-json",
                          "print argument schema as JSON and exit");
    *json      = arg_lit0(NULL, "json",
                          "output result as JSON");
    *no_create = arg_lit0("c", "no-create",
                          "do not create any files");
    *files     = arg_filen(NULL, NULL, "<file>", 1, 64,
                           "files to create or update");
    *end       = arg_end(20);

    static void *argtable[7];
    argtable[0] = *help;
    argtable[1] = *help_json;
    argtable[2] = *json;
    argtable[3] = *no_create;
    argtable[4] = *files;
    argtable[5] = *end;
    argtable[6] = NULL;
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
    struct arg_lit  *help_json;
    struct arg_lit  *json;
    struct arg_lit  *no_create;
    struct arg_file *files;
    struct arg_end  *end;
    void           **argtable;

    build_touch_argtable(&help, &help_json, &json, &no_create, &files, &end, &argtable);

    int nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0)
    {
        arg_freetable(argtable, 6);
        touch_print_usage(stdout);
        return 0;
    }
    if (help_json->count > 0)
    {
        cmd_print_help_json(stdout, "touch",
                            "create a file or update its timestamp",
                            "Update the access and modification timestamps of each file."
                            " Create the file if it does not exist, unless -c is specified.",
                            argtable);
        arg_freetable(argtable, 6);
        return 0;
    }
    if (nerrors > 0)
    {
        arg_print_errors(stdout, end, "touch");
        arg_freetable(argtable, 6);
        touch_print_usage(stdout);
        return 1;
    }

    int ret = 0;
    for (int i = 0; i < files->count; i++)
    {
        const char *path = files->filename[i];

        if (utime(path, NULL) == 0)
            continue;

        if (errno == ENOENT)
        {
            if (no_create->count > 0)
                continue;
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

    if (json->count > 0)
    {
        printf("{\n  \"status\": ");
        cmd_json_str(stdout, ret == 0 ? "ok" : "error");
        printf(",\n  \"touched\": [");
        for (int i = 0; i < files->count; i++)
        {
            if (i > 0) printf(", ");
            cmd_json_str(stdout, files->filename[i]);
        }
        printf("]\n}\n");
    }

    arg_freetable(argtable, 6);
    return ret;
}

/* ── print_usage ───────────────────────────────────────────────────────── */

void touch_print_usage(FILE *out)
{
    struct arg_lit  *help;
    struct arg_lit  *help_json;
    struct arg_lit  *json;
    struct arg_lit  *no_create;
    struct arg_file *files;
    struct arg_end  *end;
    void           **argtable;

    build_touch_argtable(&help, &help_json, &json, &no_create, &files, &end, &argtable);

    fprintf(out, "\nUsage: touch ");
    arg_print_syntax(out, argtable, "\n");
    fprintf(out, "\nUpdate the access and modification timestamps of each file to the current time.\n");
    fprintf(out, "If a file does not exist it is created (unless -c is given).\n");
    fprintf(out, "\nOptions:\n");
    arg_print_glossary(out, argtable, "  %-22s %s\n");
    fprintf(out, "\nExamples:\n");
    fprintf(out, "  touch newfile.txt\n");
    fprintf(out, "  touch -c maybe.txt\n");
    fprintf(out, "\n");

    arg_freetable(argtable, 6);
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
