/*
 * rm_run - Remove files or directories.
 *
 * Implements the 'rm' command. Handles options for recursive removal, force,
 * verbose output, and multiple file/directory arguments. Removes files as specified.
 *
 * Input:
 *   argc - Number of command-line arguments.
 *   argv - Array of argument strings.
 *
 * Output:
 *   Returns 0 on success, nonzero on error.
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include "cmd_rm.h"
#include "cmd_registry.h"
#include "argtable3.h"

/* ── argtable builder ──────────────────────────────────────────────────── */

static void build_rm_argtable(struct arg_lit  **help,
                               struct arg_lit  **recursive,
                               struct arg_lit  **force,
                               struct arg_lit  **verbose,
                               struct arg_file **files,
                               struct arg_end  **end,
                               void           ***argtable_out)
{
    *help      = arg_lit0("h", "help",      "show this help and exit");
    *recursive = arg_lit0("r", "recursive", "remove directories recursively");
    *force     = arg_lit0("f", "force",     "ignore nonexistent files, no error");
    *verbose   = arg_lit0("v", "verbose",   "explain what is being done");
    *files     = arg_filen(NULL, NULL, "<file>", 1, 64, "files or directories to remove");
    *end       = arg_end(20);

    static void *argtable[7];
    argtable[0] = *help;
    argtable[1] = *recursive;
    argtable[2] = *force;
    argtable[3] = *verbose;
    argtable[4] = *files;
    argtable[5] = *end;
    argtable[6] = NULL;
    *argtable_out = argtable;
}

/* ── helpers ───────────────────────────────────────────────────────────── */

static int remove_recursive(const char *path, int force, int verbose)
{
    struct stat st;
    if (lstat(path, &st) != 0)
    {
        if (force && errno == ENOENT) return 0;
        perror(path);
        return 1;
    }

    if (S_ISDIR(st.st_mode))
    {
        DIR *dir = opendir(path);
        if (dir == NULL) { perror(path); return 1; }

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL)
        {
            if (strcmp(entry->d_name, ".") == 0 ||
                strcmp(entry->d_name, "..") == 0)
            {
                continue;
            }
            char child[PATH_MAX];
            snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
            if (remove_recursive(child, force, verbose) != 0)
            {
                closedir(dir);
                return 1;
            }
        }
        closedir(dir);

        if (rmdir(path) != 0) { perror(path); return 1; }
        if (verbose) printf("removed directory '%s'\n", path);
    }
    else
    {
        if (unlink(path) != 0)
        {
            if (force && errno == ENOENT) return 0;
            perror(path);
            return 1;
        }
        if (verbose) printf("removed '%s'\n", path);
    }
    return 0;
}

/* ── run ───────────────────────────────────────────────────────────────── */

int rm_run(int argc, char **argv)
{
    struct arg_lit  *help;
    struct arg_lit  *recursive;
    struct arg_lit  *force;
    struct arg_lit  *verbose;
    struct arg_file *files;
    struct arg_end  *end;
    void           **argtable;

    build_rm_argtable(&help, &recursive, &force, &verbose, &files, &end,
                      &argtable);

    // Parse command-line arguments and handle help/errors
    int nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0)
    {
        arg_freetable(argtable, 6);
        rm_print_usage(stdout);
        return 0;
    }
    if (nerrors > 0)
    {
        arg_print_errors(stdout, end, "rm");
        arg_freetable(argtable, 6);
        rm_print_usage(stdout);
        return 1;
    }

    int ret = 0;
    for (int i = 0; i < files->count; i++)
    {
        if (recursive->count > 0)
        {
            if (remove_recursive(files->filename[i], force->count,
                                 verbose->count) != 0)
            {
                ret = 1;
            }
        }
        else
        {
            if (unlink(files->filename[i]) != 0)
            {
                if (force->count && errno == ENOENT) continue;
                perror(files->filename[i]);
                ret = 1;
            }
            else if (verbose->count)
            {
                printf("removed '%s'\n", files->filename[i]);
            }
        }
    }

    arg_freetable(argtable, 6);
    return ret;
}

/* ── print_usage ───────────────────────────────────────────────────────── */

void rm_print_usage(FILE *out)
{
    struct arg_lit  *help;
    struct arg_lit  *recursive;
    struct arg_lit  *force;
    struct arg_lit  *verbose;
    struct arg_file *files;
    struct arg_end  *end;
    void           **argtable;

    build_rm_argtable(&help, &recursive, &force, &verbose, &files, &end,
                      &argtable);

    fprintf(out, "\nUsage: rm ");
    arg_print_syntax(out, argtable, "\n");
    fprintf(out, "\nRemove files or directories.\n");
    fprintf(out, "\nOptions:\n");
    arg_print_glossary(out, argtable, "  %-22s %s\n");
    fprintf(out, "\n");

    arg_freetable(argtable, 6);
}

/* ── spec + registration ───────────────────────────────────────────────── */

cmd_spec_t cmd_rm_spec =
{
    .name        = "rm",
    .summary     = "remove files or directories",
    .long_help   = "Remove one or more files. Use -r to remove directories "
                   "recursively.",
    .run         = rm_run,
    .print_usage = rm_print_usage,
};

void register_rm_command(void)
{
    register_command(&cmd_rm_spec);
}
