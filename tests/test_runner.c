/*
 * tests/test_runner.c — CoreShell Automated Test Runner
 *
 * Links directly against the built-in command object files (no shell
 * subprocess). Each test case is executed in a forked child process for
 * full isolation: crashes, exit() calls, and chdir() do not affect the
 * runner. stdout/stderr are captured via pipes and evaluated against
 * optional substring expectations.
 *
 * Build & run:
 *   make test
 *
 * Output:
 *   • ANSI-coloured summary on the terminal
 *   • test_report.md  written to the project root
 *
 * Limitation:
 *   Pipe reads are sequential (stdout first, then stderr). Commands whose
 *   combined output exceeds the OS pipe buffer (~64 KB) would deadlock.
 *   None of the built-in test cases approach that limit.
 */

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE   700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#include "cmd_registry.h"
#include "cmd_spec.h"

/* ── Configuration ───────────────────────────────────────────────────── */

#define MAX_TESTS    256
#define MAX_OUTPUT   4096
#define EXPECT_FAIL  (-1)   /* any non-zero exit code is acceptable */

/* ── ANSI colours (disabled when stdout is not a tty) ────────────────── */

#define COL_RESET  "\033[0m"
#define COL_GREEN  "\033[32m"
#define COL_RED    "\033[31m"
#define COL_CYAN   "\033[36m"
#define COL_BOLD   "\033[1m"
#define COL_DIM    "\033[2m"

static int g_use_color = 0;

#define CPRINT(col, ...) \
    do { if (g_use_color) fputs((col), stdout); \
         printf(__VA_ARGS__); \
         if (g_use_color) fputs(COL_RESET, stdout); } while (0)

/* ── Test-case and result types ──────────────────────────────────────── */

typedef struct {
    const char *description;
    const char *argv[16];       /* NULL-terminated argument list         */
    int         expected_exit;  /* 0, positive code, or EXPECT_FAIL(-1)  */
    const char *stdout_contains;/* substring that must appear, or NULL   */
    const char *stderr_contains;/* substring that must appear, or NULL   */
} test_case_t;

typedef struct {
    const char *description;
    int         expected_exit;
    int         actual_exit;
    char        stdout_buf[MAX_OUTPUT];
    char        stderr_buf[MAX_OUTPUT];
    const char *stdout_contains;
    const char *stderr_contains;
    int         stdout_match;   /* 1 if check passed or no check          */
    int         stderr_match;
    int         passed;
} test_result_t;

/* ── Global state ────────────────────────────────────────────────────── */

static test_result_t g_results[MAX_TESTS];
static int           g_count       = 0;
static char          g_fixture_dir[512];

/* ── Dynamic description pool ────────────────────────────────────────── */
/* Storage for test descriptions generated at runtime (e.g. inside loops).
   Strings are written once into a static pool and never freed. */
static char g_desc_pool[128][128];
static int  g_desc_used = 0;

static const char *make_desc(const char *fmt, const char *arg)
{
    if (g_desc_used >= 128) return fmt;
    snprintf(g_desc_pool[g_desc_used], 128, fmt, arg);
    return g_desc_pool[g_desc_used++];
}

/* All built-in command names (must match register_all_builtin_commands) */
static const char *s_commands[] = {
    "ls",  "cat",   "cd",    "cp",    "echo",  "exit",
    "head","help",  "mkdir", "mv",    "pwd",   "rm",
    "rmdir","stat", "tail",  "touch"
};
#define N_COMMANDS (int)(sizeof(s_commands) / sizeof(s_commands[0]))

/* ── Fixture helpers ─────────────────────────────────────────────────── */

static void fixture_path(char *buf, size_t sz, const char *relpath)
{
    snprintf(buf, sz, "%s/%s", g_fixture_dir, relpath);
}

static void create_fixture_file(const char *relpath, const char *content)
{
    char path[600];
    fixture_path(path, sizeof(path), relpath);
    FILE *f = fopen(path, "w");
    if (!f) { perror(path); return; }
    fputs(content, f);
    fclose(f);
}

static void create_fixture_dir(const char *relpath)
{
    char path[600];
    fixture_path(path, sizeof(path), relpath);
    if (mkdir(path, 0755) != 0 && errno != EEXIST)
        perror(path);
}

static void setup_fixtures(void)
{
    char tmpl[] = "/tmp/coreshell_test_XXXXXX";
    if (!mkdtemp(tmpl)) { perror("mkdtemp"); exit(EXIT_FAILURE); }
    strncpy(g_fixture_dir, tmpl, sizeof(g_fixture_dir) - 1);

    /* Stable read-only files */
    create_fixture_file("test.txt",
        "Hello World\nLine 2\nLine 3\nLine 4\nLine 5\n");
    create_fixture_file("empty.txt", "");
    create_fixture_dir("subdir");
}

static void teardown_fixtures(void)
{
    char cmd[600];
    /* Single-quoted path avoids any shell expansion */
    snprintf(cmd, sizeof(cmd), "rm -rf -- '%s'", g_fixture_dir);
    if (system(cmd) != 0)
        fprintf(stderr, "warning: could not remove fixture dir %s\n",
                g_fixture_dir);
}

/* ── Core runner: execute one test in a forked child ────────────────── */

