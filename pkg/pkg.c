/*
 * pkg.c - CoreShell package manager (standalone program)
 *
 * Subcommands: build, install, list, remove
 *
 * Package format:  <name>-<version>.tar.gz
 *   └── pkg.json          metadata (name, version, description, files[], docs[])
 *   └── bin/<name>        compiled executable(s)
 *   └── docs/             documentation files
 *
 * Install layout:
 *   ~/.CoreShell/pkgs/<name>-<version>/   extracted package
 *   ~/.CoreShell/bin/<name>               symlink to executable
 *   ~/.CoreShell/pkgdb.txt                installed package database
 *
 * Build: gcc -Wall -Wextra -g -std=c99 -D_POSIX_C_SOURCE=200809L -o pkg pkg/pkg.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <limits.h>
#include <dirent.h>
#include <fcntl.h>

/* ── constants ─────────────────────────────────────────────────────────── */

#define CS_DIR   ".CoreShell"
#define CS_PKGS  ".CoreShell/pkgs"
#define CS_BIN   ".CoreShell/bin"
#define CS_PKGDB ".CoreShell/pkgdb.txt"

#define MAX_FILES    32
#define MAX_PKGFILE  65536

/* ── data ──────────────────────────────────────────────────────────────── */

typedef struct
{
    char name[256];
    char version[64];
    char description[512];
    char files[MAX_FILES][256];
    int  nfiles;
} pkg_meta_t;

/* ── path helpers ──────────────────────────────────────────────────────── */

static const char *home_dir(void)
{
    const char *h = getenv("HOME");
    if (!h || h[0] == '\0')
    {
        fprintf(stderr, "pkg: $HOME is not set\n");
        exit(EXIT_FAILURE);
    }
    return h;
}

/* Build an absolute path under $HOME/<sub> */
static void cs_path(char *out, size_t sz, const char *sub)
{
    snprintf(out, sz, "%s/%s", home_dir(), sub);
}

/* Build the install directory path for a package */
static void pkg_install_dir(char *out, size_t sz,
                             const char *name, const char *version)
{
    char base[PATH_MAX];
    cs_path(base, sizeof(base), CS_PKGS);
    snprintf(out, sz, "%s/%s-%s", base, name, version);
}

/* ── mkdir -p ──────────────────────────────────────────────────────────── */

static int mkdirs(const char *path)
{
    char tmp[PATH_MAX];
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    for (char *p = tmp + 1; *p; p++)
    {
        if (*p == '/')
        {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
            {
                perror(tmp);
                return 1;
            }
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
    {
        perror(tmp);
        return 1;
    }
    return 0;
}

/* Ensure ~/.CoreShell/{pkgs,bin} exist */
static void ensure_cs_dirs(void)
{
    char p[PATH_MAX];
    cs_path(p, sizeof(p), CS_DIR);  mkdirs(p);
    cs_path(p, sizeof(p), CS_PKGS); mkdirs(p);
    cs_path(p, sizeof(p), CS_BIN);  mkdirs(p);
}

/* ── fork/exec helper ──────────────────────────────────────────────────── */

/* Run a NULL-terminated argv array; returns the process exit status */
static int run_cmd(char *const argv[])
{
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return 1; }

    if (pid == 0)
    {
        execvp(argv[0], argv);
        perror(argv[0]);
        _exit(127);
    }

    int status;
    if (waitpid(pid, &status, 0) < 0) { perror("waitpid"); return 1; }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}

/*
 * Run argv, capture stdout into buf[bufsz].  stderr is discarded.
 * Returns bytes written to buf (>= 0), or -1 on fork/read error.
 */
static ssize_t capture_cmd(char *const argv[], char *buf, size_t bufsz)
{
    int pipe_fd[2];
    if (pipe(pipe_fd) < 0) { perror("pipe"); return -1; }

    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork");
        close(pipe_fd[0]); close(pipe_fd[1]);
        return -1;
    }

    if (pid == 0)
    {
        close(pipe_fd[0]);
        if (dup2(pipe_fd[1], STDOUT_FILENO) < 0) _exit(127);
        /* Silence stderr so it doesn't pollute the capture */
        int dn = open("/dev/null", O_WRONLY);
        if (dn >= 0) { dup2(dn, STDERR_FILENO); close(dn); }
        close(pipe_fd[1]);
        execvp(argv[0], argv);
        _exit(127);
    }

    close(pipe_fd[1]);
    ssize_t total = 0, n;
    while (total < (ssize_t)(bufsz - 1) &&
           (n = read(pipe_fd[0], buf + total,
                     bufsz - 1 - (size_t)total)) > 0)
        total += n;
    buf[total] = '\0';
    close(pipe_fd[0]);

    int status;
    waitpid(pid, &status, 0);
    return total;
}

/* ── JSON parsing (minimal, no library) ────────────────────────────────── */

/* Read entire file content into a heap buffer. Caller must free(). */
static char *slurp_file(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); return NULL; }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);

    if (sz <= 0 || sz > MAX_PKGFILE)
    {
        fprintf(stderr, "pkg: %s: file too large or empty\n", path);
        fclose(f);
        return NULL;
    }

    char *buf = malloc((size_t)sz + 1);
    if (!buf) { perror("malloc"); fclose(f); return NULL; }

    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz)
    {
        perror(path);
        free(buf);
        fclose(f);
        return NULL;
    }
    buf[sz] = '\0';
    fclose(f);
    return buf;
}

