#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include "cmd_stat.h"
#include "cmd_registry.h"
#include "argtable3.h"

/* ── argtable builder ──────────────────────────────────────────────────── */

static void build_stat_argtable(struct arg_lit  **help,
                                 struct arg_file **files,
                                 struct arg_end  **end,
                                 void           ***argtable_out)
{
    *help  = arg_lit0("h", "help", "show this help and exit");
    *files = arg_filen(NULL, NULL, "<file>", 1, 64, "files to stat");
    *end   = arg_end(20);

    static void *argtable[4];
    argtable[0] = *help;
    argtable[1] = *files;
    argtable[2] = *end;
    argtable[3] = NULL;
    *argtable_out = argtable;
}

/* ── helpers ───────────────────────────────────────────────────────────── */

static int print_stat(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0)
    {
        perror(path);
        return 1;
    }

    printf("  File: %s\n",               path);
    printf("  Size: %ld\n",              (long)st.st_size);
    printf("  Mode: %o (%s%s%s%s%s%s%s%s%s%s)\n",
           st.st_mode & 07777,
           S_ISDIR(st.st_mode)  ? "d" : "-",
           (st.st_mode & S_IRUSR) ? "r" : "-",
           (st.st_mode & S_IWUSR) ? "w" : "-",
           (st.st_mode & S_IXUSR) ? "x" : "-",
           (st.st_mode & S_IRGRP) ? "r" : "-",
           (st.st_mode & S_IWGRP) ? "w" : "-",
           (st.st_mode & S_IXGRP) ? "x" : "-",
           (st.st_mode & S_IROTH) ? "r" : "-",
           (st.st_mode & S_IWOTH) ? "w" : "-",
           (st.st_mode & S_IXOTH) ? "x" : "-");
    printf("  Links: %ld\n",             (long)st.st_nlink);
    printf("  UID: %d  GID: %d\n",       st.st_uid, st.st_gid);
    printf("  Access: %s",               ctime(&st.st_atime));
    printf("  Modify: %s",               ctime(&st.st_mtime));
    printf("  Change: %s\n",             ctime(&st.st_ctime));
    return 0;
}

/* ── run ───────────────────────────────────────────────────────────────── */

/*
 * stat_run - Display file or file system status.
 *
 * Implements the 'stat' command. Parses file arguments and prints detailed
 * status information for each file specified.
 *
 * Input:
 *   argc - Number of command-line arguments.
 *   argv - Array of argument strings.
 *
 * Output:
 *   Returns 0 on success, nonzero on error.
 */
int stat_run(int argc, char **argv)
{
    struct arg_lit  *help;
    struct arg_file *files;
    struct arg_end  *end;
    void           **argtable;

    build_stat_argtable(&help, &files, &end, &argtable);

    // Parse command-line arguments and handle help/errors
    int nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0)
    {
        arg_freetable(argtable, 3);
        stat_print_usage(stdout);
        return 0;
    }
    if (nerrors > 0)
    {
        arg_print_errors(stdout, end, "stat");
        arg_freetable(argtable, 3);
        stat_print_usage(stdout);
        return 1;
    }

    int ret = 0;
    for (int i = 0; i < files->count; i++)
    {
        if (print_stat(files->filename[i]) != 0)
            ret = 1;
        if (i < files->count - 1 && ret == 0)
        {
            printf("\n");
        }
    }

    arg_freetable(argtable, 3);
    return ret;
}

/* ── print_usage ───────────────────────────────────────────────────────── */

void stat_print_usage(FILE *out)
{
    struct arg_lit  *help;
    struct arg_file *files;
    struct arg_end  *end;
    void           **argtable;

    build_stat_argtable(&help, &files, &end, &argtable);

    fprintf(out, "\nUsage: stat ");
    arg_print_syntax(out, argtable, "\n");
    fprintf(out, "\nDisplay file or file system status.\n");
    fprintf(out, "\nOptions:\n");
    arg_print_glossary(out, argtable, "  %-22s %s\n");
    fprintf(out, "\n");

    arg_freetable(argtable, 3);
}

/* ── spec + registration ───────────────────────────────────────────────── */

cmd_spec_t cmd_stat_spec =
{
    .name        = "stat",
    .summary     = "display file status",
    .long_help   = "Show size, mode, link count, owner ids, and timestamps for "
                   "one or more files.",
    .run         = stat_run,
    .print_usage = stat_print_usage,
};

void register_stat_command(void)
{
    register_command(&cmd_stat_spec);
}