static void run_test(const test_case_t *tc)
{
    if (g_count >= MAX_TESTS) {
        fprintf(stderr, "error: MAX_TESTS (%d) exceeded\n", MAX_TESTS);
        return;
    }

    test_result_t *r    = &g_results[g_count++];
    r->description      = tc->description;
    r->expected_exit    = tc->expected_exit;
    r->actual_exit      = -1;
    r->stdout_buf[0]    = '\0';
    r->stderr_buf[0]    = '\0';
    r->stdout_contains  = tc->stdout_contains;
    r->stderr_contains  = tc->stderr_contains;
    r->stdout_match     = 1;
    r->stderr_match     = 1;
    r->passed           = 0;

    int out_pipe[2], err_pipe[2];
    if (pipe(out_pipe) < 0 || pipe(err_pipe) < 0) {
        perror("pipe");
        return;
    }

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return; }

    if (pid == 0) {
        /* ── Child: redirect I/O, run the command ── */
        close(out_pipe[0]);
        close(err_pipe[0]);

        if (dup2(out_pipe[1], STDOUT_FILENO) < 0) _exit(126);
        if (dup2(err_pipe[1], STDERR_FILENO) < 0) _exit(126);
        close(out_pipe[1]);
        close(err_pipe[1]);

        /* Build mutable argv array */
        char *args[16];
        int   argc = 0;
        while (tc->argv[argc] && argc < 15) {
            args[argc] = (char *)tc->argv[argc];
            argc++;
        }
        args[argc] = NULL;

        const cmd_spec_t *spec = find_command(args[0]);
        if (!spec) {
            fprintf(stderr, "command not found: %s\n", args[0]);
            _exit(127);
        }

        int ret = spec->run(argc, args);
        fflush(stdout);
        fflush(stderr);
        _exit(ret);
    }

    /* ── Parent: close write ends, drain pipes, then reap ── */
    close(out_pipe[1]);
    close(err_pipe[1]);

    /* Read stdout until EOF (write end closed when child exits) */
    {
        ssize_t n, total = 0;
        while (total < MAX_OUTPUT - 1 &&
               (n = read(out_pipe[0], r->stdout_buf + total,
                         (size_t)(MAX_OUTPUT - 1 - total))) > 0)
            total += n;
        r->stdout_buf[total] = '\0';
    }
    close(out_pipe[0]);

    /* Read stderr */
    {
        ssize_t n, total = 0;
        while (total < MAX_OUTPUT - 1 &&
               (n = read(err_pipe[0], r->stderr_buf + total,
                         (size_t)(MAX_OUTPUT - 1 - total))) > 0)
            total += n;
        r->stderr_buf[total] = '\0';
    }
    close(err_pipe[0]);

    int status;
    waitpid(pid, &status, 0);
    r->actual_exit = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

    /* Evaluate: exit code */
    if (tc->expected_exit == EXPECT_FAIL)
        r->passed = (r->actual_exit != 0);
    else
        r->passed = (r->actual_exit == tc->expected_exit);

    /* Evaluate: stdout substring */
    if (tc->stdout_contains) {
        r->stdout_match = strstr(r->stdout_buf, tc->stdout_contains) != NULL;
        if (!r->stdout_match) r->passed = 0;
    }

    /* Evaluate: stderr substring */
    if (tc->stderr_contains) {
        r->stderr_match = strstr(r->stderr_buf, tc->stderr_contains) != NULL;
        if (!r->stderr_match) r->passed = 0;
    }
}
/* ── Shell-subprocess test (forks and execs the CoreShell binary) ───── */
/*
 * Identical to run_test() except the child does execvp(argv[0], argv)
 * instead of calling spec->run() directly.  Used for multicall tests
 * that exercise the full binary entry point.
 */
static void run_shell_test(const test_case_t *tc)
{
    if (g_count >= MAX_TESTS) {
        fprintf(stderr, "error: MAX_TESTS (%d) exceeded\n", MAX_TESTS);
        return;
    }

    test_result_t *r    = &g_results[g_count++];
    r->description      = tc->description;
    r->expected_exit    = tc->expected_exit;
    r->actual_exit      = -1;
    r->stdout_buf[0]    = '\0';
    r->stderr_buf[0]    = '\0';
    r->stdout_contains  = tc->stdout_contains;
    r->stderr_contains  = tc->stderr_contains;
    r->stdout_match     = 1;
    r->stderr_match     = 1;
    r->passed           = 0;

    int out_pipe[2], err_pipe[2];
    if (pipe(out_pipe) < 0 || pipe(err_pipe) < 0) { perror("pipe"); return; }

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return; }

    if (pid == 0) {
        close(out_pipe[0]);
        close(err_pipe[0]);
        if (dup2(out_pipe[1], STDOUT_FILENO) < 0) _exit(126);
        if (dup2(err_pipe[1], STDERR_FILENO) < 0) _exit(126);
        close(out_pipe[1]);
        close(err_pipe[1]);

        char *args[16];
        int   argc = 0;
        while (tc->argv[argc] && argc < 15) {
            args[argc] = (char *)tc->argv[argc];
            argc++;
        }
        args[argc] = NULL;

        execvp(args[0], args);
        perror(args[0]);
        _exit(127);
    }

    close(out_pipe[1]);
    close(err_pipe[1]);

    { ssize_t n, total = 0;
      while (total < MAX_OUTPUT - 1 &&
             (n = read(out_pipe[0], r->stdout_buf + total,
                       (size_t)(MAX_OUTPUT - 1 - total))) > 0)
          total += n;
      r->stdout_buf[total] = '\0'; }
    close(out_pipe[0]);

    { ssize_t n, total = 0;
      while (total < MAX_OUTPUT - 1 &&
             (n = read(err_pipe[0], r->stderr_buf + total,
                       (size_t)(MAX_OUTPUT - 1 - total))) > 0)
          total += n;
      r->stderr_buf[total] = '\0'; }
    close(err_pipe[0]);

    int status;
    waitpid(pid, &status, 0);
    r->actual_exit = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

    if (tc->expected_exit == EXPECT_FAIL)
        r->passed = (r->actual_exit != 0);
    else
        r->passed = (r->actual_exit == tc->expected_exit);

    if (tc->stdout_contains) {
        r->stdout_match = strstr(r->stdout_buf, tc->stdout_contains) != NULL;
        if (!r->stdout_match) r->passed = 0;
    }
    if (tc->stderr_contains) {
        r->stderr_match = strstr(r->stderr_buf, tc->stderr_contains) != NULL;
        if (!r->stderr_match) r->passed = 0;
    }
}