/* Extract a JSON string value for "key". Returns 1 on success. */
static int json_str(const char *json, const char *key,
                    char *out, size_t outsz)
{
    char needle[300];
    snprintf(needle, sizeof(needle), "\"%s\"", key);

    const char *p = strstr(json, needle);
    if (!p) return 0;
    p += strlen(needle);

    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == ':') p++;
    if (*p != '"') return 0;
    p++;

    size_t i = 0;
    while (*p && *p != '"' && i < outsz - 1)
    {
        if (*p == '\\' && *(p + 1)) p++;  /* skip escape prefix */
        out[i++] = *p++;
    }
    out[i] = '\0';
    return 1;
}

/* Extract a JSON array of strings for "key". Returns entry count. */
static int json_arr(const char *json, const char *key,
                    char out[][256], int maxn)
{
    char needle[300];
    snprintf(needle, sizeof(needle), "\"%s\"", key);

    const char *p = strstr(json, needle);
    if (!p) return 0;
    p += strlen(needle);

    while (*p && *p != '[') p++;
    if (*p != '[') return 0;
    p++;

    int n = 0;
    while (*p && *p != ']' && n < maxn)
    {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == ',') p++;
        if (*p != '"') { if (*p) p++; continue; }
        p++;
        size_t i = 0;
        while (*p && *p != '"' && i < 255)
        {
            if (*p == '\\' && *(p + 1)) p++;
            out[n][i++] = *p++;
        }
        out[n][i] = '\0';
        if (i > 0) n++;
        if (*p == '"') p++;
    }
    return n;
}

/* Parse pkg.json at path into meta. Returns 1 on success. */
static int parse_pkg_json(const char *path, pkg_meta_t *m)
{
    char *buf = slurp_file(path);
    if (!buf) return 0;

    memset(m, 0, sizeof(*m));
    int ok = json_str(buf, "name",    m->name,        sizeof(m->name))
           & json_str(buf, "version", m->version,     sizeof(m->version));
    json_str(buf, "description", m->description, sizeof(m->description));
    m->nfiles = json_arr(buf, "files", m->files, MAX_FILES);
    free(buf);

    if (!ok || m->name[0] == '\0' || m->version[0] == '\0')
    {
        fprintf(stderr, "pkg: %s: missing required fields (name, version)\n", path);
        return 0;
    }
    return 1;
}

/* ── pkgdb helpers ─────────────────────────────────────────────────────── */

static void pkgdb_path(char *out, size_t sz)
{
    cs_path(out, sz, CS_PKGDB);
}

/* Append "<name> <version>\n" to pkgdb.txt */
static void pkgdb_add(const char *name, const char *version)
{
    char db[PATH_MAX];
    pkgdb_path(db, sizeof(db));
    FILE *f = fopen(db, "a");
    if (!f) { perror(db); return; }
    fprintf(f, "%s %s\n", name, version);
    fclose(f);
}

