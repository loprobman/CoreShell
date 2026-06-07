#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define TEST_PORT 9000
#define BUF_CAP 262144

static pid_t g_server_pid = -1;

static int connect_tcp(void)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(TEST_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
        return -1;
    }

    return fd;
}

static int send_request(const char *req, char *resp, size_t cap)
{
    int fd = connect_tcp();
    if (fd < 0) return -1;

    size_t req_len = strlen(req);
    if (write(fd, req, req_len) != (ssize_t)req_len) {
        close(fd);
        return -1;
    }
    if (write(fd, "\n", 1) != 1) {
        close(fd);
        return -1;
    }

    size_t off = 0;
    while (off + 1 < cap) {
        ssize_t n = read(fd, resp + off, cap - off - 1);
        if (n < 0) {
            close(fd);
            return -1;
        }
        if (n == 0) break;
        off += (size_t)n;
    }
    close(fd);
    resp[off] = '\0';

    char *newline = strchr(resp, '\n');
    if (newline) *newline = '\0';
    return 0;
}

static int expect_contains(const char *haystack, const char *needle, const char *name)
{
    if (!strstr(haystack, needle)) {
        fprintf(stderr, "[FAIL] %s: expected substring '%s'\n", name, needle);
        fprintf(stderr, "Response: %s\n", haystack);
        return 1;
    }
    return 0;
}

static int test_tools_list(void)
{
    char resp[BUF_CAP];
    if (send_request("{\"type\":\"tools/list\"}", resp, sizeof(resp)) != 0) {
        fprintf(stderr, "[FAIL] tools/list request failed\n");
        return 1;
    }

    int fails = 0;
    fails += expect_contains(resp, "\"ok\":true", "tools/list ok");
    fails += expect_contains(resp, "\"registry.package.lookup\"", "tools/list tool lookup");
    fails += expect_contains(resp, "\"filesystem.delete_older_than_days\"", "tools/list delete tool");
    fails += expect_contains(resp, "\"rag.docs.search\"", "tools/list rag search");
    fails += expect_contains(resp, "\"rag.command.recommend\"", "tools/list rag recommend");
    fails += expect_contains(resp, "\"agent.command.plan\"", "tools/list agent plan");
    if (fails == 0) fprintf(stdout, "[PASS] tools/list\n");
    return fails;
}

static int test_legacy_initialize(void)
{
    char resp[BUF_CAP];
    if (send_request("{\"id\":1,\"method\":\"initialize\",\"params\":{}}", resp, sizeof(resp)) != 0) {
        fprintf(stderr, "[FAIL] legacy initialize request failed\n");
        return 1;
    }

    int fails = 0;
    fails += expect_contains(resp, "\"id\":1", "legacy initialize id echo");
    fails += expect_contains(resp, "\"type\":\"response\"", "legacy initialize type");
    fails += expect_contains(resp, "\"server\":\"CoreShell MCP Server\"", "legacy initialize server");
    if (fails == 0) fprintf(stdout, "[PASS] legacy initialize\n");
    return fails;
}

static int test_legacy_list_tools(void)
{
    char resp[BUF_CAP];
    if (send_request("{\"id\":2,\"method\":\"list_tools\",\"params\":{}}", resp, sizeof(resp)) != 0) {
        fprintf(stderr, "[FAIL] legacy list_tools request failed\n");
        return 1;
    }

    int fails = 0;
    fails += expect_contains(resp, "\"id\":2", "legacy list_tools id echo");
    fails += expect_contains(resp, "\"list_files\"", "legacy list_tools list_files");
    fails += expect_contains(resp, "\"get_time\"", "legacy list_tools get_time");
    fails += expect_contains(resp, "\"delete_older_than_days\"", "legacy list_tools delete tool");
    if (fails == 0) fprintf(stdout, "[PASS] legacy list_tools\n");
    return fails;
}

