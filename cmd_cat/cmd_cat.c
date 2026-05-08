#include <stdio.h>
#include "cmd_cat.h"
#include "cmd_registry.h"
#include "argtable3.h"

#define CAT_BUF 4096

/* ── argtable builder ──────────────────────────────────────────────────── */

static void build_cat_argtable(struct arg_lit  **help,
                                struct arg_lit  **help_json,
                                struct arg_lit  **json,
                                struct arg_lit  **number,
                                struct arg_file **files,
                                struct arg_end  **end,
                                void           ***argtable_out)
{
    *help      = arg_lit0("h", "help",      "show this help and exit");
    *help_json = arg_lit0(NULL, "help-json", "print argument schema as JSON and exit");
    *json      = arg_lit0(NULL, "json",       "output file content as JSON");
    *number    = arg_lit0("n", "number",     "number all output lines");
    *files     = arg_filen(NULL, NULL, "<file>", 1, 64, "files to concatenate");
    *end       = arg_end(20);

    static void *argtable[7];
    argtable[0] = *help;
    argtable[1] = *help_json;
    argtable[2] = *json;
    argtable[3] = *number;
    argtable[4] = *files;
    argtable[5] = *end;
    argtable[6] = NULL;
    *argtable_out = argtable;
}

/* Emit file contents to stdout as a JSON string (inline char-by-char escaping) */
static void cat_json_content(FILE *fp)
{
    putchar('"');
    int ch;
    while ((ch = fgetc(fp)) != EOF)
    {
        unsigned char c = (unsigned char)ch;
        if      (c == '"')  fputs("\\\"", stdout);
        else if (c == '\\') fputs("\\\\", stdout);
        else if (c == '\n') fputs("\\n",  stdout);
        else if (c == '\r') fputs("\\r",  stdout);
        else if (c == '\t') fputs("\\t",  stdout);
        else if (c < 0x20)  printf("\\u%04x", c);
        else                putchar(c);
    }
    putchar('"');
}

/* ── run ───────────────────────────────────────────────────────────────── */

int cat_run(int argc, char **argv)
{
    struct arg_lit  *help;
    struct arg_lit  *help_json;
    struct arg_lit  *json;
    struct arg_lit  *number;
    struct arg_file *files;
    struct arg_end  *end;
    void           **argtable;

    build_cat_argtable(&help, &help_json, &json, &number, &files, &end, &argtable);

    int nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0)
    {
        arg_freetable(argtable, 6);
        cat_print_usage(stdout);
        return 0;
    }
    if (help_json->count > 0)
    {
        cmd_print_help_json(stdout, "cat",
                            "concatenate files and print to stdout",
                            "Concatenate one or more files and write them to standard output. "
                            "Use -n to prefix each output line with its sequential line number.",
                            argtable);
        arg_freetable(argtable, 6);
        return 0;
    }
    if (nerrors > 0)
    {
        arg_print_errors(stdout, end, "cat");
        arg_freetable(argtable, 6);
        cat_print_usage(stdout);
        return 1;
    }

    int ret = 0;

    if (json->count > 0)
    {
        printf("{\n  \"files\": [");
        int first_file = 1;
        for (int i = 0; i < files->count; i++)
        {
            FILE *fp = fopen(files->filename[i], "r");
            if (fp == NULL) { perror(files->filename[i]); ret = 1; continue; }
            if (!first_file) printf(",");
            printf("\n    {\n      \"path\": ");
            cmd_json_str(stdout, files->filename[i]);
            printf(",\n      \"content\": ");
            cat_json_content(fp);
            printf("\n    }");
            fclose(fp);
            first_file = 0;
        }
        printf("\n  ]\n}\n");
        arg_freetable(argtable, 6);
        return ret;
    }

    long line_num = 1;
    for (int i = 0; i < files->count; i++)
    {
        FILE *fp = fopen(files->filename[i], "r");
        if (fp == NULL)
        {
            perror(files->filename[i]);
            ret = 1;
            continue;
        }
        char buf[CAT_BUF];
        while (fgets(buf, sizeof(buf), fp) != NULL)
        {
            if (number->count > 0)
                printf("%6ld\t%s", line_num++, buf);
            else
                fputs(buf, stdout);
        }
        fclose(fp);
    }

    arg_freetable(argtable, 6);
    return ret;
}

/* ── print_usage ───────────────────────────────────────────────────────── */

/*
 * cat_print_usage - Print usage/help information for the cat command.
 *
 * Prints the command-line syntax, a brief description, and a formatted list of options
 * for the cat command to the specified output stream. This function builds the argument
 * table, uses argtable3 utilities to print syntax and glossary, and then frees resources.
 *
 * Input:
 *   out - Output stream (e.g., stdout or stderr) to which the usage information is printed.
 */
void cat_print_usage(FILE *out)
{
    struct arg_lit  *help;
    struct arg_lit  *help_json;
    struct arg_lit  *json;
    struct arg_lit  *number;
    struct arg_file *files;
    struct arg_end  *end;
    void           **argtable;

    build_cat_argtable(&help, &help_json, &json, &number, &files, &end, &argtable);

    fprintf(out, "\nUsage: cat ");
    arg_print_syntax(out, argtable, "\n");
    fprintf(out, "\nConcatenate files and print to standard output.\n");
    fprintf(out, "With -n, each output line is prefixed with its sequential line number.\n");
    fprintf(out, "\nOptions:\n");
    arg_print_glossary(out, argtable, "  %-22s %s\n");
    fprintf(out, "\nExamples:\n");
    fprintf(out, "  cat file.txt\n");
    fprintf(out, "  cat -n file.txt\n");
    fprintf(out, "\n");

    arg_freetable(argtable, 6);
}

/* ── spec + registration ───────────────────────────────────────────────── */

cmd_spec_t cmd_cat_spec =
{
    .name        = "cat",
    .summary     = "concatenate files and print to stdout",
    .long_help   = "Concatenate one or more files and write them to standard output. "
                   "Use -n to prefix each output line with its sequential line number.",
    .run         = cat_run,
    .print_usage = cat_print_usage,
};

void register_cat_command(void)
{
    register_command(&cmd_cat_spec);
}