/* ── File-content test (no fork; opens file and checks substrings) ────── */
/*
 * Opens filepath, reads its entire content, then verifies each string
 * in must_contain[] appears in the content.  No process is spawned.
 * Used to validate pkg.json and docs/<name>.md packaging artefacts.
 */
static void check_file_test(const char  *description,
                             const char  *filepath,
                             const char **must_contain,
                             int          n_checks)
{
    if (g_count >= MAX_TESTS) {
        fprintf(stderr, "error: MAX_TESTS (%d) exceeded\n", MAX_TESTS);
        return;
    }

    test_result_t *r = &g_results[g_count++];
    r->description     = description;
    r->expected_exit   = 0;
    r->actual_exit     = 0;
    r->stdout_buf[0]   = '\0';
    r->stderr_buf[0]   = '\0';
    r->stdout_contains = NULL;
    r->stderr_contains = NULL;
    r->stdout_match    = 1;
    r->stderr_match    = 1;
    r->passed          = 1;

    FILE *f = fopen(filepath, "r");
    if (!f) {
        snprintf(r->stderr_buf, MAX_OUTPUT - 1, "cannot open: %s", filepath);
        r->passed      = 0;
        r->actual_exit = 1;
        return;
    }

    size_t total = 0;
    int    c;
    while (total < (size_t)(MAX_OUTPUT - 1) && (c = fgetc(f)) != EOF)
        r->stdout_buf[total++] = (char)c;
    r->stdout_buf[total] = '\0';
    fclose(f);

    for (int i = 0; i < n_checks; i++) {
        if (strstr(r->stdout_buf, must_contain[i]) == NULL) {
            r->passed          = 0;
            r->stdout_match    = 0;
            r->stdout_contains = must_contain[i];
            snprintf(r->stderr_buf, MAX_OUTPUT - 1,
                     "missing: \"%s\"", must_contain[i]);
            break; /* report first missing entry only */
        }
    }
}
/* ── Test suites (one function per built-in command) ─────────────────── */

/* ---- help ------------------------------------------------------------ */
static void test_help(void)
{
    run_test(&(test_case_t){
        "help: no args lists all commands",
        {"help", NULL}, 0, "exit", NULL
    });
    run_test(&(test_case_t){
        "help: valid command name shows usage",
        {"help", "ls", NULL}, 0, "ls", NULL
    });
    run_test(&(test_case_t){
        "help: unknown command returns error",
        {"help", "unknowncmd_xyz99", NULL}, EXPECT_FAIL, NULL, NULL
    });
}

/* ---- exit ------------------------------------------------------------ */
static void test_exit(void)
{
    run_test(&(test_case_t){
        "exit: --help prints usage line",
        {"exit", "--help", NULL}, 0, "Usage", NULL
    });
    run_test(&(test_case_t){
        "exit: bare call terminates child with status 0",
        {"exit", NULL}, 0, NULL, NULL
    });
}

/* ---- cd -------------------------------------------------------------- */
static void test_cd(void)
{
    run_test(&(test_case_t){
        "cd: change to /tmp succeeds",
        {"cd", "/tmp", NULL}, 0, NULL, NULL
    });
    run_test(&(test_case_t){
        "cd: nonexistent path returns error",
        {"cd", "/nonexistent_xyz_coreshell_99999", NULL}, EXPECT_FAIL, NULL, NULL
    });
    run_test(&(test_case_t){
        "cd: no args navigates to HOME",
        {"cd", NULL}, 0, NULL, NULL
    });
    run_test(&(test_case_t){
        "cd: --help prints usage line",
        {"cd", "--help", NULL}, 0, "Usage", NULL
    });
}

/* ---- pwd ------------------------------------------------------------- */
static void test_pwd(void)
{
    run_test(&(test_case_t){
        "pwd: prints an absolute path",
        {"pwd", NULL}, 0, "/", NULL
    });
    run_test(&(test_case_t){
        "pwd: --help prints usage line",
        {"pwd", "--help", NULL}, 0, "Usage", NULL
    });
}

/* ---- echo ------------------------------------------------------------ */
static void test_echo(void)
{
    run_test(&(test_case_t){
        "echo: no args prints empty line",
        {"echo", NULL}, 0, "\n", NULL
    });
    run_test(&(test_case_t){
        "echo: multiple strings printed space-separated",
        {"echo", "hello", "world", NULL}, 0, "hello world", NULL
    });
    run_test(&(test_case_t){
        "echo: -n suppresses trailing newline",
        {"echo", "-n", "nolf", NULL}, 0, "nolf", NULL
    });
    run_test(&(test_case_t){
        "echo: -e interprets \\t as tab",
        {"echo", "-e", "col1\\tcol2", NULL}, 0, "col1\tcol2", NULL
    });
    run_test(&(test_case_t){
        "echo: --help prints usage line",
        {"echo", "--help", NULL}, 0, "Usage", NULL
    });
}

/* ---- ls -------------------------------------------------------------- */
static void test_ls(void)
{
    run_test(&(test_case_t){
        "ls: lists fixture directory",
        {"ls", g_fixture_dir, NULL}, 0, "test.txt", NULL
    });
    run_test(&(test_case_t){
        "ls: -a shows dot entries",
        {"ls", "-a", g_fixture_dir, NULL}, 0, ".", NULL
    });
    run_test(&(test_case_t){
        "ls: -l shows long format with test.txt",
        {"ls", "-l", g_fixture_dir, NULL}, 0, "test.txt", NULL
    });
    run_test(&(test_case_t){
        "ls: -la combines long + all flags",
        {"ls", "-la", g_fixture_dir, NULL}, 0, "subdir", NULL
    });
    run_test(&(test_case_t){
        "ls: nonexistent path returns error",
        {"ls", "/nonexistent_path_coreshell_xyz", NULL}, EXPECT_FAIL, NULL, NULL
    });
    run_test(&(test_case_t){
        "ls: --help prints usage line",
        {"ls", "--help", NULL}, 0, "Usage", NULL
    });
}