static int test_legacy_call_tool_get_time(void)
{
    char resp[BUF_CAP];
    if (send_request("{\"id\":3,\"method\":\"call_tool\",\"params\":{\"tool\":\"get_time\",\"args\":{}}}", resp, sizeof(resp)) != 0) {
        fprintf(stderr, "[FAIL] legacy call_tool get_time request failed\n");
        return 1;
    }

    int fails = 0;
    fails += expect_contains(resp, "\"id\":3", "legacy call_tool get_time id echo");
    fails += expect_contains(resp, "\"tool\":\"get_time\"", "legacy call_tool get_time tool");
    fails += expect_contains(resp, "\"time\":", "legacy call_tool get_time field");
    if (fails == 0) fprintf(stdout, "[PASS] legacy call_tool get_time\n");
    return fails;
}

static int test_legacy_call_tool_list_files(void)
{
    char resp[BUF_CAP];
    if (send_request("{\"id\":4,\"method\":\"call_tool\",\"params\":{\"tool\":\"list_files\",\"args\":{\"path\":\".\"}}}", resp, sizeof(resp)) != 0) {
        fprintf(stderr, "[FAIL] legacy call_tool list_files request failed\n");
        return 1;
    }

    int fails = 0;
    fails += expect_contains(resp, "\"id\":4", "legacy call_tool list_files id echo");
    fails += expect_contains(resp, "\"tool\":\"list_files\"", "legacy call_tool list_files tool");
    fails += expect_contains(resp, "\"output\":", "legacy call_tool list_files output");
    if (fails == 0) fprintf(stdout, "[PASS] legacy call_tool list_files\n");
    return fails;
}

static int test_lookup_echo(void)
{
    char resp[BUF_CAP];
    if (send_request("{\"type\":\"tools/call\",\"tool\":\"registry.package.lookup\",\"arguments\":{\"name\":\"echo\"}}", resp, sizeof(resp)) != 0) {
        fprintf(stderr, "[FAIL] registry.package.lookup request failed\n");
        return 1;
    }

    int fails = 0;
    fails += expect_contains(resp, "\"ok\":true", "lookup ok");
    fails += expect_contains(resp, "\"tool\":\"registry.package.lookup\"", "lookup tool tag");
    fails += expect_contains(resp, "\"name\":\"echo\"", "lookup echo name");
    if (fails == 0) fprintf(stdout, "[PASS] registry.package.lookup\n");
    return fails;
}

static int test_delete_dry_run(void)
{
    char resp[BUF_CAP];
    if (send_request("{\"type\":\"tools/call\",\"tool\":\"filesystem.delete_older_than_days\",\"arguments\":{\"path\":\"artifacts\",\"days\":30,\"dryRun\":true}}", resp, sizeof(resp)) != 0) {
        fprintf(stderr, "[FAIL] filesystem.delete_older_than_days request failed\n");
        return 1;
    }

    int fails = 0;
    fails += expect_contains(resp, "\"ok\":true", "delete dry run ok");
    fails += expect_contains(resp, "\"tool\":\"filesystem.delete_older_than_days\"", "delete tool tag");
    fails += expect_contains(resp, "\"dryRun\":true", "delete dry run true");
    if (fails == 0) fprintf(stdout, "[PASS] filesystem.delete_older_than_days dry run\n");
    return fails;
}

static int test_unknown_method(void)
{
    char resp[BUF_CAP];
    if (send_request("{\"type\":\"ping\"}", resp, sizeof(resp)) != 0) {
        fprintf(stderr, "[FAIL] unknown method request failed\n");
        return 1;
    }

    int fails = 0;
    fails += expect_contains(resp, "\"ok\":false", "unknown method not ok");
    fails += expect_contains(resp, "\"UNKNOWN_METHOD\"", "unknown method code");
    if (fails == 0) fprintf(stdout, "[PASS] unknown method\n");
    return fails;
}

static int test_rag_docs_search(void)
{
    char resp[BUF_CAP];
    if (send_request("{\"type\":\"tools/call\",\"tool\":\"rag.docs.search\","
                     "\"arguments\":{\"query\":\"print working directory\",\"topK\":3}}",
                     resp, sizeof(resp)) != 0) {
        fprintf(stderr, "[FAIL] rag.docs.search request failed\n");
        return 1;
    }

    int fails = 0;
    fails += expect_contains(resp, "\"ok\":true", "rag.docs.search ok");
    fails += expect_contains(resp, "\"tool\":\"rag.docs.search\"", "rag.docs.search tool tag");
    fails += expect_contains(resp, "\"command\":\"pwd\"", "rag.docs.search pwd hit");
    fails += expect_contains(resp, "cmd_pwd/docs/pwd.md", "rag.docs.search source path");
    if (fails == 0) fprintf(stdout, "[PASS] rag.docs.search\n");
    return fails;
}

