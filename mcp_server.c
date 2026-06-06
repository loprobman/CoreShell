#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define MCP_PORT 9000
#define BACKLOG 16
#define REQ_MAX 65536
#define RESP_MAX 262144
#define SMALL_BUF 1024
#define LOG_PATH "artifacts/mcp_calls.log"
#define MAX_SCAN_FILES 256
#define RAG_MAX_DOCS   64
#define RAG_MAX_TERMS  48
#define RAG_TEXT_MAX   16384
#define RAG_SNIP_MAX   256

typedef struct {
    char command[SMALL_BUF];
    char source_path[256];
    char raw_text[RAG_TEXT_MAX];
    char text[RAG_TEXT_MAX];
} rag_doc_t;

typedef struct {
    size_t idx;
    double score;
} rag_hit_t;

static char      g_workspace[PATH_MAX];
static rag_doc_t g_rag_corpus[RAG_MAX_DOCS];
static size_t    g_rag_count = 0;
static bool      g_rag_ready = false;

typedef struct {
    char **items;
    size_t count;
    size_t cap;
} strvec_t;

static void strvec_init(strvec_t *v)
{
    v->items = NULL;
    v->count = 0;
    v->cap = 0;
}

static void strvec_free(strvec_t *v)
{
    if (!v) return;
    for (size_t i = 0; i < v->count; i++) free(v->items[i]);
    free(v->items);
    v->items = NULL;
    v->count = 0;
    v->cap = 0;
}

static bool strvec_push(strvec_t *v, const char *s)
{
    if (v->count == v->cap) {
        size_t next = v->cap == 0 ? 8 : v->cap * 2;
        char **tmp = (char **)realloc(v->items, next * sizeof(char *));
        if (!tmp) return false;
        v->items = tmp;
        v->cap = next;
    }
    v->items[v->count] = strdup(s ? s : "");
    if (!v->items[v->count]) return false;
    v->count += 1;
    return true;
}

static int appendf(char *dst, size_t cap, size_t *len, const char *fmt, ...)
{
    if (*len >= cap) return -1;
    va_list ap;
    va_start(ap, fmt);
    int wrote = vsnprintf(dst + *len, cap - *len, fmt, ap);
    va_end(ap);
    if (wrote < 0) return -1;
    if ((size_t)wrote >= cap - *len) return -1;
    *len += (size_t)wrote;
    return 0;
}

static bool copy_string(char *dst, size_t cap, const char *src)
{
    size_t n = strlen(src ? src : "");
    if (n + 1 > cap) return false;
    memcpy(dst, src, n + 1);
    return true;
}

static bool build_path_parts(char *out, size_t cap,
                             const char *a,
                             const char *b,
                             const char *c,
                             const char *d,
                             const char *e)
{
    const char *parts[5] = { a, b, c, d, e };
    size_t used = 0;

    if (cap == 0) return false;
    out[0] = '\0';

    for (size_t i = 0; i < 5; i++) {
        const char *part = parts[i];
        if (!part || part[0] == '\0') continue;
        size_t n = strlen(part);
        if (used + n + 1 > cap) return false;
        memcpy(out + used, part, n);
        used += n;
    }

    out[used] = '\0';
    return true;
}

static void json_escape(const char *src, char *dst, size_t cap)
{
    size_t o = 0;
    if (cap == 0) return;
    for (size_t i = 0; src && src[i] != '\0'; i++) {
        unsigned char c = (unsigned char)src[i];
        const char *rep = NULL;
        char tmp[7];

        switch (c) {
            case '\\': rep = "\\\\"; break;
            case '"': rep = "\\\""; break;
            case '\n': rep = "\\n"; break;
            case '\r': rep = "\\r"; break;
            case '\t': rep = "\\t"; break;
            default:
                if (c < 0x20) {
                    snprintf(tmp, sizeof(tmp), "\\u%04x", c);
                    rep = tmp;
                }
                break;
        }

        if (rep) {
            size_t n = strlen(rep);
            if (o + n + 1 >= cap) break;
            memcpy(dst + o, rep, n);
            o += n;
        } else {
            if (o + 2 >= cap) break;
            dst[o++] = (char)c;
        }
    }
    dst[o] = '\0';
}

static bool read_file_to_buffer(const char *path, char *out, size_t outcap)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) return false;
    size_t n = fread(out, 1, outcap - 1, fp);
    out[n] = '\0';
    fclose(fp);
    return true;
}

static bool extract_json_string(const char *json, const char *key, char *out, size_t outcap)
{
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char *p = strstr(json, pattern);
    if (!p) return false;
    p = strchr(p, ':');
    if (!p) return false;
    p++;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != '"') return false;
    p++;

    size_t o = 0;
    while (*p && *p != '"') {
        if (*p == '\\' && p[1]) p++;
        if (o + 1 >= outcap) break;
        out[o++] = *p++;
    }
    out[o] = '\0';
    return true;
}

static bool extract_json_number(const char *json, const char *key, double *out)
{
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char *p = strstr(json, pattern);
    if (!p) return false;
    p = strchr(p, ':');
    if (!p) return false;
    p++;
    while (*p && isspace((unsigned char)*p)) p++;

    char *endptr = NULL;
    double v = strtod(p, &endptr);
    if (endptr == p) return false;
    *out = v;
    return true;
}

static bool extract_json_bool(const char *json, const char *key, bool *out)
{
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char *p = strstr(json, pattern);
    if (!p) return false;
    p = strchr(p, ':');
    if (!p) return false;
    p++;
    while (*p && isspace((unsigned char)*p)) p++;

    if (strncmp(p, "true", 4) == 0) {
        *out = true;
        return true;
    }
    if (strncmp(p, "false", 5) == 0) {
        *out = false;
        return true;
    }
    return false;
}

