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

/* ── Simple rule-based mock LLM ────────────────────────────────────────── */

/*
 * mock_llm - Map natural language to a shell command (demo implementation).
 *
 * This is a placeholder that demonstrates the interface.
 * In production, this would call an LLM API or use a local model.
 */
static void mock_llm(const char *query, char *out, size_t outsz)
{
    /* Simple pattern matching for demo purposes */
    if (strstr(query, "list") || strstr(query, "show"))
    {
        if (strstr(query, "C file"))
            snprintf(out, outsz, "ls -la *.c");
        else if (strstr(query, "directory") || strstr(query, "dir"))
            snprintf(out, outsz, "ls -la");
        else
            snprintf(out, outsz, "ls -la");
    }
    else if (strstr(query, "find") || strstr(query, "search"))
    {
        if (strstr(query, "file"))
            snprintf(out, outsz, "find . -type f -name '%s'", query);
        else
            snprintf(out, outsz, "find . -type f");
    }
    else if (strstr(query, "count") || strstr(query, "how many"))
    {
        snprintf(out, outsz, "find . -type f | wc -l");
    }
    else if (strstr(query, "disk") || strstr(query, "size") || strstr(query, "space"))
    {
        snprintf(out, outsz, "df -h");
    }
    else if (strstr(query, "current") || strstr(query, "where"))
    {
        snprintf(out, outsz, "pwd");
    }
    else if (strstr(query, "help") || strstr(query, "?"))
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
