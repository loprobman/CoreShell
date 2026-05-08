#include <stdio.h>
#include "cmd_cat.h"
#include "cmd_registry.h"
#include "argtable3.h"

#define CAT_BUF 4096

/* ── argtable builder ──────────────────────────────────────────────────── */

static void build_cat_argtable(struct arg_lit  **help,
                                struct arg_lit  **number,
                                struct arg_file **files,
                                struct arg_end  **end,
                                void           ***argtable_out)
{
    *help   = arg_lit0("h", "help",   "show this help and exit");
    *number = arg_lit0("n", "number", "number all output lines");
    *files  = arg_filen(NULL, NULL, "<file>", 1, 64, "files to concatenate");
    /* *end specifies the maximum number of error messages that can be stored or reported 
       during argument parsing.*/
    *end    = arg_end(20); 

    static void *argtable[5];
    argtable[0] = *help;
    argtable[1] = *number;
    argtable[2] = *files;
    argtable[3] = *end;
    argtable[4] = NULL;
    *argtable_out = argtable;
}

/* ── run ───────────────────────────────────────────────────────────────── */

int cat_run(int argc, char **argv)
{
    struct arg_lit  *help;
    struct arg_lit  *number;
    struct arg_file *files;
    struct arg_end  *end;
    void           **argtable;

    build_cat_argtable(&help, &number, &files, &end, &argtable);

    /* process the command-line arguments provided to the program. 
       Analize the arguments according to definitions in argtable 
       check for errors (missing argumuments/invalid options) and 
       return them nerrors */
    int nerrors = arg_parse(argc, argv, argtable);

    /* If the user passed -h/--help, free the argtable, print usage, and exit cleanly. */
    if (help->count > 0)
    {
        arg_freetable(argtable, 4);
        cat_print_usage(stdout);
        return 0;
    }

    /* If argument parsing produced errors, print those errors followed by usage,
       free the argtable, and exit with a failure code. */
    if (nerrors > 0)
    {
        arg_print_errors(stdout, end, "cat");
        arg_freetable(argtable, 4);
        cat_print_usage(stdout);
        return 1;
    }

    int ret = 0;
    long line_num = 1;

    for (int i = 0; i < files->count; i++)
    {
        /* Open a file (by invoking the open system call) and associates 
            it with a high level I/O stream */
        FILE *fp = fopen(files->filename[i], "r");
        /* Return NULL if fails */
        if (fp == NULL)
        {   /* Filename: errno */
            perror(files->filename[i]);
            ret = 1;
            continue;
        }

        char buf[CAT_BUF];
        /* Read a string from an input stream in buf, with a max number of characters sizeof(buf)-1
           including (\0), fp is the stream, fgets stops until encountering a new line character \n
           Buffer Limit or EOF */
        while (fgets(buf, sizeof(buf), fp) != NULL)
        {
            if (number->count > 0)
            {
                printf("%6ld\t%s", line_num++, buf);
            }
            else
            {
                /* writes a null-terminated string to a specified file stream without the null 
                   terminator (\0) */
                fputs(buf, stdout);
            }
        }
        /*  close a file stream that was previously opened with fopen */
        fclose(fp);
    }

    arg_freetable(argtable, 4);
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
    struct arg_lit  *number;
    struct arg_file *files;
    struct arg_end  *end;
    void           **argtable;

    build_cat_argtable(&help, &number, &files, &end, &argtable);

    fprintf(out, "\nUsage: cat ");
    arg_print_syntax(out, argtable, "\n");
    fprintf(out, "\nConcatenate files and print to standard output.\n");
    fprintf(out, "\nOptions:\n");
    arg_print_glossary(out, argtable, "  %-22s %s\n");
    fprintf(out, "\n");

    arg_freetable(argtable, 4);
}

/* ── spec + registration ───────────────────────────────────────────────── */

cmd_spec_t cmd_cat_spec =
{
    .name        = "cat",
    .summary     = "concatenate files and print to stdout",
    .long_help   = "Concatenate one or more files and write them to standard output.",
    .run         = cat_run,
    .print_usage = cat_print_usage,
};

void register_cat_command(void)
{
    register_command(&cmd_cat_spec);
}
