#include <stdio.h>
#include <string.h>
#include "cmd_echo.h"
#include "cmd_registry.h"
#include "argtable3.h"

/* ── argtable builder ──────────────────────────────────────────────────── */

static void build_echo_argtable(struct arg_lit **help,
                                struct arg_lit **no_newline,
                                struct arg_lit **escape,
                                struct arg_str **text,
                                struct arg_end **end,
                                void          ***argtable_out)
{
    *help       = arg_lit0("h", "help",       "show this help and exit");
    *no_newline = arg_lit0("n", "no-newline", "do not output trailing newline");
    *escape     = arg_lit0("e", "escape",     "enable backslash escape interpretation");
    *text       = arg_strn(NULL, NULL, "[string]", 0, 64,
                           "strings to print");
    *end        = arg_end(20);

    static void *argtable[6];
    argtable[0] = *help;
    argtable[1] = *no_newline;
    argtable[2] = *escape;
    argtable[3] = *text;
    argtable[4] = *end;
    argtable[5] = NULL;
    *argtable_out = argtable;
}

/* ── helpers ───────────────────────────────────────────────────────────── */

/* Print a string interpreting backslash escape sequences */
static void print_escaped(const char *s)
{
    while (*s)
    {
        if (*s == '\\' && *(s + 1))
        {
            s++;
            switch (*s)
            {
                case 'n':  putchar('\n'); break;
                case 't':  putchar('\t'); break;
                case 'r':  putchar('\r'); break;
                case '\\': putchar('\\'); break;
                case 'a':  putchar('\a'); break;
                case 'b':  putchar('\b'); break;
                case 'v':  putchar('\v'); break;
                case 'f':  putchar('\f'); break;
                default:   putchar('\\'); putchar(*s); break;
            }
        }
        else
        {
            putchar(*s);
        }
        s++;
    }
}

/* ── run ───────────────────────────────────────────────────────────────── */

int echo_run(int argc, char **argv)
{
    struct arg_lit *help;
    struct arg_lit *no_newline;
    struct arg_lit *escape;
    struct arg_str *text;
    struct arg_end *end;
    void          **argtable;

    build_echo_argtable(&help, &no_newline, &escape, &text, &end, &argtable);

    int nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0)
    {
        arg_freetable(argtable, 5);
        echo_print_usage(stdout);
        return 0;
    }
    if (nerrors > 0)
    {
        arg_print_errors(stdout, end, "echo");
        arg_freetable(argtable, 5);
        echo_print_usage(stdout);
        return 1;
    }

    for (int i = 0; i < text->count; i++)
    {
        if (i > 0)
        {
            putchar(' ');
        }
        if (escape->count > 0)
        {
            print_escaped(text->sval[i]);
        }
        else
            /*
             * echo_run - Print arguments to standard output.
             *
             * Implements the 'echo' command. Handles options for omitting the trailing newline
             * and interpreting backslash escapes, then prints the provided strings.
             *
             * Input:
             *   argc - Number of command-line arguments.
             *   argv - Array of argument strings.
             *
             * Output:
             *   Returns 0 on success, nonzero on error.
             */
        {
            fputs(text->sval[i], stdout);
        }
    }

    if (no_newline->count == 0)
    {
        putchar('\n');
    }

    arg_freetable(argtable, 5);
    return 0;
}

/* ── print_usage ───────────────────────────────────────────────────────── */

void echo_print_usage(FILE *out)
{
    struct arg_lit *help;
    struct arg_lit *no_newline;
    struct arg_lit *escape;
    struct arg_str *text;
    struct arg_end *end;
    void          **argtable;

    build_echo_argtable(&help, &no_newline, &escape, &text, &end, &argtable);

    fprintf(out, "\nUsage: echo ");
    arg_print_syntax(out, argtable, "\n");
    fprintf(out, "\nPrint arguments to standard output.\n");
    fprintf(out, "\nOptions:\n");
    arg_print_glossary(out, argtable, "  %-22s %s\n");
    fprintf(out, "\n");

    arg_freetable(argtable, 5);
}

cmd_spec_t cmd_echo_spec = {
    .name        = "echo",
    .summary     = "print arguments to standard output",
    .long_help   = "Echo the given string arguments to stdout, separated by spaces. "
                   "Supports -n and -e flags similar to GNU echo.",
    .run         = echo_run,
    .print_usage = echo_print_usage,
};

void register_echo_command(void)
{
    register_command(&cmd_echo_spec);
}