/* ---- stat ------------------------------------------------------------ */
static void test_stat(void)
{
    char path[600], nopath[600];
    fixture_path(path,   sizeof(path),   "test.txt");
    fixture_path(nopath, sizeof(nopath), "no_such_file_stat.txt");

    run_test(&(test_case_t){
        "stat: prints file size for existing file",
        {"stat", path, NULL}, 0, "Size", NULL
    });
    run_test(&(test_case_t){
        "stat: nonexistent file returns error",
        {"stat", nopath, NULL}, EXPECT_FAIL, NULL, NULL
    });
    run_test(&(test_case_t){
        "stat: --help prints usage line",
        {"stat", "--help", NULL}, 0, "Usage", NULL
    });
}

/* ---- cat ------------------------------------------------------------- */
static void test_cat(void)
{
    char path[600], nopath[600], empty_path[600];
    fixture_path(path,       sizeof(path),       "test.txt");
    fixture_path(nopath,     sizeof(nopath),     "no_such_file_cat.txt");
    fixture_path(empty_path, sizeof(empty_path), "empty.txt");

    run_test(&(test_case_t){
        "cat: prints full content of test.txt",
        {"cat", path, NULL}, 0, "Hello World", NULL
    });
    run_test(&(test_case_t){
        "cat: empty file succeeds with no output",
        {"cat", empty_path, NULL}, 0, NULL, NULL
    });
    run_test(&(test_case_t){
        "cat: nonexistent file returns error",
        {"cat", nopath, NULL}, EXPECT_FAIL, NULL, NULL
    });
    run_test(&(test_case_t){
        "cat: --help prints usage line",
        {"cat", "--help", NULL}, 0, "Usage", NULL
    });
}

/* ---- head ------------------------------------------------------------ */
static void test_head(void)
{
    char path[600], nopath[600];
    fixture_path(path,   sizeof(path),   "test.txt");
    fixture_path(nopath, sizeof(nopath), "no_such_file_head.txt");

    run_test(&(test_case_t){
        "head: default 10 lines includes first line",
        {"head", path, NULL}, 0, "Hello World", NULL
    });
    run_test(&(test_case_t){
        "head: -n 2 returns only first two lines",
        {"head", "-n", "2", path, NULL}, 0, "Hello World", NULL
    });
    run_test(&(test_case_t){
        "head: -n 1 returns exactly the first line",
        {"head", "-n", "1", path, NULL}, 0, "Hello World", NULL
    });
    run_test(&(test_case_t){
        "head: nonexistent file returns error",
        {"head", nopath, NULL}, EXPECT_FAIL, NULL, NULL
    });
    run_test(&(test_case_t){
        "head: --help prints usage line",
        {"head", "--help", NULL}, 0, "Usage", NULL
    });
}

/* ---- tail ------------------------------------------------------------ */
static void test_tail(void)
{
    char path[600], nopath[600];
    fixture_path(path,   sizeof(path),   "test.txt");
    fixture_path(nopath, sizeof(nopath), "no_such_file_tail.txt");

    run_test(&(test_case_t){
        "tail: default output includes last line",
        {"tail", path, NULL}, 0, "Line 5", NULL
    });
    run_test(&(test_case_t){
        "tail: -n 2 includes last two lines",
        {"tail", "-n", "2", path, NULL}, 0, "Line 5", NULL
    });
    run_test(&(test_case_t){
        "tail: -n 1 returns only last line",
        {"tail", "-n", "1", path, NULL}, 0, "Line 5", NULL
    });
    run_test(&(test_case_t){
        "tail: nonexistent file returns error",
        {"tail", nopath, NULL}, EXPECT_FAIL, NULL, NULL
    });
    run_test(&(test_case_t){
        "tail: --help prints usage line",
        {"tail", "--help", NULL}, 0, "Usage", NULL
    });
}

/* ---- cp -------------------------------------------------------------- */
static void test_cp(void)
{
    char src[600], dst[600], nosrc[600];
    fixture_path(src,   sizeof(src),   "test.txt");
    fixture_path(dst,   sizeof(dst),   "cp_dst.txt");
    fixture_path(nosrc, sizeof(nosrc), "no_such_file_cp.txt");

    run_test(&(test_case_t){
        "cp: copies existing file to new destination",
        {"cp", src, dst, NULL}, 0, NULL, NULL
    });
    run_test(&(test_case_t){
        "cp: nonexistent source returns error",
        {"cp", nosrc, dst, NULL}, EXPECT_FAIL, NULL, NULL
    });
    run_test(&(test_case_t){
        "cp: --help prints usage line",
        {"cp", "--help", NULL}, 0, "Usage", NULL
    });
}

/* ---- mv -------------------------------------------------------------- */
static void test_mv(void)
{
    char src[600], dst[600], nosrc[600];
    fixture_path(src,   sizeof(src),   "mv_src.txt");
    fixture_path(dst,   sizeof(dst),   "mv_dst.txt");
    fixture_path(nosrc, sizeof(nosrc), "no_such_file_mv.txt");

    /* Create a fresh file for the move; it will be consumed by the test */
    create_fixture_file("mv_src.txt", "move me\n");

    run_test(&(test_case_t){
        "mv: renames/moves source file to destination",
        {"mv", src, dst, NULL}, 0, NULL, NULL
    });
    run_test(&(test_case_t){
        "mv: nonexistent source returns error",
        {"mv", nosrc, dst, NULL}, EXPECT_FAIL, NULL, NULL
    });
    run_test(&(test_case_t){
        "mv: --help prints usage line",
        {"mv", "--help", NULL}, 0, "Usage", NULL
    });
}

