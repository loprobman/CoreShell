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
#include <time.h>
#include <unistd.h>

#define MCP_PORT 9000
#define BACKLOG 16
#define REQ_MAX 65536
#define RESP_MAX 262144
#define SMALL_BUF 1024
#define LOG_PATH "artifacts/mcp_calls.log"
#define MAX_SCAN_FILES 256

static char g_workspace[PATH_MAX];

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
             "{\"name\":\"filesystem.delete_older_than_days\",\"description\":\"Delete files older than N days under a workspace path\"}"
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
        snprintf(pkg_path, sizeof(pkg_path), "%s/%s/pkg.json", g_workspace, de->d_name);

        char name[SMALL_BUF], version[SMALL_BUF];
        read_pkg_field(pkg_path, "name", name, sizeof(name));
        read_pkg_field(pkg_path, "version", version, sizeof(version));
        if (name[0] == '\0' || version[0] == '\0') continue;

        char name_esc[SMALL_BUF], ver_esc[SMALL_BUF], url_esc[SMALL_BUF * 2];
        char url[SMALL_BUF * 2];
        snprintf(url, sizeof(url), "http://localhost:3000/downloads/%s-%s.tar.gz", name, version);
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
    snprintf(pkg_path, sizeof(pkg_path), "%s/cmd_%s/pkg.json", g_workspace, name);

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
    snprintf(url, sizeof(url), "http://localhost:3000/downloads/%s-%s.tar.gz", found_name, version);
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
        snprintf(pkg_path, sizeof(pkg_path), "%s/%s/pkg.json", g_workspace, de->d_name);

        char name[SMALL_BUF], summary[SMALL_BUF], long_desc[2048];
        read_pkg_field(pkg_path, "name", name, sizeof(name));
        read_pkg_field(pkg_path, "description", summary, sizeof(summary));
        read_pkg_field(pkg_path, "long_description", long_desc, sizeof(long_desc));
        if (name[0] == '\0') continue;

        char name_esc[SMALL_BUF], sum_esc[SMALL_BUF], long_esc[2048], docs_esc[SMALL_BUF * 2];
        char docs[SMALL_BUF * 2];
        snprintf(docs, sizeof(docs), "cmd_%s/docs/%s.md", name, name);
        json_escape(name, name_esc, sizeof(name_esc));
        json_escape(summary, sum_esc, sizeof(sum_esc));
        json_escape(long_desc, long_esc, sizeof(long_esc));
        json_escape(docs, docs_esc, sizeof(docs_esc));

        char row[4096];
        snprintf(row, sizeof(row),
                 "{\"name\":\"%s\",\"summary\":\"%s\",\"longDescription\":\"%s\",\"docsPath\":\"%s\"}",
                 name_esc, sum_esc, long_esc, docs_esc);
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
    snprintf(pkg_path, sizeof(pkg_path), "%s/cmd_%s/pkg.json", g_workspace, name);
    snprintf(docs_path, sizeof(docs_path), "%s/cmd_%s/docs/%s.md", g_workspace, name, name);

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
