#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include "cmd_stat.h"
#include "cmd_registry.h"
#include "argtable3.h"

/* ── argtable builder ──────────────────────────────────────────────────── */

static void build_stat_argtable(struct arg_lit  **help,
                                 struct arg_lit  **help_json,
                                 struct arg_lit  **json,
                                 struct arg_file **files,
                                 struct arg_end  **end,
                                 void           ***argtable_out)
{
    *help      = arg_lit0("h", "help",      "show this help and exit");
    *help_json = arg_lit0(NULL, "help-json", "print argument schema as JSON and exit");
    *json      = arg_lit0(NULL, "json",       "output file metadata as JSON");
    *files     = arg_filen(NULL, NULL, "<file>", 1, 64, "files to stat");
    *end       = arg_end(20);

    static void *argtable[6];
    argtable[0] = *help;
    argtable[1] = *help_json;
    argtable[2] = *json;
    argtable[3] = *files;
    argtable[4] = *end;
    argtable[5] = NULL;
    *argtable_out = argtable;
}

/* Emit stat data for path as a JSON object (caller writes surrounding context) */
static int print_stat_json(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) { perror(path); return 1; }

    char perm[11];
    perm[0]  = S_ISDIR(st.st_mode) ? 'd' : S_ISLNK(st.st_mode) ? 'l' : '-';
    perm[1]  = (st.st_mode & S_IRUSR) ? 'r' : '-';
    perm[2]  = (st.st_mode & S_IWUSR) ? 'w' : '-';
    perm[3]  = (st.st_mode & S_IXUSR) ? 'x' : '-';
    perm[4]  = (st.st_mode & S_IRGRP) ? 'r' : '-';
    perm[5]  = (st.st_mode & S_IWGRP) ? 'w' : '-';
    perm[6]  = (st.st_mode & S_IXGRP) ? 'x' : '-';
    perm[7]  = (st.st_mode & S_IROTH) ? 'r' : '-';
    perm[8]  = (st.st_mode & S_IWOTH) ? 'w' : '-';
    perm[9]  = (st.st_mode & S_IXOTH) ? 'x' : '-';
    perm[10] = '\0';

    char atime_buf[64], mtime_buf[64], ctime_buf[64];
    /* ctime_r would be safer, but ctime gives a newline — strip it */
    char *t;
    t = ctime(&st.st_atime); strncpy(atime_buf, t, sizeof(atime_buf)-1);
    atime_buf[strcspn(atime_buf, "\n")] = '\0';
    t = ctime(&st.st_mtime); strncpy(mtime_buf, t, sizeof(mtime_buf)-1);
    mtime_buf[strcspn(mtime_buf, "\n")] = '\0';
    t = ctime(&st.st_ctime); strncpy(ctime_buf, t, sizeof(ctime_buf)-1);
    ctime_buf[strcspn(ctime_buf, "\n")] = '\0';

    printf("    {\n");
    printf("      \"path\": ");        cmd_json_str(stdout, path);         printf(",\n");
    printf("      \"size\": %ld,\n",   (long)st.st_size);
    printf("      \"mode\": \"%04o\",\n", st.st_mode & 07777);
    printf("      \"permissions\": "); cmd_json_str(stdout, perm);         printf(",\n");
    printf("      \"links\": %ld,\n",  (long)st.st_nlink);
    printf("      \"uid\": %d,\n",     st.st_uid);
    printf("      \"gid\": %d,\n",     st.st_gid);
    printf("      \"atime\": ");       cmd_json_str(stdout, atime_buf);    printf(",\n");
    printf("      \"mtime\": ");       cmd_json_str(stdout, mtime_buf);    printf(",\n");
    printf("      \"ctime\": ");       cmd_json_str(stdout, ctime_buf);    printf("\n");
    printf("    }");
    return 0;
}

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
    struct arg_lit  *help_json;
    struct arg_lit  *json;
    struct arg_file *files;
    struct arg_end  *end;
    void           **argtable;

    build_stat_argtable(&help, &help_json, &json, &files, &end, &argtable);

    int nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0)
    {
        arg_freetable(argtable, 5);
        stat_print_usage(stdout);
        return 0;
    }
    if (help_json->count > 0)
    {
        cmd_print_help_json(stdout, "stat",
                            "display file status",
                            "Display status information for one or more files: size, permissions, "
                            "link count, owner UID/GID, and access, modify, and change timestamps.",
                            argtable);
        arg_freetable(argtable, 5);
        return 0;
    }
    if (nerrors > 0)
    {
        arg_print_errors(stdout, end, "stat");
        arg_freetable(argtable, 5);
        stat_print_usage(stdout);
        return 1;
    }

    int ret = 0;

    if (json->count > 0)
    {
        printf("{\n  \"files\": [");
        int first = 1;
        for (int i = 0; i < files->count; i++)
        {
            if (!first) printf(",");
            printf("\n");
            if (print_stat_json(files->filename[i]) != 0) ret = 1;
            first = 0;
        }
        printf("\n  ]\n}\n");
        arg_freetable(argtable, 5);
        return ret;
    }

    for (int i = 0; i < files->count; i++)
    {
        if (print_stat(files->filename[i]) != 0)
            ret = 1;
        if (i < files->count - 1 && ret == 0)
            printf("\n");
    }

    arg_freetable(argtable, 5);
    return ret;
}

/* ── print_usage ───────────────────────────────────────────────────────── */

void stat_print_usage(FILE *out)
{
    struct arg_lit  *help;
    struct arg_lit  *help_json;
    struct arg_lit  *json;
    struct arg_file *files;
    struct arg_end  *end;
    void           **argtable;

    build_stat_argtable(&help, &help_json, &json, &files, &end, &argtable);

    fprintf(out, "\nUsage: stat ");
    arg_print_syntax(out, argtable, "\n");
    fprintf(out, "\nDisplay file status: size, permissions, link count, owner UID/GID,\n");
    fprintf(out, "and access, modification, and change timestamps.\n");
    fprintf(out, "\nOptions:\n");
    arg_print_glossary(out, argtable, "  %-22s %s\n");
    fprintf(out, "\nExamples:\n");
    fprintf(out, "  stat file.txt\n");
    fprintf(out, "  stat /etc/hosts\n");
    fprintf(out, "\n");

    arg_freetable(argtable, 5);
}

/* ── spec + registration ───────────────────────────────────────────────── */

cmd_spec_t cmd_stat_spec =
{
    .name        = "stat",
    .summary     = "display file status",
    .long_help   = "Display status information for one or more files: size, permissions, "
                   "link count, owner UID/GID, and access, modify, and change timestamps.",
    .run         = stat_run,
    .print_usage = stat_print_usage,
};

void register_stat_command(void)
{
    register_command(&cmd_stat_spec);
}