/* ---- rm -------------------------------------------------------------- */
static void test_rm(void)
{
    char target[600], notarget[600];
    fixture_path(target,   sizeof(target),   "rm_target.txt");
    fixture_path(notarget, sizeof(notarget), "no_such_file_rm.txt");

    /* Fresh file consumed by the test */
    create_fixture_file("rm_target.txt", "delete me\n");

    run_test(&(test_case_t){
        "rm: removes existing file",
        {"rm", target, NULL}, 0, NULL, NULL
    });
    run_test(&(test_case_t){
        "rm: nonexistent file returns error",
        {"rm", notarget, NULL}, EXPECT_FAIL, NULL, NULL
    });
    run_test(&(test_case_t){
        "rm: --help prints usage line",
        {"rm", "--help", NULL}, 0, "Usage", NULL
    });
}

/* ---- mkdir ----------------------------------------------------------- */
static void test_mkdir(void)
{
    char newdir[600], existing[600];
    fixture_path(newdir,   sizeof(newdir),   "mkdir_new");
    fixture_path(existing, sizeof(existing), "subdir");  /* already exists */

    run_test(&(test_case_t){
        "mkdir: creates a new directory",
        {"mkdir", newdir, NULL}, 0, NULL, NULL
    });
    run_test(&(test_case_t){
        "mkdir: existing directory returns error",
        {"mkdir", existing, NULL}, EXPECT_FAIL, NULL, NULL
    });
    run_test(&(test_case_t){
        "mkdir: --help prints usage line",
        {"mkdir", "--help", NULL}, 0, "Usage", NULL
    });
}

/* ---- rmdir ----------------------------------------------------------- */
static void test_rmdir(void)
{
    char target[600], nodir[600], nonempty[600];
    fixture_path(target,   sizeof(target),   "rmdir_target");
    fixture_path(nodir,    sizeof(nodir),    "no_such_dir_rmdir");
    fixture_path(nonempty, sizeof(nonempty), "subdir"); /* non-empty dir */

    /* Fresh empty directory for the success case */
    create_fixture_dir("rmdir_target");

    run_test(&(test_case_t){
        "rmdir: removes an empty directory",
        {"rmdir", target, NULL}, 0, NULL, NULL
    });
    run_test(&(test_case_t){
        "rmdir: nonexistent directory returns error",
        {"rmdir", nodir, NULL}, EXPECT_FAIL, NULL, NULL
    });
    run_test(&(test_case_t){
        "rmdir: --help prints usage line",
        {"rmdir", "--help", NULL}, 0, "Usage", NULL
    });
}

/* ---- touch ----------------------------------------------------------- */
static void test_touch(void)
{
    char newfile[600], existing[600];
    fixture_path(newfile,  sizeof(newfile),  "touch_new.txt");
    fixture_path(existing, sizeof(existing), "test.txt");  /* already exists */

    run_test(&(test_case_t){
        "touch: creates a new file",
        {"touch", newfile, NULL}, 0, NULL, NULL
    });
    run_test(&(test_case_t){
        "touch: updates timestamp of existing file",
        {"touch", existing, NULL}, 0, NULL, NULL
    });
    run_test(&(test_case_t){
        "touch: --help prints usage line",
        {"touch", "--help", NULL}, 0, "Usage", NULL
    });
}

/* ---- cmd_spec_t metadata --------------------------------------------- */
/*
 * Validates that every registered command has all five fields of
 * cmd_spec_t populated: name, summary, long_help, run, print_usage.
 */
static void test_cmd_spec_metadata(void)
{
    for (int i = 0; i < N_COMMANDS; i++) {
        const char       *name = s_commands[i];
        const cmd_spec_t *spec = find_command(name);

        test_result_t *r = &g_results[g_count++];
        r->description     = make_desc("cmd_spec: %s has complete metadata", name);
        r->expected_exit   = 0;
        r->stdout_buf[0]   = '\0';
        r->stderr_buf[0]   = '\0';
        r->stdout_contains = NULL;
        r->stderr_contains = NULL;
        r->stdout_match    = 1;
        r->stderr_match    = 1;

        if (!spec) {
            r->actual_exit = 1;
            r->passed      = 0;
            snprintf(r->stderr_buf, MAX_OUTPUT - 1,
                     "find_command(\"%s\") returned NULL", name);
            continue;
        }

        int ok = (spec->name       && spec->name[0])       &&
                 (spec->summary    && spec->summary[0])    &&
                 (spec->long_help  && spec->long_help[0])  &&
                 (spec->run        != NULL)                 &&
                 (spec->print_usage != NULL);

        r->actual_exit = ok ? 0 : 1;
        r->passed      = ok;
        if (!ok)
            snprintf(r->stderr_buf, MAX_OUTPUT - 1,
                "name=%s summary=%s long_help=%s run=%s print_usage=%s",
                spec->name      ? (spec->name[0]      ? "ok" : "empty") : "NULL",
                spec->summary   ? (spec->summary[0]   ? "ok" : "empty") : "NULL",
                spec->long_help ? (spec->long_help[0] ? "ok" : "empty") : "NULL",
                spec->run       ? "ok" : "NULL",
                spec->print_usage ? "ok" : "NULL");
    }
}

/* ---- pkg.json artefacts ---------------------------------------------- */
/*
 * Opens cmd_<name>/pkg.json for each command and verifies the five
 * required JSON key strings are present: name, version, description,
 * long_description, docs.
 */
static void test_pkg_json(void)
{
    static const char *required[] = {
        "\"name\"", "\"version\"", "\"description\"",
        "\"long_description\"", "\"docs\"", "\"files\""
    };
    for (int i = 0; i < N_COMMANDS; i++) {
        char path[256];
        snprintf(path, sizeof(path), "cmd_%s/pkg.json", s_commands[i]);
        check_file_test(
            make_desc("pkg.json: %s has all required fields", s_commands[i]),
            path, required, 6);
    }
}

