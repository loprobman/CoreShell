#include <stdio.h>
#include <string.h>
#include "cmd_echo.h"
#include "cmd_registry.h"
#include "argtable3.h"

/* ── argtable builder ──────────────────────────────────────────────────── */

static void build_echo_argtable(struct arg_lit **help,
                                struct arg_lit **help_json,
                                struct arg_lit **json,
                                struct arg_lit **no_newline,
                                struct arg_lit **escape,
                                struct arg_str **text,
                                struct arg_end **end,
                                void          ***argtable_out)
{
    *help       = arg_lit0("h", "help",       "show this help and exit");
    *help_json  = arg_lit0(NULL, "help-json",  "print argument schema as JSON and exit");
    *json       = arg_lit0(NULL, "json",        "output result as JSON");
    *no_newline = arg_lit0("n", "no-newline",  "do not output trailing newline");
    *escape     = arg_lit0("e", "escape",       "enable backslash escape interpretation");
    *text       = arg_strn(NULL, NULL, "[string]", 0, 64,
                           "strings to print");
    *end        = arg_end(20);

    static void *argtable[8];
    argtable[0] = *help;
    argtable[1] = *help_json;
    argtable[2] = *json;
    argtable[3] = *no_newline;
    argtable[4] = *escape;
    argtable[5] = *text;
    argtable[6] = *end;
    argtable[7] = NULL;
    *argtable_out = argtable;
}

/* ── helpers ───────────────────────────────────────────────────────────── */

/* Build escaped output into buf (size bufsz); returns bytes written */
static size_t echo_build_output(const struct arg_str *text, int do_escape,
                                char *buf, size_t bufsz)
{
    size_t pos = 0;
    for (int i = 0; i < text->count; i++)
    {
        if (i > 0 && pos < bufsz - 1)
            buf[pos++] = ' ';
        const char *s = text->sval[i];
        if (do_escape)
        {
            while (*s && pos < bufsz - 2)
            {
                if (*s == '\\' && *(s + 1))
                {
                    s++;
                    switch (*s)
                    {
                        case 'n':  buf[pos++] = '\n'; break;
                        case 't':  buf[pos++] = '\t'; break;
                        case 'r':  buf[pos++] = '\r'; break;
                        case '\\': buf[pos++] = '\\'; break;
                        case 'a':  buf[pos++] = '\a'; break;
                        case 'b':  buf[pos++] = '\b'; break;
                        case 'v':  buf[pos++] = '\v'; break;
                        case 'f':  buf[pos++] = '\f'; break;
                        default:   buf[pos++] = '\\'; if (pos < bufsz-1) buf[pos++] = *s; break;
                    }
                }
                else
                {
                    buf[pos++] = *s;
                }
                s++;
            }
        }
        else
        {
            size_t l = strlen(s);
            if (l > bufsz - pos - 1) l = bufsz - pos - 1;
            memcpy(buf + pos, s, l);
            pos += l;
        }
    }
    buf[pos] = '\0';
    return pos;
}

/* ── run ───────────────────────────────────────────────────────────────── */

int echo_run(int argc, char **argv)
{
    struct arg_lit *help;
    struct arg_lit *help_json;
    struct arg_lit *json;
    struct arg_lit *no_newline;
    struct arg_lit *escape;
    struct arg_str *text;
    struct arg_end *end;
    void          **argtable;

    build_echo_argtable(&help, &help_json, &json, &no_newline, &escape, &text, &end, &argtable);

    int nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0)
    {
        arg_freetable(argtable, 7);
        echo_print_usage(stdout);
        return 0;
    }
    if (help_json->count > 0)
    {
        cmd_print_help_json(stdout, "echo",
                            "print arguments to standard output",
                            "Echo the given string arguments to stdout, separated by spaces."
                            " Supports -n and -e flags similar to GNU echo.",
                            argtable);
        arg_freetable(argtable, 7);
        return 0;
    }
    if (nerrors > 0)
    {
        arg_print_errors(stdout, end, "echo");
        arg_freetable(argtable, 7);
        echo_print_usage(stdout);
        return 1;
    }

    char buf[65536];
    echo_build_output(text, escape->count, buf, sizeof(buf));

    if (json->count > 0)
    {
        printf("{\n  \"output\": ");
        cmd_json_str(stdout, buf);
        printf("\n}\n");
        arg_freetable(argtable, 7);
        return 0;
    }

    fputs(buf, stdout);
    if (no_newline->count == 0)
        putchar('\n');

    arg_freetable(argtable, 7);
    return 0;
}

/* ── print_usage ───────────────────────────────────────────────────────── */

void echo_print_usage(FILE *out)
{
    struct arg_lit *help;
    struct arg_lit *help_json;
    struct arg_lit *json;
    struct arg_lit *no_newline;
    struct arg_lit *escape;
    struct arg_str *text;
    struct arg_end *end;
    void          **argtable;

    build_echo_argtable(&help, &help_json, &json, &no_newline, &escape, &text, &end, &argtable);

    fprintf(out, "\nUsage: echo ");
    arg_print_syntax(out, argtable, "\n");
    fprintf(out, "\nPrint arguments to standard output, separated by spaces.\n");
    fprintf(out, "With -e, backslash sequences are interpreted: \\n (newline), \\t (tab), \\\\ etc.\n");
    fprintf(out, "With -n, the trailing newline is suppressed.\n");
    fprintf(out, "\nOptions:\n");
    arg_print_glossary(out, argtable, "  %-22s %s\n");
    fprintf(out, "\nExamples:\n");
    fprintf(out, "  echo Hello World\n");
    fprintf(out, "  echo -e \"line1\\nline2\"\n");
    fprintf(out, "  echo -n no-newline\n");
    fprintf(out, "\n");

    arg_freetable(argtable, 7);
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
