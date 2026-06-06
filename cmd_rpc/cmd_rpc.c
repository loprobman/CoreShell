#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>

#include "cmd_rpc.h"
#include "cmd_registry.h"
#include "argtable3.h"

#define RPC_DEFAULT_HOST "127.0.0.1"
#define RPC_DEFAULT_PORT "3000"
#define RPC_DEFAULT_TIMEOUT 3
#define RPC_DEFAULT_RETRIES 0
#define RPC_MAX_LINE 2048

typedef struct
{
    int success;
    int attempts;
    char error[128];
    char response[RPC_MAX_LINE];
} rpc_result_t;

static void build_rpc_argtable(struct arg_lit **help,
                               struct arg_lit **help_json,
                               struct arg_lit **json,
                               struct arg_str **host,
                               struct arg_int **port,
                               struct arg_int **timeout,
                               struct arg_int **retries,
                               struct arg_str **message,
                               struct arg_end **end,
                               void ***argtable_out)
{
    *help      = arg_lit0("h", "help", "show this help and exit");
    *help_json = arg_lit0(NULL, "help-json", "print argument schema as JSON and exit");
    *json      = arg_lit0(NULL, "json", "output response and metadata as JSON");
    *host      = arg_str0("H", "host", "<host>", "target host (default: 127.0.0.1)");
    *port      = arg_int0("p", "port", "<port>", "target TCP port (default: 3000)");
    *timeout   = arg_int0("t", "timeout", "<sec>", "connect/read/write timeout in seconds (default: 3)");
    *retries   = arg_int0("r", "retries", "<n>", "retry count after first attempt (default: 0)");
    *message   = arg_str1(NULL, NULL, "<message>", "line request payload");
    *end       = arg_end(20);

    static void *argtable[10];
    argtable[0] = *help;
    argtable[1] = *help_json;
    argtable[2] = *json;
    argtable[3] = *host;
    argtable[4] = *port;
    argtable[5] = *timeout;
    argtable[6] = *retries;
    argtable[7] = *message;
    argtable[8] = *end;
    argtable[9] = NULL;
    *argtable_out = argtable;
}

static int write_all(int fd, const char *buf, size_t len)
{
    size_t off = 0;
    while (off < len)
    {
        ssize_t n = send(fd, buf + off, len - off, 0);
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            return -1;
        }
        off += (size_t)n;
    }
    return 0;
}

static int read_line(int fd, char *out, size_t out_sz)
{
    size_t used = 0;
    while (used + 1 < out_sz)
    {
        char ch;
        ssize_t n = recv(fd, &ch, 1, 0);
        if (n == 0)
            break;
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            return -1;
        }

        if (ch == '\n')
            break;

        out[used++] = ch;
    }
    out[used] = '\0';

    if (used == 0)
        return 1;
    return 0;
}

static int connect_with_timeout(const char *host,
                                const char *port,
                                int timeout_sec,
                                int *sock_out,
                                char *errbuf,
                                size_t errbuf_sz)
{
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    struct addrinfo *rp;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int gai = getaddrinfo(host, port, &hints, &res);
    if (gai != 0)
    {
        snprintf(errbuf, errbuf_sz, "resolve failed: %s", gai_strerror(gai));
        return -1;
    }

    for (rp = res; rp != NULL; rp = rp->ai_next)
    {
        int s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (s < 0)
            continue;

        struct timeval tv;
        tv.tv_sec = timeout_sec;
        tv.tv_usec = 0;

        if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0 ||
            setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) != 0)
        {
            close(s);
            continue;
        }

        if (connect(s, rp->ai_addr, rp->ai_addrlen) == 0)
        {
            *sock_out = s;
            freeaddrinfo(res);
            return 0;
        }

        close(s);
    }

    freeaddrinfo(res);
    snprintf(errbuf, errbuf_sz, "connect failed: %s", strerror(errno));
    return -1;
}

static int rpc_exchange_once(const char *host,
                             const char *port,
                             int timeout_sec,
                             const char *message,
                             rpc_result_t *result)
{
    int s = -1;
    char err[128];

    if (connect_with_timeout(host, port, timeout_sec, &s, err, sizeof(err)) != 0)
    {
        snprintf(result->error, sizeof(result->error), "%s", err);
        return 1;
    }

    char line[RPC_MAX_LINE];
    size_t mlen = strlen(message);
    if (mlen + 1 >= sizeof(line))
    {
        close(s);
        snprintf(result->error, sizeof(result->error), "message too long");
        return 1;
    }

    memcpy(line, message, mlen);
    line[mlen] = '\n';
    line[mlen + 1] = '\0';

    if (write_all(s, line, mlen + 1) != 0)
    {
        close(s);
        snprintf(result->error, sizeof(result->error), "send failed: %s", strerror(errno));
        return 1;
    }

    int rr = read_line(s, result->response, sizeof(result->response));
    close(s);

    if (rr < 0)
    {
        snprintf(result->error, sizeof(result->error), "recv failed: %s", strerror(errno));
        return 1;
    }
    if (rr > 0)
    {
        snprintf(result->error, sizeof(result->error), "empty response");
        return 1;
    }

    return 0;
}