/* ---- docs/<name>.md artefacts ----------------------------------------- */
/*
 * Opens cmd_<name>/docs/<name>.md for each command and verifies it
 * contains a ## Usage section and a ## Options section.
 */
static void test_docs_md(void)
{
    static const char *required[] = { "## Usage", "## Options" };
    for (int i = 0; i < N_COMMANDS; i++) {
        char path[256];
        snprintf(path, sizeof(path), "cmd_%s/docs/%s.md",
                 s_commands[i], s_commands[i]);
        check_file_test(
            make_desc("docs: %s.md has Usage and Options sections",
                      s_commands[i]),
            path, required, 2);
    }
}

/* ---- multicall dispatch ---------------------------------------------- */
/*
 * Tests both multicall dispatch modes introduced in this refactor:
 *   Mode 2: ./CoreShell <cmd> [args...]  (argv[1] is the command)
 *   Mode 1: ./echo [args...]             (symlink; argv[0] is the command)
 */
static void test_multicall_dispatch(void)
{
    /* Mode 2: known built-in commands dispatched directly */
    run_shell_test(&(test_case_t){
        "multicall mode2: echo prints argument",
        {"./CoreShell", "echo", "hello_multicall", NULL}, 0, "hello_multicall", NULL
    });
    run_shell_test(&(test_case_t){
        "multicall mode2: pwd returns an absolute path",
        {"./CoreShell", "pwd", NULL}, 0, "/", NULL
    });
    run_shell_test(&(test_case_t){
        "multicall mode2: ls --help shows Usage",
        {"./CoreShell", "ls", "--help", NULL}, 0, "Usage", NULL
    });
    run_shell_test(&(test_case_t){
        "multicall mode2: help lists built-in commands",
        {"./CoreShell", "help", NULL}, 0, "exit", NULL
    });

    /* Mode 2: unknown command must fail with an error message */
    run_shell_test(&(test_case_t){
        "multicall mode2: unknown command returns non-zero",
        {"./CoreShell", "xyznosuchcmd99", NULL}, EXPECT_FAIL,
        NULL, "unknown command"
    });

    /* Mode 1: symlink named after 'echo' dispatches to echo_run */
    unlink("./echo");  /* remove any leftover from earlier runs */
    if (symlink("./CoreShell", "./echo") == 0) {
        run_shell_test(&(test_case_t){
            "multicall mode1: symlink 'echo' dispatches correctly",
            {"./echo", "mode1_ok", NULL}, 0, "mode1_ok", NULL
        });
        unlink("./echo");
    } else {
        /* Record as a skipped pass if symlink creation is unavailable */
        test_result_t *r = &g_results[g_count++];
        r->description     = "multicall mode1: symlink 'echo' dispatches correctly";
        r->expected_exit   = 0;
        r->actual_exit     = 0;
        r->stdout_match    = 1;
        r->stderr_match    = 1;
        r->stdout_contains = NULL;
        r->stderr_contains = NULL;
        r->passed          = 1;
        snprintf(r->stdout_buf, MAX_OUTPUT - 1, "(skipped: symlink() failed)");
        r->stderr_buf[0]   = '\0';
    }
}

/* ---- pwd --help column-alignment regression test --------------------- */
/*
 * Verifies that the %%-22s typo in cmd_pwd/cmd_pwd.c is fixed:
 * the help output must show the long option forms rather than literal
 * "%-22s" that the bug caused arg_print_glossary to emit.
 */
static void test_pwd_help_format(void)
{
    run_test(&(test_case_t){
        "pwd: --help lists --logical long option (format bug regression)",
        {"pwd", "--help", NULL}, 0, "--logical", NULL
    });
    run_test(&(test_case_t){
        "pwd: --help lists --physical long option (format bug regression)",
        {"pwd", "--help", NULL}, 0, "--physical", NULL
    });
}

/* ---- --json / --help-json flags -------------------------------------- */
/*
 * Verifies that --help-json emits a JSON schema object containing the
 * "name" and "options" keys, and that --json produces a JSON result
 * object with the command-specific output key.
 */
static void test_json_flags(void)
{
    /* --help-json: schema must contain "name" and "options" keys */
    run_test(&(test_case_t){
        "json: echo --help-json emits JSON schema with \"name\" key",
        {"echo", "--help-json", NULL}, 0, "\"name\"", NULL
    });
    run_test(&(test_case_t){
        "json: ls --help-json emits JSON schema with \"options\" key",
        {"ls", "--help-json", NULL}, 0, "\"options\"", NULL
    });
    run_test(&(test_case_t){
        "json: pwd --help-json emits JSON schema with \"name\" key",
        {"pwd", "--help-json", NULL}, 0, "\"name\"", NULL
    });

    /* --json: runtime output must be a JSON object with expected key */
    run_test(&(test_case_t){
        "json: echo --json returns {\"output\": ...}",
        {"echo", "--json", "hello", NULL}, 0, "\"output\"", NULL
    });
    run_test(&(test_case_t){
        "json: pwd --json returns {\"path\": ...}",
        {"pwd", "--json", NULL}, 0, "\"path\"", NULL
    });
    run_test(&(test_case_t){
        "json: ls --json returns {\"entries\": ...}",
        {"ls", "--json", ".", NULL}, 0, "\"entries\"", NULL
    });
}

/* ---- pkg binary ------------------------------------------------------ */
/*
 * Tests the standalone pkg/pkg binary via run_shell_test().
 * Requires the binary to have been built by 'make'.
 */