/* Look up name in pkgdb; copy version into out. Returns 1 if found. */
static int pkgdb_find(const char *name, char *version, size_t vsz)
{
    char db[PATH_MAX];
    pkgdb_path(db, sizeof(db));
    FILE *f = fopen(db, "r");
    if (!f) return 0;

    char line[512];
    while (fgets(line, sizeof(line), f))
    {
        char n[256], v[64];
        if (sscanf(line, "%255s %63s", n, v) == 2 && strcmp(n, name) == 0)
        {
            strncpy(version, v, vsz - 1);
            version[vsz - 1] = '\0';
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

/* Rewrite pkgdb.txt omitting any line for name */
static void pkgdb_remove(const char *name)
{
    char db[PATH_MAX], tmp[PATH_MAX + 8];
    pkgdb_path(db, sizeof(db));
    snprintf(tmp, sizeof(tmp), "%s.tmp", db);

    FILE *fin  = fopen(db, "r");
    if (!fin) return;
    FILE *fout = fopen(tmp, "w");
    if (!fout) { fclose(fin); return; }

    char line[512];
    while (fgets(line, sizeof(line), fin))
    {
        char n[256], v[64];
        if (sscanf(line, "%255s %63s", n, v) == 2 && strcmp(n, name) != 0)
            fputs(line, fout);
    }
    fclose(fin);
    fclose(fout);
    rename(tmp, db);
}

/* ── recursive remove ──────────────────────────────────────────────────── */

static int rm_rf(const char *path)
{
    struct stat st;
    if (lstat(path, &st) != 0) return 0;  /* already gone */

    if (S_ISDIR(st.st_mode))
    {
        DIR *d = opendir(path);
        if (!d) { perror(path); return 1; }

        struct dirent *e;
        while ((e = readdir(d)))
        {
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
            char child[PATH_MAX];
            snprintf(child, sizeof(child), "%s/%s", path, e->d_name);
            rm_rf(child);
        }
        closedir(d);

        if (rmdir(path) != 0) { perror(path); return 1; }
    }
    else
    {
        if (unlink(path) != 0) { perror(path); return 1; }
    }
    return 0;
}

/* ── subcommands ───────────────────────────────────────────────────────── */

/*
 * pkg build <src-dir> <output.tar.gz>
 *
 * Packages the contents of <src-dir> into a .tar.gz archive.
 * The directory must contain a pkg.json at its root.
 * Uses a staging directory so that files[] entries (e.g. bin/<name>) that
 * live outside the source tree are included in the archive at their correct
 * relative paths, matching what pkg install expects.
 */
static int cmd_build(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: pkg build <src-dir> <output.tar.gz>\n");
        return 1;
    }

    const char *srcdir = argv[0];
    const char *output = argv[1];

    /* Verify pkg.json exists in srcdir before packaging */
    char json_check[PATH_MAX];
    snprintf(json_check, sizeof(json_check), "%s/pkg.json", srcdir);
    if (access(json_check, R_OK) != 0)
    {
        fprintf(stderr, "pkg build: no pkg.json found in '%s'\n", srcdir);
        return 1;
    }

    /* Parse pkg.json to find files[] entries */
    pkg_meta_t meta;
    if (!parse_pkg_json(json_check, &meta)) return 1;

    /* Stage: create a temp dir, copy srcdir contents, then add files[] */
    char stagedir[] = "/tmp/pkgbuild_XXXXXX";
    if (mkdtemp(stagedir) == NULL) { perror("mkdtemp"); return 1; }

    /* Copy module source tree into staging root */
    char srcdirslash[PATH_MAX + 2];
    snprintf(srcdirslash, sizeof(srcdirslash), "%s/.", srcdir);
    char *cp_args[] = { "cp", "-r", srcdirslash, stagedir, NULL };
    if (run_cmd(cp_args) != 0)
    {
        fprintf(stderr, "pkg build: failed to copy %s to staging\n", srcdir);
        rm_rf(stagedir);
        return 1;
    }

    /* Copy each files[] entry from the project root into staging, preserving
     * its relative path so pkg install finds it at the expected location. */
    for (int i = 0; i < meta.nfiles; i++)
    {
        const char *rel = meta.files[i];   /* e.g. "bin/echo" */

        /* Ensure the parent directory exists in staging */
        char parent[PATH_MAX + 256];
        snprintf(parent, sizeof(parent), "%s/%s", stagedir, rel);
        char *slash = strrchr(parent, '/');
        if (slash && slash != parent)
        {
            *slash = '\0';
            mkdirs(parent);
            *slash = '/';
        }

        char dest[PATH_MAX + 256];
        snprintf(dest, sizeof(dest), "%s/%s", stagedir, rel);

        /* Skip if the source file doesn't exist (not yet compiled) */
        if (access(rel, F_OK) != 0)
        {
            fprintf(stderr,
                    "pkg build: warning: files[] entry '%s' not found "
                    "(run pkg compile first)\n", rel);
            continue;
        }

        char *cp2[] = { "cp", (char *)rel, dest, NULL };
        if (run_cmd(cp2) != 0)
        {
            fprintf(stderr, "pkg build: failed to copy %s\n", rel);
            rm_rf(stagedir);
            return 1;
        }
    }

    /* Resolve output to absolute path before chdir operations in tar */
    char abs_output[PATH_MAX * 2];
    if (output[0] == '/')
        snprintf(abs_output, sizeof(abs_output), "%s", output);
    else
    {
        char cwd[PATH_MAX];
        if (!getcwd(cwd, sizeof(cwd))) { perror("getcwd"); rm_rf(stagedir); return 1; }
        snprintf(abs_output, sizeof(abs_output), "%s/%s", cwd, output);
    }

    printf("Building: %s from %s/ (staged)\n", output, srcdir);
    char *tar_args[] = { "tar", "-czf", abs_output, "-C", stagedir, ".", NULL };
    int ret = run_cmd(tar_args);
    rm_rf(stagedir);
    if (ret == 0)
        printf("Created: %s\n", output);
    return ret;
}

/*
 * pkg install <archive.tar.gz>
 *
 * 1. Extract archive to a temp dir and read pkg.json.
 * 2. Create ~/.CoreShell/pkgs/<name>-<version>/ and extract there.
 * 3. For each entry in files[], symlink into ~/.CoreShell/bin/.
 * 4. Record the package in ~/.CoreShell/pkgdb.txt.
 */
static int cmd_install(int argc, char **argv)
{
    if (argc < 1)
    {
        fprintf(stderr, "Usage: pkg install <archive.tar.gz>\n");
        return 1;
    }

    /* Resolve archive to absolute path before chdir operations */
    char archive[PATH_MAX];
    if (realpath(argv[0], archive) == NULL)
    {
        perror(argv[0]);
        return 1;
    }

    /* Step 1: extract to temp dir to read pkg.json */
    char tmpdir[] = "/tmp/pkg_XXXXXX";
    if (mkdtemp(tmpdir) == NULL) { perror("mkdtemp"); return 1; }

    char *extract[] = { "tar", "-xzf", archive, "-C", tmpdir, NULL };
    if (run_cmd(extract) != 0)
    {
        fprintf(stderr, "pkg: failed to extract %s\n", archive);
        rm_rf(tmpdir);
        return 1;
    }

    /* Step 2: parse pkg.json */
    char json_path[PATH_MAX + 16];
    snprintf(json_path, sizeof(json_path), "%s/pkg.json", tmpdir);

    pkg_meta_t meta;
    if (!parse_pkg_json(json_path, &meta))
    {
        rm_rf(tmpdir);
        return 1;
    }
    rm_rf(tmpdir);

    printf("Installing %s %s...\n", meta.name, meta.version);

    /* Step 3: create install dir and extract archive there */
    ensure_cs_dirs();

    char instdir[PATH_MAX];
    pkg_install_dir(instdir, sizeof(instdir), meta.name, meta.version);
    if (mkdirs(instdir) != 0) return 1;

    char *install[] = { "tar", "-xzf", archive, "-C", instdir, NULL };
    if (run_cmd(install) != 0)
    {
        fprintf(stderr, "pkg: failed to install into %s\n", instdir);
        rm_rf(instdir);
        return 1;
    }

    /* Step 4: symlink each executable into ~/.CoreShell/bin/ */
    char bindir[PATH_MAX];
    cs_path(bindir, sizeof(bindir), CS_BIN);

    for (int i = 0; i < meta.nfiles; i++)
    {
        char srcpath[PATH_MAX + 256];
        snprintf(srcpath, sizeof(srcpath), "%s/%s", instdir, meta.files[i]);

        /* chmod +x the installed binary */
        chmod(srcpath, 0755);

        /* basename of the files[] entry */
        const char *bname = strrchr(meta.files[i], '/');
        bname = bname ? bname + 1 : meta.files[i];

        char linkpath[PATH_MAX + 256];
        snprintf(linkpath, sizeof(linkpath), "%s/%s", bindir, bname);

        unlink(linkpath);  /* remove stale symlink if present */
        if (symlink(srcpath, linkpath) == 0)
            printf("  installed: %s\n", linkpath);
        else
            perror(linkpath);
    }

    /* Step 5: record in pkgdb */
    pkgdb_add(meta.name, meta.version);
    printf("Package %s %s installed successfully.\n", meta.name, meta.version);
    return 0;
}

/*
 * pkg list
 *
 * Prints all packages recorded in ~/.CoreShell/pkgdb.txt.
 */
static int cmd_list(void)
{
    char db[PATH_MAX];
    pkgdb_path(db, sizeof(db));

    FILE *f = fopen(db, "r");
    if (!f)
    {
        printf("No packages installed.\n");
        return 0;
    }

    printf("Installed packages:\n");
    char line[512];
    int  count = 0;
    while (fgets(line, sizeof(line), f))
    {
        char n[256], v[64];
        if (sscanf(line, "%255s %63s", n, v) == 2)
        {
            printf("  %-20s %s\n", n, v);
            count++;
        }
    }
    fclose(f);
    if (count == 0) printf("  (none)\n");
    return 0;
}

/*
 * pkg remove <name>
 *
 * 1. Look up version in pkgdb.txt.
 * 2. Read pkg.json from the install dir to find which symlinks to remove.
 * 3. Remove symlinks from ~/.CoreShell/bin/.
 * 4. Remove the install dir tree.
 * 5. Update pkgdb.txt.
 */
static int cmd_remove(int argc, char **argv)
{
    if (argc < 1)
    {
        fprintf(stderr, "Usage: pkg remove <name>\n");
        return 1;
    }

    const char *name = argv[0];
    char version[64];

    if (!pkgdb_find(name, version, sizeof(version)))
    {
        fprintf(stderr, "pkg: '%s' is not installed\n", name);
        return 1;
    }

    char instdir[PATH_MAX];
    pkg_install_dir(instdir, sizeof(instdir), name, version);

    /* Remove symlinks recorded in pkg.json */
    char json_path[PATH_MAX + 16];
    snprintf(json_path, sizeof(json_path), "%s/pkg.json", instdir);

    char bindir[PATH_MAX];
    cs_path(bindir, sizeof(bindir), CS_BIN);

    pkg_meta_t meta;
    if (parse_pkg_json(json_path, &meta))
    {
        for (int i = 0; i < meta.nfiles; i++)
        {
            const char *bname = strrchr(meta.files[i], '/');
            bname = bname ? bname + 1 : meta.files[i];

            char linkpath[PATH_MAX + 256];
            snprintf(linkpath, sizeof(linkpath), "%s/%s", bindir, bname);
            if (unlink(linkpath) == 0)
                printf("  removed: %s\n", linkpath);
        }
    }

    /* Remove install directory */
    if (rm_rf(instdir) == 0)
        printf("Removed %s %s\n", name, version);

    /* Update pkgdb */
    pkgdb_remove(name);
    return 0;
}

/* ── compile helpers ────────────────────────────────────────────────────── */

/* Collect all .c files directly inside dir (non-recursive). Returns count. */
static int collect_sources(const char *dir, char srcs[][PATH_MAX], int maxn)
{
    DIR *d = opendir(dir);
    if (!d) return 0;

    int n = 0;
    struct dirent *e;
    while ((e = readdir(d)) && n < maxn)
    {
        size_t len = strlen(e->d_name);
        if (len < 3) continue;
        if (strcmp(e->d_name + len - 2, ".c") != 0) continue;
        snprintf(srcs[n], PATH_MAX, "%s/%s", dir, e->d_name);
        n++;
    }
    closedir(d);
    return n;
}

/*
 * Write a minimal main() wrapper to /tmp/pkgcompile_<name>_main.c.
 * The wrapper:
 *   - Provides a single-command registry (register_command stores the spec).
 *   - Implements --help-json using real cmd_spec_t data from the module.
 *   - Delegates all other invocations to <name>_run(argc, argv).
 *   - Stubs cmd_json_str / cmd_print_help_json used by the run() function.
 */
static int write_main_wrapper(const char *name, const char *header_rel,
                               char *out_path, size_t outsz)
{
    snprintf(out_path, outsz, "/tmp/pkgcompile_%s_main.c", name);
    FILE *f = fopen(out_path, "w");
    if (!f) { perror(out_path); return 0; }

    fprintf(f, "/* Auto-generated by pkg compile — do not edit */\n");
    fprintf(f, "#include <stdio.h>\n");
    fprintf(f, "#include <string.h>\n");
    fprintf(f, "#include \"cmd_spec/cmd_spec.h\"\n");
    fprintf(f, "#include \"cmd_registry/cmd_registry.h\"\n");
    fprintf(f, "#include \"%s\"\n\n", header_rel);

    /* Single-command registry: stores the first registered spec */
    fprintf(f, "static const cmd_spec_t *_reg = NULL;\n");
    fprintf(f, "void register_command(const cmd_spec_t *s) { _reg = s; }\n");
    fprintf(f, "const cmd_spec_t *find_command(const char *n) { (void)n; return _reg; }\n");
    fprintf(f, "void for_each_command(void (*cb)(const cmd_spec_t *, void *), void *d)\n");
    fprintf(f, "  { if (_reg && cb) cb(_reg, d); }\n");
    fprintf(f, "void register_all_builtin_commands(void) {}\n\n");

    /* JSON string helpers needed by the command's run() */
    fprintf(f, "void cmd_json_str(FILE *out, const char *s) {\n");
    fprintf(f, "  if (!s) { fputs(\"null\", out); return; }\n");
    fprintf(f, "  fputc('\"', out);\n");
    fprintf(f, "  for (; *s; s++) {\n");
    fprintf(f, "    if (*s == '\"') fputs(\"\\\\\\\"\", out);\n");
    fprintf(f, "    else if (*s == '\\\\') fputs(\"\\\\\\\\\", out);\n");
    fprintf(f, "    else if (*s == '\\n') fputs(\"\\\\n\", out);\n");
    fprintf(f, "    else fputc(*s, out); }\n");
    fprintf(f, "  fputc('\"', out); }\n");
    fprintf(f, "void cmd_print_help_json(FILE *out, const char *n, const char *s,\n");
    fprintf(f, "    const char *l, void **a)\n");
    fprintf(f, "  { (void)n; (void)s; (void)l; (void)a; fputs(\"{}\\n\", out); }\n\n");

    /* JSON string helper used by --help-json in main() */
    fprintf(f, "static void _jstr(const char *s) {\n");
    fprintf(f, "  if (!s) { fputs(\"null\", stdout); return; }\n");
    fprintf(f, "  putchar('\"');\n");
    fprintf(f, "  for (; *s; s++) {\n");
    fprintf(f, "    if (*s == '\"') fputs(\"\\\\\\\"\", stdout);\n");
    fprintf(f, "    else if (*s == '\\\\') fputs(\"\\\\\\\\\", stdout);\n");
    fprintf(f, "    else if (*s == '\\n') fputs(\"\\\\n\", stdout);\n");
    fprintf(f, "    else putchar(*s); }\n");
    fprintf(f, "  putchar('\"'); }\n\n");

    /* main(): register, optionally respond to --help-json, then delegate */
    fprintf(f, "int main(int argc, char **argv)\n{\n");
    fprintf(f, "    register_%s_command();\n", name);
    fprintf(f, "    if (argc > 1 && strcmp(argv[1], \"--help-json\") == 0) {\n");
    fprintf(f, "        const cmd_spec_t *sp = _reg;\n");
    fprintf(f, "        fputs(\"{\\\"name\\\":\", stdout); _jstr(sp ? sp->name : NULL);\n");
    fprintf(f, "        fputs(\",\\\"summary\\\":\", stdout); _jstr(sp ? sp->summary : NULL);\n");
    fprintf(f, "        fputs(\",\\\"long_help\\\":\", stdout); _jstr(sp ? sp->long_help : NULL);\n");
    fprintf(f, "        fputs(\"}\\n\", stdout);\n");
    fprintf(f, "        return 0;\n");
    fprintf(f, "    }\n");
    fprintf(f, "    return %s_run(argc, argv);\n", name);
    fprintf(f, "}\n");
    fclose(f);
    return 1;
}

/* Write s to f as JSON-safe content (no surrounding quotes) */
static void fwrite_json_escaped(FILE *f, const char *s)
{
    for (; *s; s++) {
        if      (*s == '"')  fputs("\\\"", f);
        else if (*s == '\\') fputs("\\\\", f);
        else if (*s == '\n') fputs("\\n",  f);
        else if (*s == '\r') fputs("\\r",  f);
        else                 fputc(*s, f);
    }
}

/*
 * Run bin/<name> --help, split output at "Options:", and write
 * cmd_<name>/docs/<name>.md with ## Usage and ## Options sections.
 */
static void generate_docs_md(const char *name, const char *subdir, int dry_run)
{
    char docs_path[PATH_MAX];
    snprintf(docs_path, sizeof(docs_path), "%s/docs/%s.md", subdir, name);
    printf("  docs     : %s\n", docs_path);
    if (dry_run) return;

    char bin_path[PATH_MAX];
    snprintf(bin_path, sizeof(bin_path), "bin/%s", name);
    if (access(bin_path, X_OK) != 0)
    {
        fprintf(stderr, "  [WARN] %s not found, skipping docs\n", bin_path);
        return;
    }

    char help_buf[16384];
    char *help_argv[] = { bin_path, "--help", NULL };
    if (capture_cmd(help_argv, help_buf, sizeof(help_buf)) <= 0)
    {
        fprintf(stderr, "  [WARN] capture %s --help failed\n", bin_path);
        return;
    }

    /* Split at the last occurrence of "Options:" preceded by newline */
    char *options_start = strstr(help_buf, "\nOptions:");
    if (!options_start) options_start = strstr(help_buf, "Options:");

    /* Ensure docs/ directory exists */
    char docs_dir[PATH_MAX];
    snprintf(docs_dir, sizeof(docs_dir), "%s/docs", subdir);
    mkdirs(docs_dir);

    FILE *f = fopen(docs_path, "w");
    if (!f) { perror(docs_path); return; }

    fprintf(f, "# %s\n\n", name);

    fprintf(f, "## Usage\n\n```\n");
    if (options_start)
        fwrite(help_buf, 1, (size_t)(options_start - help_buf), f);
    else
        fputs(help_buf, f);
    fprintf(f, "\n```\n\n");

    fprintf(f, "## Options\n\n```\n");
    if (options_start)
    {
        const char *p = options_start;
        if (*p == '\n') p++;  /* skip leading newline before "Options:" */
        fputs(p, f);
    }
    fprintf(f, "\n```\n");

    fclose(f);
    printf("  docs     : written\n");
}

/*
 * Run bin/<name> --help-json, parse name/summary/long_help, and rewrite
 * pkg.json preserving version and files[] from the existing metadata.
 */
static void update_pkg_json_from_bin(const char *name, const char *subdir,
                                      const pkg_meta_t *meta, int dry_run)
{
    char json_path[PATH_MAX + 16];
    snprintf(json_path, sizeof(json_path), "%s/pkg.json", subdir);
    printf("  pkg.json : %s (refreshed)\n", json_path);
    if (dry_run) return;

    char bin_path[PATH_MAX];
    snprintf(bin_path, sizeof(bin_path), "bin/%s", name);
    if (access(bin_path, X_OK) != 0)
    {
        fprintf(stderr, "  [WARN] %s not found, skipping pkg.json refresh\n", bin_path);
        return;
    }

    char hj_buf[4096];
    char *hj_argv[] = { bin_path, "--help-json", NULL };
    if (capture_cmd(hj_argv, hj_buf, sizeof(hj_buf)) <= 0)
    {
        fprintf(stderr, "  [WARN] capture %s --help-json failed\n", bin_path);
        return;
    }

    char summary[512], long_help_val[1024];
    if (!json_str(hj_buf, "summary", summary, sizeof(summary)))
    {
        fprintf(stderr, "  [WARN] could not parse summary from %s --help-json\n", name);
        return;
    }
    if (!json_str(hj_buf, "long_help", long_help_val, sizeof(long_help_val)))
        long_help_val[0] = '\0';

    FILE *f = fopen(json_path, "w");
    if (!f) { perror(json_path); return; }

    fprintf(f, "{\n");
    fprintf(f, "  \"name\": \"%s\",\n", meta->name);
    fprintf(f, "  \"version\": \"%s\",\n", meta->version);
    fprintf(f, "  \"description\": \""); fwrite_json_escaped(f, summary);
    fprintf(f, "\",\n");
    fprintf(f, "  \"long_description\": \""); fwrite_json_escaped(f, long_help_val);
    fprintf(f, "\",\n");
    fprintf(f, "  \"files\": [");
    for (int i = 0; i < meta->nfiles; i++) {
        if (i) fprintf(f, ", ");
        fprintf(f, "\"%s\"", meta->files[i]);
    }
    fprintf(f, "],\n");
    fprintf(f, "  \"docs\": [\"docs/%s.md\"]\n", meta->name);
    fprintf(f, "}\n");
    fclose(f);
    printf("  pkg.json : written\n");
}

/*
 * pkg compile [--dry-run] [scan-dir]
 *
 * Two modes:
 *   Single-module   scan-dir itself contains pkg.json → compile that one module.
 *   Multi-module    scan-dir has no pkg.json of its own → walk immediate
 *                   subdirectories and compile each one that has pkg.json.
 *
 * For each module:
 *   Strategy A — Makefile present in module dir → make -C <dir>
 *   Strategy B — no Makefile → gcc:
 *     1. Compile each .c → build/<name>_<stem>.o, archive → build/lib<name>.a
 *     2. Generate a minimal main() wrapper, link → bin/<name> (standalone)
 *
 * Flags:
 *   --dry-run   Print every command without executing it.
 */

/* Compile one module directory. Returns 0 on success, 1 on failure. */
static int compile_one_module(const char *subdir, int dry_run)
{
    char json_path[PATH_MAX + 16];
    snprintf(json_path, sizeof(json_path), "%s/pkg.json", subdir);

    pkg_meta_t meta;
    if (!parse_pkg_json(json_path, &meta)) return 1;

    printf("\n  [%s %s]  %s/\n", meta.name, meta.version, subdir);

    /* ── Strategy A: Makefile present ── */
    char mk_path[PATH_MAX + 16];
    snprintf(mk_path, sizeof(mk_path), "%s/Makefile", subdir);

    if (access(mk_path, R_OK) == 0)
    {
        printf("  strategy : Makefile\n");
        printf("  command  : make -C %s\n", subdir);
        if (!dry_run)
        {
            char *args[] = { "make", "-C", (char *)subdir, NULL };
            if (run_cmd(args) != 0)
            {
                fprintf(stderr, "  [FAIL] make -C %s\n", subdir);
                return 1;
            }
        }
        generate_docs_md(meta.name, subdir, dry_run);
        update_pkg_json_from_bin(meta.name, subdir, &meta, dry_run);
        printf("  [OK]\n");
        return 0;
    }

    /* ── Strategy B: gcc build (no Makefile) ── */
    printf("  strategy : gcc\n");

    char srcs[32][PATH_MAX];
    int  nsrcs = collect_sources(subdir, srcs, 32);
    if (nsrcs == 0)
    {
        fprintf(stderr, "  [SKIP] no .c files found in %s/\n", subdir);
        return 1;
    }
    for (int i = 0; i < nsrcs; i++)
        printf("  source   : %s\n", srcs[i]);

    char incl_sub[PATH_MAX + 2];
    snprintf(incl_sub, sizeof(incl_sub), "-I%s", subdir);

    /* Step 1: compile each .c → build/<name>_<stem>.o */
    char obj_paths[32][PATH_MAX];
    int  compile_ok = 1;

    for (int i = 0; i < nsrcs; i++)
    {
        const char *bn = strrchr(srcs[i], '/');
        bn = bn ? bn + 1 : srcs[i];

        char stem[256];
        strncpy(stem, bn, sizeof(stem) - 1);
        stem[sizeof(stem) - 1] = '\0';
        size_t slen = strlen(stem);
        if (slen > 2 && stem[slen - 2] == '.' && stem[slen - 1] == 'c')
            stem[slen - 2] = '\0';  /* strip .c */

        snprintf(obj_paths[i], PATH_MAX, "build/%s_%s.o", meta.name, stem);

        char *cc[] = {
            "gcc", "-Wall", "-Wextra", "-g",
            "-std=c99", "-D_POSIX_C_SOURCE=200809L", "-D_XOPEN_SOURCE=700",
            "-I.", incl_sub, "-Iargtable3", "-Icmd_spec", "-Icmd_registry",
            "-c", srcs[i], "-o", obj_paths[i],
            NULL
        };
        printf("  compile  : %s → %s\n", srcs[i], obj_paths[i]);
        if (!dry_run && run_cmd(cc) != 0)
        {
            fprintf(stderr, "  [FAIL] compile %s\n", srcs[i]);
            compile_ok = 0;
            break;
        }
    }
    if (!compile_ok) return 1;

    /* Step 2: archive → build/lib<name>.a */
    char lib_path[PATH_MAX];
    snprintf(lib_path, sizeof(lib_path), "build/lib%s.a", meta.name);

    char *ar_args[64];
    int   ai = 0;
    ar_args[ai++] = "ar";
    ar_args[ai++] = "rcs";
    ar_args[ai++] = lib_path;
    for (int i = 0; i < nsrcs; i++)
        ar_args[ai++] = obj_paths[i];
    ar_args[ai] = NULL;

    printf("  archive  : %s\n", lib_path);
    if (!dry_run && run_cmd(ar_args) != 0)
    {
        fprintf(stderr, "  [FAIL] ar %s\n", lib_path);
        return 1;
    }

    /* Step 3: generate a minimal main() wrapper */
    char header_rel[PATH_MAX + 272];
    snprintf(header_rel, sizeof(header_rel), "%s/cmd_%s.h", subdir, meta.name);
    if (access(header_rel, R_OK) != 0)
        snprintf(header_rel, sizeof(header_rel), "%s/%s.h", subdir, meta.name);

    char wrapper_path[PATH_MAX + 32];
    snprintf(wrapper_path, sizeof(wrapper_path),
             "/tmp/pkgcompile_%s_main.c", meta.name);
    printf("  wrapper  : %s (calls %s_run)\n", wrapper_path, meta.name);

    if (!dry_run && !write_main_wrapper(meta.name, header_rel,
                                        wrapper_path, sizeof(wrapper_path)))
        return 1;

    /* Step 4: link standalone binary → bin/<name> */
    char bin_path[PATH_MAX];
    snprintf(bin_path, sizeof(bin_path), "bin/%s", meta.name);

    char *bin_args[128];
    int   bi = 0;
    bin_args[bi++] = "gcc";
    bin_args[bi++] = "-Wall";
    bin_args[bi++] = "-g";
    bin_args[bi++] = "-std=c99";
    bin_args[bi++] = "-D_POSIX_C_SOURCE=200809L";
    bin_args[bi++] = "-D_XOPEN_SOURCE=700";
    bin_args[bi++] = "-I.";
    bin_args[bi++] = incl_sub;
    bin_args[bi++] = "-Iargtable3";
    bin_args[bi++] = "-Icmd_spec";
    bin_args[bi++] = "-Icmd_registry";
    bin_args[bi++] = "-o";
    bin_args[bi++] = bin_path;
    for (int i = 0; i < nsrcs; i++)
        bin_args[bi++] = srcs[i];
    bin_args[bi++] = "argtable3/argtable3.c";
    bin_args[bi++] = wrapper_path;
    bin_args[bi++] = "-lm";
    bin_args[bi]   = NULL;

    printf("  binary   : %s\n", bin_path);
    if (!dry_run)
    {
        int ret = run_cmd(bin_args);
        if (ret != 0)
        {
            unlink(wrapper_path);
            fprintf(stderr, "  [FAIL] link %s\n", bin_path);
            return 1;
        }
        unlink(wrapper_path);
    }

    generate_docs_md(meta.name, subdir, dry_run);
    update_pkg_json_from_bin(meta.name, subdir, &meta, dry_run);
    printf("  [OK]\n");
    return 0;
}

static int cmd_compile(int argc, char **argv)
{
    int         dry_run  = 0;
    const char *scan_dir = ".";

    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "--dry-run") == 0)
            dry_run = 1;
        else if (argv[i][0] != '-')
            scan_dir = argv[i];
    }

    if (!dry_run)
    {
        mkdirs("bin");
        mkdirs("build");
    }
    else
    {
        printf("  [dry-run] mkdir -p bin build\n");
    }

    /* Single-module mode: scan_dir itself has pkg.json */
    char self_json[PATH_MAX + 16];
    snprintf(self_json, sizeof(self_json), "%s/pkg.json", scan_dir);
    if (access(self_json, R_OK) == 0)
    {
        printf("pkg compile: single module '%s'%s\n", scan_dir,
               dry_run ? " [dry-run]" : "");
        int err = compile_one_module(scan_dir, dry_run);
        printf("\npkg compile: %s\n", err ? "1 error" : "done");
        return err;
    }

    /* Multi-module mode: scan subdirectories */
    printf("pkg compile: scanning '%s'%s\n", scan_dir,
           dry_run ? " [dry-run]" : "");

    DIR *d = opendir(scan_dir);
    if (!d) { perror(scan_dir); return 1; }

    int errors = 0, built = 0;
    struct dirent *e;

    while ((e = readdir(d)))
    {
        if (e->d_name[0] == '.') continue;

        char subdir[PATH_MAX];
        snprintf(subdir, sizeof(subdir), "%s/%s", scan_dir, e->d_name);

        struct stat st;
        if (stat(subdir, &st) != 0 || !S_ISDIR(st.st_mode)) continue;

        char json_path[PATH_MAX + 16];
        snprintf(json_path, sizeof(json_path), "%s/pkg.json", subdir);
        if (access(json_path, R_OK) != 0) continue;

        if (compile_one_module(subdir, dry_run) == 0)
            built++;
        else
            errors++;
    }
    closedir(d);

    printf("\npkg compile: %d module(s) built", built);
    if (errors) printf(", %d error(s)", errors);
    printf("\n");
    return errors ? 1 : 0;
}

