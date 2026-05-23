/*
 * coresh_llm - LLM helper for CoreShell natural language @ commands
 *
 * This is a template/stub implementation demonstrating the helper-side architecture.
 *
 * The helper program:
 * 1. Receives a natural language query as argv[1].
 * 2. Maps it to a concrete shell command (using an LLM API, rules, etc.).
 * 3. Outputs exactly one line: the suggested command.
 *
 * To use:
 *   - Extend this with real LLM integration (OpenAI API, local model, etc.).
 *   - Build: gcc -o coresh_llm coresh_llm.c
 *   - Add to PATH or run from CoreShell's directory.
 *
 * Example usage:
 *   coresh_llm "list all C files sorted by size"
 *   # Output: ls -lS *.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ── Simple rule-based mock LLM ────────────────────────────────────────── */

static void lowercase_copy(const char *src, char *dst, size_t dstsz)
{
    size_t i;

    if (dstsz == 0)
        return;

    for (i = 0; i + 1 < dstsz && src[i] != '\0'; i++)
        dst[i] = (char)tolower((unsigned char)src[i]);

    dst[i] = '\0';
}

/*
 * mock_llm - Map natural language to a shell command (demo implementation).
 *
 * This is a placeholder that demonstrates the interface.
 * In production, this would call an LLM API or use a local model.
 */
static void mock_llm(const char *query, char *out, size_t outsz)
{
    char normalized[1024];

    lowercase_copy(query, normalized, sizeof(normalized));

    /* Simple pattern matching for demo purposes */
    if (strstr(normalized, "list") || strstr(normalized, "show"))
    {
        if (strstr(normalized, "c file"))
            snprintf(out, outsz, "/bin/ls -la *.c");
        else if (strstr(normalized, "directory") || strstr(normalized, "dir"))
            snprintf(out, outsz, "/bin/ls -la");
        else
            snprintf(out, outsz, "/bin/ls -la");
    }
    else if (strstr(normalized, "find") || strstr(normalized, "search"))
    {
        if (strstr(normalized, "file"))
            snprintf(out, outsz, "find . -type f -name '%s'", query);
        else
            snprintf(out, outsz, "find . -type f");
    }
    else if (strstr(normalized, "count") || strstr(normalized, "how many"))
    {
        snprintf(out, outsz, "find . -type f | wc -l");
    }
    else if (strstr(normalized, "disk") || strstr(normalized, "size") || strstr(normalized, "space"))
    {
        snprintf(out, outsz, "df -h");
    }
    else if (strstr(normalized, "current") || strstr(normalized, "where"))
    {
        snprintf(out, outsz, "pwd");
    }
    else if (strstr(normalized, "help") || strstr(normalized, "?"))
    {
        snprintf(out, outsz, "help");
    }
    else
    {
        /* Default fallback: just echo the query with 'echo' command */
        snprintf(out, outsz, "echo \"Query: %s\"", query);
    }
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: coresh_llm <query>\n");
        fprintf(stderr, "Example: coresh_llm \"list all C files\"\n");
        return EXIT_FAILURE;
    }

    char query[1024];
    /* Reconstruct the full query from all arguments */
    query[0] = '\0';
    for (int i = 1; i < argc; i++)
    {
        if (i > 1)
            strcat(query, " ");
        strcat(query, argv[i]);
    }

    char suggested[1024];
    mock_llm(query, suggested, sizeof(suggested));

    /* Output the suggested command to stdout (one line) */
    printf("%s\n", suggested);
    fflush(stdout);

    return EXIT_SUCCESS;
}