int rpc_run(int argc, char **argv)
{
    struct arg_lit *help;
    struct arg_lit *help_json;
    struct arg_lit *json;
    struct arg_str *host;
    struct arg_int *port;
    struct arg_int *timeout;
    struct arg_int *retries;
    struct arg_str *message;
    struct arg_end *end;
    void **argtable;

    build_rpc_argtable(&help, &help_json, &json,
                       &host, &port, &timeout, &retries,
                       &message, &end, &argtable);

    int nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0)
    {
        arg_freetable(argtable, 9);
        rpc_print_usage(stdout);
        return 0;
    }

    if (help_json->count > 0)
    {
        cmd_print_help_json(stdout, "rpc",
                            "send a line request to a TCP service",
                            "Connect to a TCP host/port, send one line request, and read one line response. "
                            "Includes timeout and retry controls for non-hanging shell behavior.",
                            argtable);
        arg_freetable(argtable, 9);
        return 0;
    }

    if (nerrors > 0)
    {
        arg_print_errors(stdout, end, "rpc");
        arg_freetable(argtable, 9);
        rpc_print_usage(stdout);
        return 1;
    }

    const char *host_v = host->count > 0 ? host->sval[0] : RPC_DEFAULT_HOST;
    int port_v = port->count > 0 ? port->ival[0] : 3000;
    int timeout_v = timeout->count > 0 ? timeout->ival[0] : RPC_DEFAULT_TIMEOUT;
    int retries_v = retries->count > 0 ? retries->ival[0] : RPC_DEFAULT_RETRIES;
    const char *msg_v = message->sval[0];

    if (port_v <= 0 || port_v > 65535)
    {
        fprintf(stderr, "rpc: invalid --port (1..65535)\n");
        arg_freetable(argtable, 9);
        return 1;
    }

    if (timeout_v <= 0 || timeout_v > 60)
    {
        fprintf(stderr, "rpc: invalid --timeout (1..60)\n");
        arg_freetable(argtable, 9);
        return 1;
    }

    if (retries_v < 0 || retries_v > 10)
    {
        fprintf(stderr, "rpc: invalid --retries (0..10)\n");
        arg_freetable(argtable, 9);
        return 1;
    }

    char port_buf[16];
    snprintf(port_buf, sizeof(port_buf), "%d", port_v);

    rpc_result_t result;
    memset(&result, 0, sizeof(result));

    int max_attempts = 1 + retries_v;
    int rc = 1;
    for (int attempt = 1; attempt <= max_attempts; attempt++)
    {
        result.attempts = attempt;
        if (rpc_exchange_once(host_v, port_buf, timeout_v, msg_v, &result) == 0)
        {
            result.success = 1;
            rc = 0;
            break;
        }
    }

    if (json->count > 0)
    {
        printf("{\n");
        printf("  \"ok\": %s,\n", result.success ? "true" : "false");
        printf("  \"host\": "); cmd_json_str(stdout, host_v); printf(",\n");
        printf("  \"port\": %d,\n", port_v);
        printf("  \"attempts\": %d,\n", result.attempts);
        printf("  \"response\": ");
        if (result.success)
            cmd_json_str(stdout, result.response);
        else
            fputs("null", stdout);
        printf(",\n");
        printf("  \"error\": ");
        if (result.success)
            fputs("null", stdout);
        else
            cmd_json_str(stdout, result.error);
        printf("\n}\n");
    }
    else if (result.success)
    {
        printf("%s\n", result.response);
    }
    else
    {
        fprintf(stderr,
                "rpc: request failed after %d attempt(s): %s\n",
                result.attempts,
                result.error[0] ? result.error : "unknown error");
    }

    arg_freetable(argtable, 9);
    return rc;
}

void rpc_print_usage(FILE *out)
{
    struct arg_lit *help;
    struct arg_lit *help_json;
    struct arg_lit *json;
    struct arg_str *host;
    struct arg_int *port;
    struct arg_int *timeout;
    struct arg_int *retries;
    struct arg_str *message;
    struct arg_end *end;
    void **argtable;

    build_rpc_argtable(&help, &help_json, &json,
                       &host, &port, &timeout, &retries,
                       &message, &end, &argtable);

    fprintf(out, "\nUsage: rpc ");
    arg_print_syntax(out, argtable, "\n");
    fprintf(out, "\nSend one line request to a TCP service and read one line response.\n");
    fprintf(out, "Uses timeout and retry controls so network issues do not hang the shell.\n");
    fprintf(out, "\nOptions:\n");
    arg_print_glossary(out, argtable, "  %-22s %s\n");
    fprintf(out, "\nExamples:\n");
    fprintf(out, "  rpc \"ping\"\n");
    fprintf(out, "  rpc -H 127.0.0.1 -p 5555 -t 2 -r 2 \"health\"\n");
    fprintf(out, "  rpc --json -H 127.0.0.1 -p 3000 \"status\"\n\n");

    arg_freetable(argtable, 9);
}

cmd_spec_t cmd_rpc_spec = {
    .name = "rpc",
    .summary = "send a line request to a TCP service",
    .long_help = "Connect to a TCP host/port, send one line request, and read one line response. "
                 "Includes timeout and retry controls for non-hanging shell behavior.",
    .run = rpc_run,
    .print_usage = rpc_print_usage,
};

void register_rpc_command(void)
{
    register_command(&cmd_rpc_spec);
}