/* ── usage ─────────────────────────────────────────────────────────────── */

static void print_usage(void)
{
    printf("\nUsage: pkg <subcommand> [args...]\n\n");
    printf("Subcommands:\n");
    printf("  build <src-dir> <output.tar.gz>   package a directory into a .tar.gz\n");
    printf("  install <archive.tar.gz>           install a package\n");
    printf("  list                               list installed packages\n");
    printf("  remove <name>                      remove an installed package\n");
    printf("  compile [--dry-run] [dir]          build modules in dir (default: .)\n");
    printf("\nInstall layout:\n");
    printf("  ~/.CoreShell/pkgs/<name>-<version>/  extracted package contents\n");
    printf("  ~/.CoreShell/bin/<name>              symlink to executable\n");
    printf("  ~/.CoreShell/pkgdb.txt               installed package database\n\n");
    printf("Compile outputs:\n");
    printf("  bin/<name>              standalone executable\n");
    printf("  build/lib<name>.a       static library for linking into the shell\n\n");
}

/* ── main ──────────────────────────────────────────────────────────────── */

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        print_usage();
        return 1;
    }

    const char *sub = argv[1];

    if (strcmp(sub, "build") == 0)
    {
        if (argc < 4)
        {
            fprintf(stderr, "Usage: pkg build <src-dir> <output.tar.gz>\n");
            return 1;
        }
        return cmd_build(argc - 2, argv + 2);
    }
    if (strcmp(sub, "install") == 0)
    {
        if (argc < 3)
        {
            fprintf(stderr, "Usage: pkg install <archive.tar.gz>\n");
            return 1;
        }
        return cmd_install(argc - 2, argv + 2);
    }
    if (strcmp(sub, "list") == 0)
    {
        return cmd_list();
    }
    if (strcmp(sub, "remove") == 0)
    {
        if (argc < 3)
        {
            fprintf(stderr, "Usage: pkg remove <name>\n");
            return 1;
        }
        return cmd_remove(argc - 2, argv + 2);
    }
    if (strcmp(sub, "compile") == 0)
    {
        return cmd_compile(argc - 2, argv + 2);
    }
    if (strcmp(sub, "--help") == 0 || strcmp(sub, "-h") == 0)
    {
        print_usage();
        return 0;
    }

    fprintf(stderr, "pkg: unknown subcommand '%s'\n", sub);
    print_usage();
    return 1;
}
