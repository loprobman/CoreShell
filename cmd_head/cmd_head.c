#include <stdio.h>
#include "cmd_head.h"
#include "cmd_registry.h"
#include "argtable3.h"

#define HEAD_BUF 4096

/* ── argtable builder ──────────────────────────────────────────────────── */

static void build_head_argtable(struct arg_lit  **help,
                                 struct arg_int  **lines,
                                 struct arg_file **file,
                                 struct arg_end  **end,
                                 void           ***argtable_out)
{
    *help  = arg_lit0("h", "help",  "show this help and exit");
    *lines = arg_int0("n", "lines", "<N>",
                      "number of lines to print (default: 10)");
    *file  = arg_file1(NULL, NULL, "<file>", "input file");
    *end   = arg_end(20);

    static void *argtable[5];
    argtable[0] = *help;
    argtable[1] = *lines;
    argtable[2] = *file;
    argtable[3] = *end;
    argtable[4] = NULL;
    *argtable_out = argtable;
}

/* ── run ───────────────────────────────────────────────────────────────── */

/*
 * head_run - Print the first N lines of a file.
 *
 * Implements the 'head' command. Parses options for the number of lines
 * and the input file, then prints the specified number of lines from the file.
 *
 * Input:
 *   argc - Number of command-line arguments.
 *   argv - Array of argument strings.
 *
 * Output:
 *   Returns 0 on success, nonzero on error.
 */
int head_run(int argc, char **argv)
{
    struct arg_lit  *help;
    struct arg_int  *lines;
    struct arg_file *file;
    struct arg_end  *end;
    void           **argtable;

    build_head_argtable(&help, &lines, &file, &end, &argtable);

    // Parse command-line arguments and handle help/errors
    int nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0)
    {
        arg_freetable(argtable, 4);
        head_print_usage(stdout);
        return 0;
    }
    if (nerrors > 0)
    {
        arg_print_errors(stdout, end, "head");
        arg_freetable(argtable, 4);
        head_print_usage(stdout);
        return 1;
    }

    // Determine number of lines to print (default: 10)
    int n = (lines->count > 0) ? lines->ival[0] : 10;

    FILE *fp = fopen(file->filename[0], "r");
    if (fp == NULL)
    {
        perror("head");
        arg_freetable(argtable, 4);
        return 1;
    }

    char buf[HEAD_BUF];
    int count = 0;
    while (count < n && fgets(buf, sizeof(buf), fp) != NULL)
    {
        fputs(buf, stdout);
        count++;
    }
    fclose(fp);

    arg_freetable(argtable, 4);
    return 0;
}

/* ── print_usage ───────────────────────────────────────────────────────── */

void head_print_usage(FILE *out)
{
    struct arg_lit  *help;
    struct arg_int  *lines;
    struct arg_file *file;
    struct arg_end  *end;
    void           **argtable;

    build_head_argtable(&help, &lines, &file, &end, &argtable);

    fprintf(out, "\nUsage: head ");
    arg_print_syntax(out, argtable, "\n");
    fprintf(out, "\nPrint the first N lines of a file.\n");
    fprintf(out, "\nOptions:\n");
    arg_print_glossary(out, argtable, "  %-22s %s\n");
    fprintf(out, "\n");

    arg_freetable(argtable, 4);
}

/* ── spec + registration ───────────────────────────────────────────────── */

cmd_spec_t cmd_head_spec =
{
    .name        = "head",
    .summary     = "print the first lines of a file",
    .long_help   = "Output the first N lines (default: 10) of the given file.",
    .run         = head_run,
    .print_usage = head_print_usage,
};

void register_head_command(void)
{
    register_command(&cmd_head_spec);
}
