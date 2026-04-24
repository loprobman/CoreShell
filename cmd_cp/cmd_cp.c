#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>
#include "cmd_cp.h"
#include "cmd_registry.h"
#include "argtable3.h"

#define CP_BUF 4096

/* ── argtable builder ──────────────────────────────────────────────────── */

static void build_cp_argtable(struct arg_lit  **help,
                               struct arg_lit  **recursive,
                               struct arg_lit  **verbose,
                               struct arg_file **files,
                               struct arg_end  **end,
                               void           ***argtable_out)
{
    *help      = arg_lit0("h", "help",      "show this help and exit");
    *recursive = arg_lit0("r", "recursive", "copy directories recursively");
    *verbose   = arg_lit0("v", "verbose",   "explain what is being done");
    *files     = arg_filen(NULL, NULL, "<src> <dst>", 2, 2,
                           "source file and destination file/directory");
    *end       = arg_end(20);

    static void *argtable[6];
    argtable[0] = *help;
    argtable[1] = *recursive;
    argtable[2] = *verbose;
    argtable[3] = *files;
    argtable[4] = *end;
    argtable[5] = NULL;
    *argtable_out = argtable;
}

/* ── helpers ───────────────────────────────────────────────────────────── */

static int copy_file(const char *src, const char *dst, int verbose)
{
    FILE *fsrc = fopen(src, "rb");
    if (fsrc == NULL) { perror(src); return 1; }

    FILE *fdst = fopen(dst, "wb");
    if (fdst == NULL) { perror(dst); fclose(fsrc); return 1; }

    char buf[CP_BUF];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fsrc)) > 0)
    {
        if (fwrite(buf, 1, n, fdst) != n)
        {
            perror(dst);
            fclose(fsrc);
            fclose(fdst);
            return 1;
        }
    }
    fclose(fsrc);
    fclose(fdst);
    if (verbose) printf("'%s' -> '%s'\n", src, dst);
    return 0;
}

static int copy_recursive(const char *src, const char *dst, int verbose)
{
    struct stat st;
    if (stat(src, &st) != 0) { perror(src); return 1; }

    if (S_ISDIR(st.st_mode))
    {
        if (mkdir(dst, st.st_mode & 0777) != 0 && errno != EEXIST)
        {
            perror(dst);
            return 1;
        }
        if (verbose) printf("mkdir '%s'\n", dst);

        DIR *dir = opendir(src);
        if (dir == NULL) { perror(src); return 1; }

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL)
        {
            if (strcmp(entry->d_name, ".") == 0 ||
                strcmp(entry->d_name, "..") == 0)
            {
                continue;
            }
            char src_child[PATH_MAX], dst_child[PATH_MAX];
            snprintf(src_child, sizeof(src_child), "%s/%s", src, entry->d_name);
            snprintf(dst_child, sizeof(dst_child), "%s/%s", dst, entry->d_name);
            if (copy_recursive(src_child, dst_child, verbose) != 0)
            {
                closedir(dir);
                return 1;
            }
        }
        closedir(dir);
        return 0;
    }

    return copy_file(src, dst, verbose);
}

/* ── run ───────────────────────────────────────────────────────────────── */

int cp_run(int argc, char **argv)
{
    struct arg_lit  *help;
    struct arg_lit  *recursive;
    struct arg_lit  *verbose;
    struct arg_file *files;
    struct arg_end  *end;
    void           **argtable;

    build_cp_argtable(&help, &recursive, &verbose, &files, &end, &argtable);

    int nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0)
    {
        arg_freetable(argtable, 5);
        cp_print_usage(stdout);
        return 0;
    }
    if (nerrors > 0)
    {
        arg_print_errors(stdout, end, "cp");
        arg_freetable(argtable, 5);
        cp_print_usage(stdout);
        return 1;
    }

    const char *src = files->filename[0];
    const char *dst = files->filename[1];
    int ret;

    if (recursive->count > 0)
    {
        ret = copy_recursive(src, dst, verbose->count);
    }
    else
    {
        ret = copy_file(src, dst, verbose->count);
    }

    arg_freetable(argtable, 5);
    return ret;
}

/* ── print_usage ───────────────────────────────────────────────────────── */

void cp_print_usage(FILE *out)
{
    struct arg_lit  *help;
    struct arg_lit  *recursive;
    struct arg_lit  *verbose;
    struct arg_file *files;
    struct arg_end  *end;
    void           **argtable;

    build_cp_argtable(&help, &recursive, &verbose, &files, &end, &argtable);

    fprintf(out, "\nUsage: cp ");
    arg_print_syntax(out, argtable, "\n");
    fprintf(out, "\nCopy SOURCE to DEST.\n");
    fprintf(out, "\nOptions:\n");
    arg_print_glossary(out, argtable, "  %-22s %s\n");
    fprintf(out, "\n");

    arg_freetable(argtable, 5);
}

/* ── spec + registration ───────────────────────────────────────────────── */

cmd_spec_t cmd_cp_spec =
{
    .name        = "cp",
    .summary     = "copy a file or directory",
    .long_help   = "Copy SOURCE to DEST. Use -r to copy directories recursively.",
    .run         = cp_run,
    .print_usage = cp_print_usage,
};

void register_cp_command(void)
{
    register_command(&cmd_cp_spec);
}