static void extract_args_array(const char *json, strvec_t *args)
{
    const char *p = strstr(json, "\"args\"");
    if (!p) return;
    p = strchr(p, '[');
    if (!p) return;
    p++;

    while (*p && *p != ']') {
        while (*p && (isspace((unsigned char)*p) || *p == ',')) p++;
        if (*p != '"') break;
        p++;

        char item[SMALL_BUF];
        size_t o = 0;
        while (*p && *p != '"') {
            if (*p == '\\' && p[1]) p++;
            if (o + 1 < sizeof(item)) item[o++] = *p;
            p++;
        }
        item[o] = '\0';
        if (*p == '"') p++;
        (void)strvec_push(args, item);
    }
}

static bool path_within_workspace(const char *candidate)
{
    size_t root_len = strlen(g_workspace);
    if (strncmp(candidate, g_workspace, root_len) != 0) return false;
    if (candidate[root_len] == '\0' || candidate[root_len] == '/') return true;
    return false;
}

static bool resolve_workspace_path(const char *raw, char *out, size_t outcap)
{
    if (!raw || raw[0] == '\0') return false;

    if (raw[0] == '/') {
        if (snprintf(out, outcap, "%s", raw) >= (int)outcap) return false;
    } else {
        if (snprintf(out, outcap, "%s/%s", g_workspace, raw) >= (int)outcap) return false;
    }

    char normalized[PATH_MAX];
    if (!realpath(out, normalized)) {
        char parent[PATH_MAX];
        if (snprintf(parent, sizeof(parent), "%s", out) >= (int)sizeof(parent)) return false;
        char *slash = strrchr(parent, '/');
        if (!slash) return false;
        *slash = '\0';
        char parent_real[PATH_MAX];
        if (!realpath(parent, parent_real)) return false;
        if (snprintf(normalized, sizeof(normalized), "%s/%s", parent_real, slash + 1) >= (int)sizeof(normalized)) return false;
    }

    if (!path_within_workspace(normalized)) return false;
    if (snprintf(out, outcap, "%s", normalized) >= (int)outcap) return false;
    return true;
}

static void log_call(const char *request, const char *response)
{
    FILE *fp = fopen(LOG_PATH, "a");
    if (!fp) return;

    char ts[64];
    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S%z", &tm_now);

    fprintf(fp, "{\"ts\":\"%s\",\"request\":%s,\"response\":%s}\n", ts, request, response);
    fclose(fp);
}

static void respond_error(char *resp, size_t cap, const char *type, const char *code, const char *msg)
{
    char esc[SMALL_BUF];
    json_escape(msg, esc, sizeof(esc));
    snprintf(resp, cap,
             "{\"ok\":false,\"type\":\"%s\",\"error\":{\"code\":\"%s\",\"message\":\"%s\"},\"protocol\":\"mcp-line-json\"}",
             type ? type : "tools/call", code, esc);
}

static void read_pkg_field(const char *pkg_path, const char *field, char *out, size_t outcap)
{
    char raw[8192];
    out[0] = '\0';
    if (!read_file_to_buffer(pkg_path, raw, sizeof(raw))) return;
    (void)extract_json_string(raw, field, out, outcap);
}

static void build_tools_list_response(char *resp, size_t cap)
{
    snprintf(resp, cap,
             "{\"ok\":true,\"type\":\"tools/list\",\"protocol\":\"mcp-line-json\",\"tools\":["
             "{\"name\":\"registry.packages.list\",\"description\":\"List the CoreShell packages known to the registry\"},"
             "{\"name\":\"registry.package.lookup\",\"description\":\"Look up one package by name\"},"
             "{\"name\":\"shell.commands.list\",\"description\":\"List the CoreShell commands exposed by the shell\"},"
             "{\"name\":\"shell.command.help\",\"description\":\"Return the help metadata for a CoreShell command\"},"
             "{\"name\":\"shell.command.run\",\"description\":\"Run a small allowlisted shell command\"},"
             "{\"name\":\"filesystem.delete_older_than_days\",\"description\":\"Delete files older than N days under a workspace path\"},"
             "{\"name\":\"rag.docs.search\",\"description\":\"Retrieve the most relevant CoreShell command docs for a natural-language query\"},"
             "{\"name\":\"rag.command.recommend\",\"description\":\"Recommend one CoreShell command for a natural-language task, grounded in retrieved docs\"},"
             "{\"name\":\"agent.command.plan\",\"description\":\"Use an OpenAI-backed agent to suggest one CoreShell command with grounding and execution notes\"}"
             "]}");
}

static void build_registry_packages_list(char *resp, size_t cap)
{
    DIR *root = opendir(g_workspace);
    if (!root) {
        respond_error(resp, cap, "tools/call", "COMMAND_EXECUTION_FAILED", "Unable to read workspace");
        return;
    }

    size_t len = 0;
    appendf(resp, cap, &len, "{\"ok\":true,\"tool\":\"registry.packages.list\",\"result\":[");

    bool first = true;
    struct dirent *de;
    while ((de = readdir(root)) != NULL) {
        if (strncmp(de->d_name, "cmd_", 4) != 0) continue;

        char pkg_path[PATH_MAX];
        if (!build_path_parts(pkg_path, sizeof(pkg_path),
                              g_workspace, "/", de->d_name, "/pkg.json", "")) {
            continue;
        }

        char name[SMALL_BUF], version[SMALL_BUF];
        read_pkg_field(pkg_path, "name", name, sizeof(name));
        read_pkg_field(pkg_path, "version", version, sizeof(version));
        if (name[0] == '\0' || version[0] == '\0') continue;

        char name_esc[SMALL_BUF], ver_esc[SMALL_BUF], url_esc[SMALL_BUF * 2];
        char url[SMALL_BUF * 2];
        size_t url_len = 0;
        if (appendf(url, sizeof(url), &url_len,
                "http://localhost:3000/downloads/%s-%s.tar.gz", name, version) != 0) {
            continue;
        }
        json_escape(name, name_esc, sizeof(name_esc));
        json_escape(version, ver_esc, sizeof(ver_esc));
        json_escape(url, url_esc, sizeof(url_esc));

        appendf(resp, cap, &len, "%s{\"name\":\"%s\",\"latestVersion\":\"%s\",\"downloadUrl\":\"%s\"}",
                first ? "" : ",", name_esc, ver_esc, url_esc);
        first = false;
    }
    closedir(root);

    appendf(resp, cap, &len, "],\"type\":\"tools/call\",\"protocol\":\"mcp-line-json\"}");
}