static int test_rag_command_recommend(void)
{
    char resp[BUF_CAP];
    if (send_request("{\"type\":\"tools/call\",\"tool\":\"rag.command.recommend\","
                     "\"arguments\":{\"query\":\"How do I print my working directory?\"}}",
                     resp, sizeof(resp)) != 0) {
        fprintf(stderr, "[FAIL] rag.command.recommend request failed\n");
        return 1;
    }

    int fails = 0;
    fails += expect_contains(resp, "\"ok\":true", "rag.command.recommend ok");
    fails += expect_contains(resp, "\"tool\":\"rag.command.recommend\"", "rag.command.recommend tool tag");
    fails += expect_contains(resp, "\"command\":\"pwd\"", "rag.command.recommend pwd");
    fails += expect_contains(resp, "\"citations\":[", "rag.command.recommend citations");
    if (fails == 0) fprintf(stdout, "[PASS] rag.command.recommend\n");
    return fails;
}

static int test_agent_command_plan(void)
{
    char resp[BUF_CAP];
    if (send_request("{\"type\":\"tools/call\",\"tool\":\"agent.command.plan\","
                     "\"arguments\":{\"query\":\"How do I print my working directory?\"}}",
                     resp, sizeof(resp)) != 0) {
        fprintf(stderr, "[FAIL] agent.command.plan request failed\n");
        return 1;
    }

    int fails = 0;
    fails += expect_contains(resp, "\"ok\":true", "agent.command.plan ok");
    fails += expect_contains(resp, "\"tool\":\"agent.command.plan\"", "agent.command.plan tool tag");
    fails += expect_contains(resp, "\"command\":\"pwd\"", "agent.command.plan pwd");
    fails += expect_contains(resp, "\"provider\":", "agent.command.plan provider");
    if (fails == 0) fprintf(stdout, "[PASS] agent.command.plan\n");
    return fails;
}

static int wait_for_server_ready(void)
{
    const int retries = 40;
    for (int i = 0; i < retries; i++) {
        int fd = connect_tcp();
        if (fd >= 0) {
            close(fd);
            return 0;
        }
        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = 100000000L;
        nanosleep(&ts, NULL);
    }
    return -1;
}

static int start_server(void)
{
    g_server_pid = fork();
    if (g_server_pid < 0) return -1;

    if (g_server_pid == 0) {
        execl("./mcp_server", "./mcp_server", (char *)NULL);
        _exit(127);
    }

    if (wait_for_server_ready() != 0) {
        kill(g_server_pid, SIGTERM);
        waitpid(g_server_pid, NULL, 0);
        g_server_pid = -1;
        return -1;
    }

    return 0;
}

static void stop_server(void)
{
    if (g_server_pid <= 0) return;
    kill(g_server_pid, SIGTERM);
    waitpid(g_server_pid, NULL, 0);
    g_server_pid = -1;
}

int main(void)
{
    if (start_server() != 0) {
        fprintf(stderr, "[FAIL] unable to start ./mcp_server on port %d\n", TEST_PORT);
        return 1;
    }

    int fails = 0;
    fails += test_legacy_initialize();
    fails += test_legacy_list_tools();
    fails += test_legacy_call_tool_get_time();
    fails += test_legacy_call_tool_list_files();
    fails += test_tools_list();
    fails += test_lookup_echo();
    fails += test_delete_dry_run();
    fails += test_unknown_method();
    fails += test_rag_docs_search();
    fails += test_rag_command_recommend();
    fails += test_agent_command_plan();

    stop_server();

    if (fails == 0) {
        fprintf(stdout, "All native C MCP server tests passed.\n");
        return 0;
    }

    fprintf(stderr, "Native C MCP server tests failed: %d\n", fails);
    return 1;
}