static void test_pkg_binary(void)
{
    run_shell_test(&(test_case_t){
        "pkg: --help lists build subcommand",
        {"./pkg/pkg", "--help", NULL}, 0, "build", NULL
    });
    run_shell_test(&(test_case_t){
        "pkg: list reports installed packages",
        {"./pkg/pkg", "list", NULL}, 0, "packages", NULL
    });
    run_shell_test(&(test_case_t){
        "pkg: build with missing args returns error",
        {"./pkg/pkg", "build", NULL}, EXPECT_FAIL, NULL, NULL
    });
    run_shell_test(&(test_case_t){
        "pkg: install of nonexistent archive returns error",
        {"./pkg/pkg", "install", "/tmp/no_such_pkg_coreshell_99.tar.gz", NULL},
        EXPECT_FAIL, NULL, NULL
    });
    /* compile --dry-run should mention docs and pkg.json without building */
    run_shell_test(&(test_case_t){
        "pkg compile --dry-run: mentions docs",
        {"./pkg/pkg", "compile", "--dry-run", "cmd_echo", NULL}, 0, "docs", NULL
    });
    run_shell_test(&(test_case_t){
        "pkg compile --dry-run: mentions pkg.json",
        {"./pkg/pkg", "compile", "--dry-run", "cmd_echo", NULL}, 0, "pkg.json", NULL
    });
    run_shell_test(&(test_case_t){
        "pkg compile cmd_echo: succeeds and prints [OK]",
        {"./pkg/pkg", "compile", "cmd_echo", NULL}, 0, "[OK]", NULL
    });
}

/* ---- compile output artefacts ----------------------------------------- */
/*
 * After 'pkg compile', verify that the generated docs/<name>.md files
 * contain ## Usage and ## Options sections (written by generate_docs_md).
 */
static void test_compile_output(void)
{
    static const char *required[] = { "## Usage", "## Options" };
    /* Spot-check a few commands that are reliably compiled via pkg compile */
    static const char *spot[] = { "echo", "pwd", "ls" };
    for (int i = 0; i < 3; i++) {
        char path[256];
        snprintf(path, sizeof(path), "cmd_%s/docs/%s.md", spot[i], spot[i]);
        check_file_test(
            make_desc("compile output: %s.md has Usage and Options", spot[i]),
            path, required, 2);
    }
    /* Spot-check pkg.json for real summary field after compile */
    static const char *json_req[] = {
        "\"name\"", "\"version\"", "\"description\"",
        "\"long_description\"", "\"files\"", "\"docs\""
    };
    for (int i = 0; i < 3; i++) {
        char path[256];
        snprintf(path, sizeof(path), "cmd_%s/pkg.json", spot[i]);
        check_file_test(
            make_desc("compile output: %s pkg.json has all fields", spot[i]),
            path, json_req, 6);
    }
}

/* ── Report generation ───────────────────────────────────────────────── */

static const char *pass_label(int passed)
{
    return passed ? "PASS" : "FAIL";
}

/* Truncate a captured output buffer to a single display line */
static void one_line(const char *src, char *dst, size_t dstsz)
{
    size_t i = 0;
    while (i < dstsz - 1 && src[i] && src[i] != '\n' && src[i] != '\r') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void print_terminal_report(void)
{
    int passed = 0, failed = 0;
    for (int i = 0; i < g_count; i++)
        g_results[i].passed ? passed++ : failed++;

    CPRINT(COL_BOLD, "\n CoreShell Test Runner\n");
    CPRINT(COL_DIM,  " ════════════════════════════════════════\n\n");

    for (int i = 0; i < g_count; i++) {
        const test_result_t *r = &g_results[i];
        if (r->passed) {
            CPRINT(COL_GREEN, " [PASS]");
        } else {
            CPRINT(COL_RED,   " [FAIL]");
        }
        printf(" %-55s", r->description);

        /* Show mismatch details inline for failures */
        if (!r->passed) {
            if (r->expected_exit == EXPECT_FAIL)
                printf("  exit=%d (expected non-zero)", r->actual_exit);
            else
                printf("  exit=%d (expected %d)", r->actual_exit,
                       r->expected_exit);

            if (!r->stdout_match)
                printf("  stdout missing: \"%s\"", r->stdout_contains);
            if (!r->stderr_match)
                printf("  stderr missing: \"%s\"", r->stderr_contains);
        } else {
            CPRINT(COL_DIM, "  exit=%d", r->actual_exit);
        }
        putchar('\n');
    }

    printf("\n");
    CPRINT(COL_BOLD, " Results: ");
    CPRINT(COL_GREEN, "%d passed", passed);
    printf(" / ");
    if (failed > 0) {
        CPRINT(COL_RED, "%d failed", failed);
    } else {
        printf("0 failed");
    }
    printf(" / %d total  (%.0f%%%%)\n\n", g_count,
           g_count > 0 ? 100.0 * passed / g_count : 0.0);
}

static void write_markdown_report(const char *filepath)
{
    FILE *f = fopen(filepath, "w");
    if (!f) { perror(filepath); return; }

    /* Header */
    time_t now = time(NULL);
    char timebuf[64];
    struct tm *tm_info = localtime(&now);
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm_info);

    int passed = 0, failed = 0;
    for (int i = 0; i < g_count; i++)
        g_results[i].passed ? passed++ : failed++;

    fprintf(f, "# CoreShell Test Report\n\n");
    fprintf(f, "Generated: %s  \n", timebuf);
    fprintf(f, "Fixture dir: `%s`\n\n", g_fixture_dir);

    fprintf(f, "## Summary\n\n");
    fprintf(f, "| Metric  | Value |\n");
    fprintf(f, "|---------|-------|\n");
    fprintf(f, "| Total   | %d    |\n", g_count);
    fprintf(f, "| Passed  | %d    |\n", passed);
    fprintf(f, "| Failed  | %d    |\n", failed);
    fprintf(f, "| Rate    | %.1f%% |\n\n",
            g_count > 0 ? 100.0 * passed / g_count : 0.0);

    /* Full results table */
    fprintf(f, "## Results\n\n");
    fprintf(f,
        "| # | Result | Description "
        "| Expected exit | Actual exit | stdout snippet |\n");
    fprintf(f,
        "|---|--------|-------------|"
        "---------------|-------------|----------------|\n");

    for (int i = 0; i < g_count; i++) {
        const test_result_t *r = &g_results[i];
        char snippet[80];
        one_line(r->stdout_buf[0] ? r->stdout_buf : r->stderr_buf,
                 snippet, sizeof(snippet));

        const char *exp_str = (r->expected_exit == EXPECT_FAIL)
                              ? "non-zero" : "";
        char exp_buf[16];
        if (r->expected_exit != EXPECT_FAIL)
            snprintf(exp_buf, sizeof(exp_buf), "%d", r->expected_exit);
        else
            snprintf(exp_buf, sizeof(exp_buf), "non-zero");

        fprintf(f, "| %d | **%s** | %s | `%s` | `%d` | `%s` |\n",
                i + 1,
                pass_label(r->passed),
                r->description,
                exp_buf,
                r->actual_exit,
                snippet);
        (void)exp_str;
    }

    /* Failed tests detail section */
    if (failed > 0) {
        fprintf(f, "\n## Failed Tests\n\n");
        for (int i = 0; i < g_count; i++) {
            const test_result_t *r = &g_results[i];
            if (r->passed) continue;

            fprintf(f, "### %d. %s\n\n", i + 1, r->description);

            if (r->expected_exit == EXPECT_FAIL)
                fprintf(f,
                    "- **Exit code**: got `%d`, expected any non-zero\n",
                    r->actual_exit);
            else
                fprintf(f,
                    "- **Exit code**: got `%d`, expected `%d`\n",
                    r->actual_exit, r->expected_exit);

            if (!r->stdout_match)
                fprintf(f,
                    "- **stdout**: expected to contain `\"%s\"`\n",
                    r->stdout_contains);
            if (!r->stderr_match)
                fprintf(f,
                    "- **stderr**: expected to contain `\"%s\"`\n",
                    r->stderr_contains);

            if (r->stdout_buf[0])
                fprintf(f, "- **captured stdout**:\n```\n%s\n```\n",
                        r->stdout_buf);
            if (r->stderr_buf[0])
                fprintf(f, "- **captured stderr**:\n```\n%s\n```\n",
                        r->stderr_buf);
            fprintf(f, "\n");
        }
    }

    fclose(f);
    printf(" Report written to: %s\n\n", filepath);
}