static void build_registry_package_lookup(const char *req, char *resp, size_t cap)
{
    char name[SMALL_BUF];
    if (!extract_json_string(req, "name", name, sizeof(name))) {
        respond_error(resp, cap, "tools/call", "BAD_ARGUMENTS", "name is required");
        return;
    }

    char pkg_path[PATH_MAX];
    if (!build_path_parts(pkg_path, sizeof(pkg_path),
                          g_workspace, "/cmd_", name, "/pkg.json", "")) {
        respond_error(resp, cap, "tools/call", "BAD_ARGUMENTS", "command path is too long");
        return;
    }

    char found_name[SMALL_BUF], version[SMALL_BUF];
    read_pkg_field(pkg_path, "name", found_name, sizeof(found_name));
    read_pkg_field(pkg_path, "version", version, sizeof(version));

    if (found_name[0] == '\0' || version[0] == '\0') {
        char name_esc[SMALL_BUF];
        json_escape(name, name_esc, sizeof(name_esc));
        snprintf(resp, cap,
                 "{\"ok\":false,\"tool\":\"registry.package.lookup\",\"error\":{\"code\":\"PACKAGE_NOT_FOUND\",\"message\":\"Package not found\",\"packageName\":\"%s\"},\"type\":\"tools/call\",\"protocol\":\"mcp-line-json\"}",
                 name_esc);
        return;
    }

    char name_esc[SMALL_BUF], ver_esc[SMALL_BUF], url_esc[SMALL_BUF * 2];
    char url[SMALL_BUF * 2];
    size_t url_len = 0;
    if (appendf(url, sizeof(url), &url_len,
                "http://localhost:3000/downloads/%s-%s.tar.gz", found_name, version) != 0) {
        respond_error(resp, cap, "tools/call", "COMMAND_EXECUTION_FAILED", "download URL is too long");
        return;
    }
    json_escape(found_name, name_esc, sizeof(name_esc));
    json_escape(version, ver_esc, sizeof(ver_esc));
    json_escape(url, url_esc, sizeof(url_esc));

    snprintf(resp, cap,
             "{\"ok\":true,\"tool\":\"registry.package.lookup\",\"result\":{\"name\":\"%s\",\"latestVersion\":\"%s\",\"downloadUrl\":\"%s\"},\"type\":\"tools/call\",\"protocol\":\"mcp-line-json\"}",
             name_esc, ver_esc, url_esc);
}

static int cmd_name_cmp(const void *a, const void *b)
{
    const char *sa = *(const char *const *)a;
    const char *sb = *(const char *const *)b;
    return strcmp(sa, sb);
}

static void build_shell_commands_list(char *resp, size_t cap)
{
    DIR *root = opendir(g_workspace);
    if (!root) {
        respond_error(resp, cap, "tools/call", "COMMAND_EXECUTION_FAILED", "Unable to read workspace");
        return;
    }

    strvec_t lines;
    strvec_init(&lines);

    struct dirent *de;
    while ((de = readdir(root)) != NULL) {
        if (strncmp(de->d_name, "cmd_", 4) != 0) continue;

        char pkg_path[PATH_MAX];
        if (!build_path_parts(pkg_path, sizeof(pkg_path),
                              g_workspace, "/", de->d_name, "/pkg.json", "")) {
            continue;
        }

        char name[SMALL_BUF], summary[SMALL_BUF], long_desc[2048];
        read_pkg_field(pkg_path, "name", name, sizeof(name));
        read_pkg_field(pkg_path, "description", summary, sizeof(summary));
        read_pkg_field(pkg_path, "long_description", long_desc, sizeof(long_desc));
        if (name[0] == '\0') continue;

        char name_esc[SMALL_BUF], sum_esc[SMALL_BUF], long_esc[2048], docs_esc[4096];
        char docs[4096];
        if (!build_path_parts(docs, sizeof(docs), "cmd_", name, "/docs/", name, ".md")) {
            continue;
        }
        json_escape(name, name_esc, sizeof(name_esc));
        json_escape(summary, sum_esc, sizeof(sum_esc));
        json_escape(long_desc, long_esc, sizeof(long_esc));
        json_escape(docs, docs_esc, sizeof(docs_esc));

        char row[8192];
        size_t row_len = 0;
        if (appendf(row, sizeof(row), &row_len,
                    "{\"name\":\"%s\",\"summary\":\"%s\",\"longDescription\":\"%s\",\"docsPath\":\"%s\"}",
                    name_esc, sum_esc, long_esc, docs_esc) != 0) {
            continue;
        }
        (void)strvec_push(&lines, row);
    }
    closedir(root);

    qsort(lines.items, lines.count, sizeof(char *), cmd_name_cmp);

    size_t len = 0;
    appendf(resp, cap, &len, "{\"ok\":true,\"tool\":\"shell.commands.list\",\"result\":[");
    for (size_t i = 0; i < lines.count; i++) {
        appendf(resp, cap, &len, "%s%s", i == 0 ? "" : ",", lines.items[i]);
    }
    appendf(resp, cap, &len, "],\"type\":\"tools/call\",\"protocol\":\"mcp-line-json\"}");

    strvec_free(&lines);
}

