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
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define HTTP_BUF_SIZE 16384

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
            snprintf(out, outsz, "find . -type f -name '*.c'");
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

static void json_escape(const char *src, char *dst, size_t dstsz)
{
    size_t out = 0;

    if (dstsz == 0)
        return;

    for (size_t i = 0; src && src[i] != '\0' && out + 1 < dstsz; i++)
    {
        unsigned char c = (unsigned char)src[i];
        const char *rep = NULL;

        switch (c)
        {
            case '\\': rep = "\\\\"; break;
            case '"':  rep = "\\\""; break;
            case '\n': rep = "\\n"; break;
            case '\r': rep = "\\r"; break;
            case '\t': rep = "\\t"; break;
            default: break;
        }

        if (rep)
        {
            size_t n = strlen(rep);
            if (out + n >= dstsz)
                break;
            memcpy(dst + out, rep, n);
            out += n;
        }
        else
        {
            dst[out++] = (char)c;
        }
    }

    dst[out] = '\0';
}

static int extract_json_string(const char *json, const char *key, char *out, size_t outsz)
{
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char *p = strstr(json, pattern);
    if (!p)
        return 0;

    p = strchr(p, ':');
    if (!p)
        return 0;
    p++;
    while (*p && isspace((unsigned char)*p))
        p++;
    if (*p != '"')
        return 0;
    p++;

    size_t used = 0;
    while (*p && *p != '"' && used + 1 < outsz)
    {
        if (*p == '\\' && p[1] != '\0')
            p++;
        out[used++] = *p++;
    }
    out[used] = '\0';
    return 1;
}

static int fetch_agent_command(const char *query, char *out, size_t outsz)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return 0;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(3000);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    {
        close(fd);
        return 0;
    }

    char query_json[2048];
    json_escape(query, query_json, sizeof(query_json));

    char body[3072];
    int body_len = snprintf(body, sizeof(body), "{\"query\":\"%s\"}", query_json);
    if (body_len < 0 || body_len >= (int)sizeof(body))
    {
        close(fd);
        return 0;
    }

    char request[4096];
    int request_len = snprintf(request, sizeof(request),
                               "POST /agent/command HTTP/1.1\r\n"
                               "Host: 127.0.0.1:3000\r\n"
                               "Content-Type: application/json\r\n"
                               "Content-Length: %d\r\n"
                               "Connection: close\r\n\r\n"
                               "%s",
                               body_len, body);
    if (request_len < 0 || request_len >= (int)sizeof(request))
    {
        close(fd);
        return 0;
    }

    if (write(fd, request, (size_t)request_len) != request_len)
    {
        close(fd);
        return 0;
    }

    char response[HTTP_BUF_SIZE];
    size_t used = 0;
    ssize_t n;
    while ((n = read(fd, response + used, sizeof(response) - used - 1)) > 0)
    {
        used += (size_t)n;
        if (used + 1 >= sizeof(response))
            break;
    }
    close(fd);
    response[used] = '\0';

    if (strncmp(response, "HTTP/1.1 200", 12) != 0 && strncmp(response, "HTTP/1.0 200", 12) != 0)
        return 0;

    char *body_start = strstr(response, "\r\n\r\n");
    if (!body_start)
        return 0;
    body_start += 4;

    return extract_json_string(body_start, "command", out, outsz);
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
    if (!fetch_agent_command(query, suggested, sizeof(suggested)))
        mock_llm(query, suggested, sizeof(suggested));

    /* Output the suggested command to stdout (one line) */
    printf("%s\n", suggested);
    fflush(stdout);

    return EXIT_SUCCESS;
}