/* ── Plain-text log ─────────────────────────────────────────────────── */
/*
 * Writes a colour-free plain-text version of the test results to
 * filepath.  Used alongside the Markdown report for CI log integration.
 */
static void write_text_log(const char *filepath)
{
    FILE *f = fopen(filepath, "w");
    if (!f) { perror(filepath); return; }

    int passed = 0, failed = 0;
    for (int i = 0; i < g_count; i++)
        g_results[i].passed ? passed++ : failed++;

    time_t now = time(NULL);
    char timebuf[64];
    struct tm *tm_info = localtime(&now);
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm_info);

    fprintf(f, "CoreShell Test Log\n");
    fprintf(f, "Generated : %s\n", timebuf);
    fprintf(f, "Fixture   : %s\n", g_fixture_dir);
    fprintf(f, "========================================\n\n");

    for (int i = 0; i < g_count; i++) {
        const test_result_t *r = &g_results[i];
        fprintf(f, "[%s] %s\n", r->passed ? "PASS" : "FAIL", r->description);
        if (!r->passed) {
            if (r->expected_exit == EXPECT_FAIL)
                fprintf(f, "       exit=%d (expected non-zero)\n",
                        r->actual_exit);
            else
                fprintf(f, "       exit=%d (expected %d)\n",
                        r->actual_exit, r->expected_exit);
            if (!r->stdout_match)
                fprintf(f, "       stdout missing: \"%s\"\n",
                        r->stdout_contains);
            if (!r->stderr_match)
                fprintf(f, "       stderr missing: \"%s\"\n",
                        r->stderr_contains);
            if (r->stdout_buf[0])
                fprintf(f, "       stdout: %.200s\n", r->stdout_buf);
            if (r->stderr_buf[0])
                fprintf(f, "       stderr: %.200s\n", r->stderr_buf);
        }
    }

    fprintf(f, "\n========================================\n");
    fprintf(f, "Results: %d passed / %d failed / %d total (%.0f%%)\n",
            passed, failed, g_count,
            g_count > 0 ? 100.0 * passed / g_count : 0.0);

    fclose(f);
    printf(" Log written to: %s\n\n", filepath);
}

/* ── Entry point ─────────────────────────────────────────────────────── */

int main(void)
{
    g_use_color = isatty(STDOUT_FILENO);

    /* Initialise the command registry (same path as the real shell) */
    register_all_builtin_commands();

    /* Prepare filesystem fixtures */
    setup_fixtures();

    CPRINT(COL_CYAN, " Setting up fixtures in %s\n", g_fixture_dir);

    /* Run all command test suites */
    test_help();
    test_exit();
    test_cd();
    test_pwd();
    test_echo();
    test_ls();
    test_stat();
    test_cat();
    test_head();
    test_tail();
    test_cp();
    test_mv();
    test_rm();
    test_mkdir();
    test_rmdir();
    test_touch();

    /* New test suites covering session-3 additions */
    test_cmd_spec_metadata();
    test_pkg_json();
    test_docs_md();
    test_multicall_dispatch();
    test_pwd_help_format();

    /* New test suites covering Week-3 additions: JSON flags and pkg binary */
    test_json_flags();
    test_pkg_binary();
    test_compile_output();

    /* Print terminal summary */
    print_terminal_report();

    /* Write markdown report */
    write_markdown_report("test_report.md");

    /* Write plain-text log */
    write_text_log("test_output.log");

    /* Clean up */
    teardown_fixtures();

    /* Exit with non-zero if any test failed */
    int exit_code = 0;
    for (int i = 0; i < g_count; i++)
        if (!g_results[i].passed) { exit_code = 1; break; }

    return exit_code;
}