static void build_shell_command_help(const char *req, char *resp, size_t cap)
{
    char name[SMALL_BUF];
    if (!extract_json_string(req, "name", name, sizeof(name))) {
        respond_error(resp, cap, "tools/call", "BAD_ARGUMENTS", "name is required");
        return;
    }

    for (size_t i = 0; name[i] != '\0'; i++) {
        if (!(isalnum((unsigned char)name[i]) || name[i] == '_' || name[i] == '-')) {
            respond_error(resp, cap, "tools/call", "INVALID_COMMAND_NAME", "name must be alphanumeric");
            return;
        }
    }

    char pkg_path[PATH_MAX], docs_path[PATH_MAX];
    if (!build_path_parts(pkg_path, sizeof(pkg_path),
                          g_workspace, "/cmd_", name, "/pkg.json", "")) {
        respond_error(resp, cap, "tools/call", "BAD_ARGUMENTS", "command path is too long");
        return;
    }
    if (!build_path_parts(docs_path, sizeof(docs_path),
                          g_workspace, "/cmd_", name, "/docs/", "") ||
        !build_path_parts(docs_path, sizeof(docs_path),
                          docs_path, name, ".md", "", "")) {
        respond_error(resp, cap, "tools/call", "BAD_ARGUMENTS", "docs path is too long");
        return;
    }

    char pkg_name[SMALL_BUF], summary[SMALL_BUF], long_desc[2048], docs[16384];
    read_pkg_field(pkg_path, "name", pkg_name, sizeof(pkg_name));
    read_pkg_field(pkg_path, "description", summary, sizeof(summary));
    read_pkg_field(pkg_path, "long_description", long_desc, sizeof(long_desc));
    if (pkg_name[0] == '\0') {
        respond_error(resp, cap, "tools/call", "COMMAND_NOT_FOUND", "Command not found");
        return;
    }

    if (!read_file_to_buffer(docs_path, docs, sizeof(docs))) {
        snprintf(docs, sizeof(docs), "Documentation file not found: %s", docs_path);
    }

    char name_esc[SMALL_BUF], sum_esc[SMALL_BUF], long_esc[2048], docs_esc[32768];
    json_escape(pkg_name, name_esc, sizeof(name_esc));
    json_escape(summary, sum_esc, sizeof(sum_esc));
    json_escape(long_desc, long_esc, sizeof(long_esc));
    json_escape(docs, docs_esc, sizeof(docs_esc));

    snprintf(resp, cap,
             "{\"ok\":true,\"tool\":\"shell.command.help\",\"result\":{\"name\":\"%s\",\"summary\":\"%s\",\"longDescription\":\"%s\",\"docs\":\"%s\"},\"type\":\"tools/call\",\"protocol\":\"mcp-line-json\"}",
             name_esc, sum_esc, long_esc, docs_esc);
}

static void build_shell_command_run(const char *req, char *resp, size_t cap)
{
    char name[SMALL_BUF];
    if (!extract_json_string(req, "name", name, sizeof(name))) {
        respond_error(resp, cap, "tools/call", "BAD_ARGUMENTS", "name is required");
        return;
    }

    strvec_t args;
    strvec_init(&args);
    extract_args_array(req, &args);

    if (strcmp(name, "echo") == 0) {
        char out[8192] = {0};
        size_t len = 0;
        for (size_t i = 0; i < args.count; i++) {
            if (i != 0 && len + 1 < sizeof(out)) out[len++] = ' ';
            size_t n = strlen(args.items[i]);
            if (len + n >= sizeof(out)) n = sizeof(out) - len - 1;
            memcpy(out + len, args.items[i], n);
            len += n;
        }
        out[len] = '\0';

        char esc[16384];
        json_escape(out, esc, sizeof(esc));
        snprintf(resp, cap,
                 "{\"ok\":true,\"tool\":\"shell.command.run\",\"result\":{\"name\":\"echo\",\"stdout\":\"%s\"},\"type\":\"tools/call\",\"protocol\":\"mcp-line-json\"}",
                 esc);
        strvec_free(&args);
        return;
    }

    if (strcmp(name, "pwd") == 0) {
        char cwd[PATH_MAX], esc[PATH_MAX * 2];
        if (!getcwd(cwd, sizeof(cwd))) {
            respond_error(resp, cap, "tools/call", "COMMAND_EXECUTION_FAILED", "getcwd failed");
            strvec_free(&args);
            return;
        }
        json_escape(cwd, esc, sizeof(esc));
        snprintf(resp, cap,
                 "{\"ok\":true,\"tool\":\"shell.command.run\",\"result\":{\"name\":\"pwd\",\"stdout\":\"%s\"},\"type\":\"tools/call\",\"protocol\":\"mcp-line-json\"}",
                 esc);
        strvec_free(&args);
        return;
    }

    if (strcmp(name, "help") == 0) {
        char msg[SMALL_BUF] = "Use shell.commands.list and shell.command.help for structured help output.";
        char esc[SMALL_BUF * 2];
        json_escape(msg, esc, sizeof(esc));
        snprintf(resp, cap,
                 "{\"ok\":true,\"tool\":\"shell.command.run\",\"result\":{\"name\":\"help\",\"stdout\":\"%s\"},\"type\":\"tools/call\",\"protocol\":\"mcp-line-json\"}",
                 esc);
        strvec_free(&args);
        return;
    }

    respond_error(resp, cap, "tools/call", "COMMAND_NOT_ALLOWED", "Only echo, pwd, and help are enabled in native C mode");
    strvec_free(&args);
}

