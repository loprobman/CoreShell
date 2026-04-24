#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <errno.h>
#include "cmd_ls.h"
#include "cmd_registry.h"
#include "argtable3.h"

/* ── argtable builder ──────────────────────────────────────────────────── */

static void build_ls_argtable(struct arg_lit  **help,
                               struct arg_lit  **all,
                               struct arg_lit  **long_fmt,
                               struct arg_file **paths,
                               struct arg_end  **end,
                               void           ***argtable_out)
{
    *help     = arg_lit0("h", "help",  "show this help and exit");
    *all      = arg_lit0("a", "all",   "include hidden entries (starting with .)");
    *long_fmt = arg_lit0("l", "long",  "use long listing format");
    *paths    = arg_file0(NULL, NULL, "[path]",
                          "directory to list (default: .)");
    *end      = arg_end(20);

    static void *argtable[6];
    argtable[0] = *help;
    argtable[1] = *all;
    argtable[2] = *long_fmt;
    argtable[3] = *paths;
    argtable[4] = *end;
    argtable[5] = NULL;
    *argtable_out = argtable;
}

/* ── helpers ───────────────────────────────────────────────────────────── */

/* Build a 10-char permission string from a mode_t */
static void format_mode(mode_t mode, char *buf)
{
    buf[0] = S_ISDIR(mode)  ? 'd'
           : S_ISLNK(mode)  ? 'l'
           : S_ISCHR(mode)  ? 'c'
           : S_ISBLK(mode)  ? 'b'
           : S_ISFIFO(mode) ? 'p'
           : S_ISSOCK(mode) ? 's'
           : '-';
    buf[1] = (mode & S_IRUSR) ? 'r' : '-';
    buf[2] = (mode & S_IWUSR) ? 'w' : '-';
    buf[3] = (mode & S_IXUSR) ? 'x' : '-';
    buf[4] = (mode & S_IRGRP) ? 'r' : '-';
    buf[5] = (mode & S_IWGRP) ? 'w' : '-';
    buf[6] = (mode & S_IXGRP) ? 'x' : '-';
    buf[7] = (mode & S_IROTH) ? 'r' : '-';
    buf[8] = (mode & S_IWOTH) ? 'w' : '-';
    buf[9] = (mode & S_IXOTH) ? 'x' : '-';
    buf[10] = '\0';
}

/* Print one long-format line for a file */
static void print_long(const char *dir_path, const char *name)
{
    char full[4096];
    if (strcmp(dir_path, ".") == 0)
    {
        snprintf(full, sizeof(full), "%s", name);
    }
    else
    {
        snprintf(full, sizeof(full), "%s/%s", dir_path, name);
    }

    struct stat st;
    if (lstat(full, &st) != 0)
    {
        perror(full);
        return;
    }

    char mode_str[11];
    format_mode(st.st_mode, mode_str);

    /* Owner and group names, with numeric fallback */
    struct passwd *pw = getpwuid(st.st_uid);
    struct group  *gr = getgrgid(st.st_gid);
    char owner[32], group_name[32];
    if (pw)
    {
        snprintf(owner, sizeof(owner), "%s", pw->pw_name);
    }
    else
    {
        snprintf(owner, sizeof(owner), "%d", st.st_uid);
    }
    if (gr)
    {
        snprintf(group_name, sizeof(group_name), "%s", gr->gr_name);
    }
    else
    {
        snprintf(group_name, sizeof(group_name), "%d", st.st_gid);
    }

    /* Modification time */
    char time_buf[32];
    struct tm *tm_info = localtime(&st.st_mtime);
    strftime(time_buf, sizeof(time_buf), "%b %e %H:%M", tm_info);

    printf("%s %3ld %-8s %-8s %8ld %s %s\n",
           mode_str,
           (long)st.st_nlink,
           owner,
           group_name,
           (long)st.st_size,
           time_buf,
           name);
}

/* ── run ───────────────────────────────────────────────────────────────── */

/*
 * ls_run - List directory contents.
 *
 * Implements the 'ls' command. Handles options for showing hidden files,
 * long format, and custom paths. Lists files and directories according to options.
 *
 * Input:
 *   argc - Number of command-line arguments.
 *   argv - Array of argument strings.
 *
 * Output:
 *   Returns 0 on success, nonzero on error.
 */
int ls_run(int argc, char **argv)
{
    struct arg_lit  *help;
    struct arg_lit  *all;
    struct arg_lit  *long_fmt;
    struct arg_file *paths;
    struct arg_end  *end;
    void           **argtable;

    build_ls_argtable(&help, &all, &long_fmt, &paths, &end, &argtable);

    // Parse command-line arguments and handle help/errors
    int nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0)
    {
        arg_freetable(argtable, 5);
        ls_print_usage(stdout);
        return 0;
    }
    if (nerrors > 0)
    {
        arg_print_errors(stdout, end, "ls");
        arg_freetable(argtable, 5);
        ls_print_usage(stdout);
        return 1;
    }

    const char *dir_path = (paths->count > 0) ? paths->filename[0] : ".";

    DIR *dir = opendir(dir_path);
    if (dir == NULL)
    {
        perror("ls");
        arg_freetable(argtable, 5);
        return 1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        /* Skip hidden entries unless -a */
        if (entry->d_name[0] == '.' && all->count == 0)
        {
            continue;
        }

        if (long_fmt->count > 0)
        {
            print_long(dir_path, entry->d_name);
        }
        else
        {
            printf("%s  ", entry->d_name);
        }
    }

    if (long_fmt->count == 0)
    {
        printf("\n");
    }

    closedir(dir);
    arg_freetable(argtable, 5);
    return 0;
}

/* ── print_usage ───────────────────────────────────────────────────────── */

void ls_print_usage(FILE *out)
{
    struct arg_lit  *help;
    struct arg_lit  *all;
    struct arg_lit  *long_fmt;
    struct arg_file *paths;
    struct arg_end  *end;
    void           **argtable;

    build_ls_argtable(&help, &all, &long_fmt, &paths, &end, &argtable);

    fprintf(out, "\nUsage: ls ");
    arg_print_syntax(out, argtable, "\n");
    fprintf(out, "\nList directory contents.\n");
    fprintf(out, "\nOptions:\n");
    arg_print_glossary(out, argtable, "  %-22s %s\n");
    fprintf(out, "\n");

    arg_freetable(argtable, 5);
}

/* ── spec + registration ───────────────────────────────────────────────── */

cmd_spec_t cmd_ls_spec =
{
    .name        = "ls",
    .summary     = "list directory contents",
    .long_help   = "List information about entries in the specified directory "
                   "(default: current directory).",
    .run         = ls_run,
    .print_usage = ls_print_usage,
};

void register_ls_command(void)
{
    register_command(&cmd_ls_spec);
}