static int scan_old_files(const char *root, double threshold_ms, strvec_t *matched)
{
    DIR *dir = opendir(root);
    if (!dir) return 0;

    struct dirent *de;
    while ((de = readdir(dir)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;

        char pathbuf[PATH_MAX];
        if (snprintf(pathbuf, sizeof(pathbuf), "%s/%s", root, de->d_name) >= (int)sizeof(pathbuf)) continue;

        struct stat st;
        if (lstat(pathbuf, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            scan_old_files(pathbuf, threshold_ms, matched);
        } else if (S_ISREG(st.st_mode)) {
            double mtime_ms = (double)st.st_mtim.tv_sec * 1000.0 + (double)st.st_mtim.tv_nsec / 1000000.0;
            if (mtime_ms < threshold_ms) {
                if (matched->count < MAX_SCAN_FILES) {
                    (void)strvec_push(matched, pathbuf);
                }
            }
        }
    }

    closedir(dir);
    return 0;
}

static void build_delete_older_than_days(const char *req, char *resp, size_t cap)
{
    char path_arg[PATH_MAX];
    double days = 0.0;
    bool dry_run = true;

    if (!extract_json_string(req, "path", path_arg, sizeof(path_arg))) {
        respond_error(resp, cap, "tools/call", "BAD_ARGUMENTS", "path is required");
        return;
    }
    if (!extract_json_number(req, "days", &days) || days < 0) {
        respond_error(resp, cap, "tools/call", "BAD_ARGUMENTS", "days must be non-negative");
        return;
    }
    (void)extract_json_bool(req, "dryRun", &dry_run);

    char resolved[PATH_MAX];
    if (!resolve_workspace_path(path_arg, resolved, sizeof(resolved))) {
        respond_error(resp, cap, "tools/call", "COMMAND_NOT_ALLOWED", "path must be inside workspace");
        return;
    }

    struct stat st;
    if (stat(resolved, &st) != 0) {
        respond_error(resp, cap, "tools/call", "COMMAND_EXECUTION_FAILED", "stat failed");
        return;
    }

    strvec_t matched;
    strvec_init(&matched);

    double now_ms = (double)time(NULL) * 1000.0;
    double threshold_ms = now_ms - days * 24.0 * 60.0 * 60.0 * 1000.0;

    if (S_ISREG(st.st_mode)) {
        double mtime_ms = (double)st.st_mtim.tv_sec * 1000.0 + (double)st.st_mtim.tv_nsec / 1000000.0;
        if (mtime_ms < threshold_ms) (void)strvec_push(&matched, resolved);
    } else if (S_ISDIR(st.st_mode)) {
        scan_old_files(resolved, threshold_ms, &matched);
    } else {
        respond_error(resp, cap, "tools/call", "BAD_ARGUMENTS", "path must be file or directory");
        strvec_free(&matched);
        return;
    }

    size_t deleted = 0;
    if (!dry_run) {
        for (size_t i = 0; i < matched.count; i++) {
            if (unlink(matched.items[i]) == 0) deleted += 1;
        }
    }

    size_t len = 0;
    appendf(resp, cap, &len,
            "{\"ok\":true,\"tool\":\"filesystem.delete_older_than_days\",\"result\":{\"path\":\"%s\",\"days\":%.0f,\"dryRun\":%s,\"matchedCount\":%zu,\"deletedCount\":%zu,\"files\":[",
            path_arg, days, dry_run ? "true" : "false", matched.count, deleted);

    for (size_t i = 0; i < matched.count; i++) {
        char rel[PATH_MAX], esc[PATH_MAX * 2];
        const char *m = matched.items[i];
        if (strncmp(m, g_workspace, strlen(g_workspace)) == 0) {
            snprintf(rel, sizeof(rel), "%s", m + strlen(g_workspace) + (m[strlen(g_workspace)] == '/' ? 1 : 0));
        } else {
            snprintf(rel, sizeof(rel), "%s", m);
        }
        json_escape(rel, esc, sizeof(esc));
        appendf(resp, cap, &len, "%s\"%s\"", i == 0 ? "" : ",", esc);
    }

    appendf(resp, cap, &len, "]},\"type\":\"tools/call\",\"protocol\":\"mcp-line-json\"}");

    strvec_free(&matched);
}

/* ── RAG document retrieval ─────────────────────────────────────────── */

static const char *const g_stop_words[] = {
    "a","an","and","are","as","at","be","by","do","for","from",
    "how","i","in","is","it","of","on","or","that","the","this",
    "to","use","with",NULL};

static bool is_stop_word(const char *word)
{
    for (size_t i = 0; g_stop_words[i] != NULL; i++) {
        if (strcmp(word, g_stop_words[i]) == 0) return true;
    }
    return false;
}

static size_t rag_tokenize(const char *text, char terms[][64], size_t max_terms)
{
    size_t count = 0;
    const char *p = text;
    while (*p && count < max_terms) {
        while (*p && !(isalnum((unsigned char)*p) || *p == '_' || *p == '-')) p++;
        if (!*p) break;
        const char *start = p;
        while (*p && (isalnum((unsigned char)*p) || *p == '_' || *p == '-')) p++;
        size_t len = (size_t)(p - start);
        if (len < 2 || len >= 64) continue;
        char word[64];
        for (size_t k = 0; k < len; k++)
            word[k] = (char)tolower((unsigned char)start[k]);
        word[len] = '\0';
        if (is_stop_word(word)) continue;
        memcpy(terms[count++], word, len + 1);
    }
    return count;
}

static void rag_normalize(const char *src, char *dst, size_t cap)
{
    size_t o = 0;
    for (size_t i = 0; src[i] && o + 1 < cap; i++) {
        unsigned char c = (unsigned char)src[i];
        if (isalnum(c) || c == '_' || c == '-') {
            dst[o++] = (char)tolower(c);
        } else {
            if (o > 0 && dst[o - 1] != ' ')
                dst[o++] = ' ';
        }
    }
    dst[o] = '\0';
}

static void rag_build_corpus(void)
{
    if (g_rag_ready) return;
    g_rag_count = 0;
    DIR *root = opendir(g_workspace);
    if (!root) { g_rag_ready = true; return; }

    struct dirent *de;
    while ((de = readdir(root)) != NULL && g_rag_count < RAG_MAX_DOCS) {
        if (strncmp(de->d_name, "cmd_", 4) != 0) continue;
        char pkg_path[PATH_MAX];
        if (!build_path_parts(pkg_path, sizeof(pkg_path),
                              g_workspace, "/", de->d_name, "/pkg.json", "")) {
            continue;
        }

        char cmd_name[SMALL_BUF], summary[SMALL_BUF], long_desc[2048];
        read_pkg_field(pkg_path, "name",             cmd_name,  sizeof(cmd_name));
        read_pkg_field(pkg_path, "description",      summary,   sizeof(summary));
        read_pkg_field(pkg_path, "long_description", long_desc, sizeof(long_desc));
        if (cmd_name[0] == '\0') continue;

        char docs_path[PATH_MAX], docs[RAG_TEXT_MAX / 2];
        if (!build_path_parts(docs_path, sizeof(docs_path),
                              g_workspace, "/cmd_", cmd_name, "/docs/", "") ||
            !build_path_parts(docs_path, sizeof(docs_path),
                              docs_path, cmd_name, ".md", "", "")) {
            continue;
        }
        docs[0] = '\0';
        (void)read_file_to_buffer(docs_path, docs, sizeof(docs));

        rag_doc_t *doc = &g_rag_corpus[g_rag_count++];
        if (!copy_string(doc->command, sizeof(doc->command), cmd_name)) {
            g_rag_count--;
            continue;
        }
        if (!build_path_parts(doc->source_path, sizeof(doc->source_path),
                              "cmd_", cmd_name, "/docs/", cmd_name, ".md")) {
            g_rag_count--;
            continue;
        }
        snprintf(doc->raw_text, sizeof(doc->raw_text), "%s\n%s\n%s\n%s",
                 cmd_name, summary, long_desc, docs);
        rag_normalize(doc->raw_text, doc->text, sizeof(doc->text));
    }
    closedir(root);
    g_rag_ready = true;
}

static double rag_score(const rag_doc_t *doc, char (*terms)[64], size_t nterms)
{
    double score = 0.0;
    for (size_t i = 0; i < nterms; i++) {
        const char *found = strstr(doc->text, terms[i]);
        if (!found) continue;
        score += 1.0;
        if ((size_t)(found - doc->text) < 200) score += 0.25;
    }
    return score;
}

static int rag_hit_cmp(const void *a, const void *b)
{
    const rag_hit_t *ha = (const rag_hit_t *)a;
    const rag_hit_t *hb = (const rag_hit_t *)b;
    if (hb->score > ha->score) return 1;
    if (hb->score < ha->score) return -1;
    return strcmp(g_rag_corpus[ha->idx].command, g_rag_corpus[hb->idx].command);
}

static size_t rag_top_hits(char (*terms)[64], size_t nterms,
                            rag_hit_t *hits, size_t max_hits)
{
    size_t nhits = 0;
    for (size_t i = 0; i < g_rag_count && nhits < RAG_MAX_DOCS; i++) {
        double s = rag_score(&g_rag_corpus[i], terms, nterms);
        if (s > 0.0) {
            hits[nhits].idx   = i;
            hits[nhits].score = s;
            nhits++;
        }
    }
    qsort(hits, nhits, sizeof(rag_hit_t), rag_hit_cmp);
    if (nhits > max_hits) nhits = max_hits;
    return nhits;
}

static void rag_snippet(const char *raw_text, char (*terms)[64], size_t nterms,
                        char *out, size_t out_cap)
{
    const char *best = NULL;
    const char *p    = raw_text;
    while (*p && !best) {
        const char *line_start = p;
        const char *line_end   = strchr(p, '\n');
        if (!line_end) line_end = p + strlen(p);
        size_t line_len = (size_t)(line_end - line_start);
        if (line_len < RAG_SNIP_MAX) {
            char lc[RAG_SNIP_MAX];
            for (size_t k = 0; k < line_len; k++)
                lc[k] = (char)tolower((unsigned char)line_start[k]);
            lc[line_len] = '\0';
            for (size_t i = 0; i < nterms && !best; i++) {
                if (strstr(lc, terms[i])) best = line_start;
            }
        }
        p = (*line_end == '\n') ? line_end + 1 : line_end;
        if (!*p) break;
    }
    if (!best) {
        p = raw_text;
        while (*p == '\n') p++;
        best = p;
    }
    size_t o = 0;
    while (*best && *best != '\n' && o + 1 < out_cap && o < 220)
        out[o++] = *best++;
    out[o] = '\0';
}

static void build_rag_docs_search(const char *req, char *resp, size_t cap)
{
    char query[SMALL_BUF];
    if (!extract_json_string(req, "query", query, sizeof(query)) || query[0] == '\0') {
        respond_error(resp, cap, "tools/call", "BAD_ARGUMENTS", "query is required");
        return;
    }

    double top_k_d = 3.0;
    (void)extract_json_number(req, "topK", &top_k_d);
    size_t top_k = (top_k_d >= 1.0 && top_k_d <= 8.0) ? (size_t)top_k_d : 3;

    char terms[RAG_MAX_TERMS][64];
    size_t nterms = rag_tokenize(query, terms, RAG_MAX_TERMS);
    if (nterms == 0) {
        respond_error(resp, cap, "tools/call", "BAD_ARGUMENTS",
                      "query must include meaningful keywords");
        return;
    }

    rag_build_corpus();

    rag_hit_t hits[RAG_MAX_DOCS];
    size_t nhits = rag_top_hits(terms, nterms, hits, top_k);

    size_t len = 0;
    appendf(resp, cap, &len,
            "{\"ok\":true,\"tool\":\"rag.docs.search\",\"result\":[");
    for (size_t i = 0; i < nhits; i++) {
        const rag_doc_t *doc = &g_rag_corpus[hits[i].idx];
        char snip[RAG_SNIP_MAX];
        rag_snippet(doc->raw_text, terms, nterms, snip, sizeof(snip));
        char cmd_e[SMALL_BUF], src_e[512], snip_e[RAG_SNIP_MAX * 2];
        json_escape(doc->command,     cmd_e,  sizeof(cmd_e));
        json_escape(doc->source_path, src_e,  sizeof(src_e));
        json_escape(snip,             snip_e, sizeof(snip_e));
        appendf(resp, cap, &len,
                "%s{\"command\":\"%s\",\"sourcePath\":\"%s\",\"score\":%.2f,\"snippet\":\"%s\"}",
                i == 0 ? "" : ",", cmd_e, src_e, hits[i].score, snip_e);
    }
    appendf(resp, cap, &len, "],\"type\":\"tools/call\",\"protocol\":\"mcp-line-json\"}");
}

static void build_rag_command_recommend(const char *req, char *resp, size_t cap)
{
    char query[SMALL_BUF];
    if (!extract_json_string(req, "query", query, sizeof(query)) || query[0] == '\0') {
        respond_error(resp, cap, "tools/call", "BAD_ARGUMENTS", "query is required");
        return;
    }

    char terms[RAG_MAX_TERMS][64];
    size_t nterms = rag_tokenize(query, terms, RAG_MAX_TERMS);
    if (nterms == 0) {
        respond_error(resp, cap, "tools/call", "BAD_ARGUMENTS",
                      "query must include meaningful keywords");
        return;
    }

    rag_build_corpus();

    rag_hit_t hits[RAG_MAX_DOCS];
    size_t nhits = rag_top_hits(terms, nterms, hits, 3);

    /* Apply grounding heuristics (mirrors Node bridge logic) */
    char q_lower[SMALL_BUF];
    size_t qlen = strlen(query);
    if (qlen >= sizeof(q_lower)) qlen = sizeof(q_lower) - 1;
    for (size_t i = 0; i < qlen; i++)
        q_lower[i] = (char)tolower((unsigned char)query[i]);
    q_lower[qlen] = '\0';

    char top_cmd[SMALL_BUF * 2];
    if (strstr(q_lower, "working directory") || strstr(q_lower, "where am i")) {
        snprintf(top_cmd, sizeof(top_cmd), "pwd");
    } else if (strstr(q_lower, "list") && strstr(q_lower, "file")) {
        snprintf(top_cmd, sizeof(top_cmd), "ls");
    } else if (strstr(q_lower, "show") && strstr(q_lower, "first")) {
        snprintf(top_cmd, sizeof(top_cmd), "head");
    } else if (nhits > 0) {
        snprintf(top_cmd, sizeof(top_cmd), "%s --help",
                 g_rag_corpus[hits[0].idx].command);
    } else {
        snprintf(top_cmd, sizeof(top_cmd), "help");
    }

    char rationale[SMALL_BUF * 2];
    if (nhits > 0) {
        snprintf(rationale, sizeof(rationale), "Grounded recommendation from %s",
                 g_rag_corpus[hits[0].idx].source_path);
    } else {
        snprintf(rationale, sizeof(rationale),
                 "No command docs matched strongly; fallback recommendation provided.");
    }

    char cmd_e[SMALL_BUF * 2], rat_e[SMALL_BUF * 4];
    json_escape(top_cmd,   cmd_e, sizeof(cmd_e));
    json_escape(rationale, rat_e, sizeof(rat_e));

    size_t len = 0;
    appendf(resp, cap, &len,
            "{\"ok\":true,\"tool\":\"rag.command.recommend\",\"result\":{"
            "\"command\":\"%s\",\"rationale\":\"%s\",\"citations\":[",
            cmd_e, rat_e);
    for (size_t i = 0; i < nhits; i++) {
        char cit_e[512];
        json_escape(g_rag_corpus[hits[i].idx].source_path, cit_e, sizeof(cit_e));
        appendf(resp, cap, &len, "%s\"%s\"", i == 0 ? "" : ",", cit_e);
    }
    appendf(resp, cap, &len,
            "]},\"type\":\"tools/call\",\"protocol\":\"mcp-line-json\"}");
}

static bool post_agent_command_proxy(const char *query, char *resp, size_t cap)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return false;

    struct timeval timeout;
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(3000);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
        return false;
    }

    char query_esc[SMALL_BUF * 2];
    json_escape(query, query_esc, sizeof(query_esc));

    char body[SMALL_BUF * 2];
    int body_len = snprintf(body, sizeof(body), "{\"query\":\"%s\"}", query_esc);
    if (body_len < 0 || body_len >= (int)sizeof(body)) {
        close(fd);
        return false;
    }

    char reqbuf[SMALL_BUF * 4];
    int req_len = snprintf(reqbuf, sizeof(reqbuf),
                           "POST /agent/command HTTP/1.1\r\n"
                           "Host: 127.0.0.1:3000\r\n"
                           "Content-Type: application/json\r\n"
                           "Content-Length: %d\r\n"
                           "Connection: close\r\n\r\n"
                           "%s",
                           body_len, body);
    if (req_len < 0 || req_len >= (int)sizeof(reqbuf)) {
        close(fd);
        return false;
    }

    ssize_t sent = send(fd, reqbuf, (size_t)req_len, 0);
    if (sent != req_len) {
        close(fd);
        return false;
    }

    char raw[RESP_MAX];
    size_t used = 0;
    for (;;) {
        ssize_t n = recv(fd, raw + used, sizeof(raw) - used - 1, 0);
        if (n <= 0) break;
        used += (size_t)n;
        if (used + 1 >= sizeof(raw)) break;
    }
    close(fd);
    raw[used] = '\0';

    if (strncmp(raw, "HTTP/1.1 200", 12) != 0 && strncmp(raw, "HTTP/1.0 200", 12) != 0) {
        return false;
    }

    char *body_start = strstr(raw, "\r\n\r\n");
    if (!body_start) return false;
    body_start += 4;

    if (!copy_string(resp, cap, body_start)) return false;
    return strstr(resp, "\"ok\":true") != NULL;
}

static void build_agent_command_plan_local(const char *query, char *resp, size_t cap)
{
    char terms[RAG_MAX_TERMS][64];
    size_t nterms = rag_tokenize(query, terms, RAG_MAX_TERMS);
    if (nterms == 0) {
        respond_error(resp, cap, "tools/call", "BAD_ARGUMENTS",
                      "query must include meaningful keywords");
        return;
    }

    rag_build_corpus();

    rag_hit_t hits[RAG_MAX_DOCS];
    size_t nhits = rag_top_hits(terms, nterms, hits, 3);

    char q_lower[SMALL_BUF];
    size_t qlen = strlen(query);
    if (qlen >= sizeof(q_lower)) qlen = sizeof(q_lower) - 1;
    for (size_t i = 0; i < qlen; i++)
        q_lower[i] = (char)tolower((unsigned char)query[i]);
    q_lower[qlen] = '\0';

    char command[SMALL_BUF * 2];
    if (strstr(q_lower, "working directory") || strstr(q_lower, "where am i")) {
        snprintf(command, sizeof(command), "pwd");
    } else if (strstr(q_lower, "list") && strstr(q_lower, "file")) {
        snprintf(command, sizeof(command), "ls");
    } else if (strstr(q_lower, "show") && strstr(q_lower, "first")) {
        snprintf(command, sizeof(command), "head");
    } else if (nhits > 0) {
        snprintf(command, sizeof(command), "%s --help", g_rag_corpus[hits[0].idx].command);
    } else {
        snprintf(command, sizeof(command), "help");
    }

    char rationale[SMALL_BUF * 4];
    if (nhits > 0) {
        snprintf(rationale, sizeof(rationale),
                 "Grounded recommendation from %s Fallback path used because the Node/OpenAI agent service is unavailable.",
                 g_rag_corpus[hits[0].idx].source_path);
    } else {
        snprintf(rationale, sizeof(rationale),
                 "Fallback path used because the Node/OpenAI agent service is unavailable.");
    }

    char cmd_e[SMALL_BUF * 2], rat_e[SMALL_BUF * 8];
    json_escape(command, cmd_e, sizeof(cmd_e));
    json_escape(rationale, rat_e, sizeof(rat_e));

    size_t len = 0;
    appendf(resp, cap, &len,
            "{\"ok\":true,\"tool\":\"agent.command.plan\",\"result\":{"
            "\"command\":\"%s\",\"rationale\":\"%s\",\"citations\":[",
            cmd_e, rat_e);
    for (size_t i = 0; i < nhits; i++) {
        char cit_e[512];
        json_escape(g_rag_corpus[hits[i].idx].source_path, cit_e, sizeof(cit_e));
        appendf(resp, cap, &len, "%s\"%s\"", i == 0 ? "" : ",", cit_e);
    }
    appendf(resp, cap, &len, "],\"trace\":["
            "\"Tokenized the natural-language task.\","
            "\"Retrieved relevant CoreShell docs from the local corpus.\","
            "\"Selected the top grounded command recommendation.\"],"
            "\"provider\":\"c-local-rag-fallback\","
            "\"model\":\"c-local-rag-fallback\"},"
            "\"type\":\"tools/call\",\"protocol\":\"mcp-line-json\"}");
}

static void build_agent_command_plan(const char *req, char *resp, size_t cap)
{
    char query[SMALL_BUF];
    if (!extract_json_string(req, "query", query, sizeof(query)) || query[0] == '\0') {
        respond_error(resp, cap, "tools/call", "BAD_ARGUMENTS", "query is required");
        return;
    }

    if (post_agent_command_proxy(query, resp, cap)) {
        return;
    }

    build_agent_command_plan_local(query, resp, cap);
}

static void handle_tools_call(const char *req, char *resp, size_t cap)
{
    char tool[SMALL_BUF];
    if (!extract_json_string(req, "tool", tool, sizeof(tool))) {
        if (!extract_json_string(req, "name", tool, sizeof(tool))) {
            respond_error(resp, cap, "tools/call", "BAD_ARGUMENTS", "tool is required");
            return;
        }
    }

    if (strcmp(tool, "registry.packages.list") == 0) {
        build_registry_packages_list(resp, cap);
        return;
    }
    if (strcmp(tool, "registry.package.lookup") == 0) {
        build_registry_package_lookup(req, resp, cap);
        return;
    }
    if (strcmp(tool, "shell.commands.list") == 0) {
        build_shell_commands_list(resp, cap);
        return;
    }
    if (strcmp(tool, "shell.command.help") == 0) {
        build_shell_command_help(req, resp, cap);
        return;
    }
    if (strcmp(tool, "shell.command.run") == 0) {
        build_shell_command_run(req, resp, cap);
        return;
    }
    if (strcmp(tool, "filesystem.delete_older_than_days") == 0) {
        build_delete_older_than_days(req, resp, cap);
        return;
    }
    if (strcmp(tool, "rag.docs.search") == 0) {
        build_rag_docs_search(req, resp, cap);
        return;
    }
    if (strcmp(tool, "rag.command.recommend") == 0) {
        build_rag_command_recommend(req, resp, cap);
        return;
    }
    if (strcmp(tool, "agent.command.plan") == 0) {
        build_agent_command_plan(req, resp, cap);
        return;
    }

    respond_error(resp, cap, "tools/call", "TOOL_NOT_FOUND", "Tool not found");
}

static void handle_request(const char *req, char *resp, size_t cap)
{
    if (strstr(req, "\"type\":\"tools/list\"") || strstr(req, "\"method\":\"tools/list\"")) {
        build_tools_list_response(resp, cap);
        return;
    }
    if (strstr(req, "\"type\":\"tools/call\"") || strstr(req, "\"method\":\"tools/call\"")) {
        handle_tools_call(req, resp, cap);
        return;
    }

    respond_error(resp, cap, "unknown", "UNKNOWN_METHOD", "Expected tools/list or tools/call");
}

static ssize_t recv_line(int fd, char *buf, size_t cap)
{
    size_t len = 0;
    while (len + 1 < cap) {
        char c;
        ssize_t n = recv(fd, &c, 1, 0);
        if (n <= 0) break;
        if (c == '\n') break;
        buf[len++] = c;
    }
    buf[len] = '\0';
    return (ssize_t)len;
}

int main(void)
{
    if (!getcwd(g_workspace, sizeof(g_workspace))) {
        perror("getcwd");
        return 1;
    }

    mkdir("artifacts", 0755);

    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) {
        perror("socket");
        return 1;
    }

    int yes = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(MCP_PORT);

    if (bind(sfd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        perror("bind");
        close(sfd);
        return 1;
    }

    if (listen(sfd, BACKLOG) != 0) {
        perror("listen");
        close(sfd);
        return 1;
    }

    printf("CoreShell native C MCP server listening on 127.0.0.1:%d\n", MCP_PORT);
    fflush(stdout);

    for (;;) {
        struct sockaddr_in cli;
        socklen_t cli_len = sizeof(cli);
        int cfd = accept(sfd, (struct sockaddr *)&cli, &cli_len);
        if (cfd < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }

        char req[REQ_MAX];
        ssize_t n = recv_line(cfd, req, sizeof(req));
        if (n > 0) {
            char resp[RESP_MAX];
            handle_request(req, resp, sizeof(resp));
            log_call(req, resp);
            send(cfd, resp, strlen(resp), 0);
            send(cfd, "\n", 1, 0);
        }

        close(cfd);
    }

    close(sfd);
    return 0;
}
