#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <libgen.h>
#include <pthread.h>
#include "Absyn.h"
#include "Parser.h"
#include "Printer.h"
#include "cmd_registry.h"
#include "cmd_jobs.h"

#define BUFFER_SIZE 1024
#define MAX_ARGS    64
#define MAX_PIPE_CMDS 16

/* ── multicall helpers ─────────────────────────────────────────────────── */

/* Return the basename of path without modifying it.
   Uses a static buffer — suitable only for one call at a time. */
static const char *argv0_basename(const char *path)
{
    /* basename(3) may modify its argument; work on a copy */
    static char buf[256];
    strncpy(buf, path, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    /* basename() takes a file path string and returns only the last component of that path 
      — everything after the final /. */
    return basename(buf); 
}

/* Called in multicall mode when no command matches. */
static int unknown_command(const char *cmd)
{
    fprintf(stderr, "CoreShell: unknown command '%s'\n", cmd);
    fprintf(stderr, "Run 'CoreShell' with no arguments to enter the interactive shell.\n");
    return EXIT_FAILURE;
}

/* Parse common shell syntax through BNFC first, then render it back into a
   normalized string. The legacy tokenizer still handles quote-heavy lines
   and any syntax that falls outside the BNFC grammar. */
static const char *normalize_line_with_bnfc(const char *line)
{
    Input ast = psInput(line);
    const char *rendered;

    if (ast == NULL)
        return NULL;

    /* printInput returns Printer.c's internal reusable buffer;
       caller must NOT free the returned pointer. */
    rendered = printInput(ast);
    free_Input(ast);
    return rendered;
}

/* ── job table ─────────────────────────────────────────────────────────── */

#define MAX_JOBS 64

static bg_job_t  s_jobs[MAX_JOBS];
static int       s_job_count = 0;  /* total slots used (never shrinks in this session) */
static int       s_next_job_id = 1;

/* Set by SIGCHLD handler — checked in the REPL loop to print notifications */
static volatile sig_atomic_t g_sigchld = 0;

/* Add a background job.  Returns the assigned job_id, or -1 if full. */
int job_add(pid_t pid, const char *cmd)
{
    for (int i = 0; i < MAX_JOBS; i++)
    {
        if (s_jobs[i].pid == 0)
        {
            s_jobs[i].pid        = pid;
            s_jobs[i].job_id     = s_next_job_id++;
            s_jobs[i].state      = JOB_RUNNING;
            s_jobs[i].exit_status = 0;
            strncpy(s_jobs[i].cmd, cmd ? cmd : "?", sizeof(s_jobs[i].cmd) - 1);
            s_jobs[i].cmd[sizeof(s_jobs[i].cmd) - 1] = '\0';
            if (i >= s_job_count) s_job_count = i + 1;
            return s_jobs[i].job_id;
        }
    }
    return -1; /* table full */
}

/* Look up a job by pid.  Returns NULL if not found. */
bg_job_t *job_by_pid(pid_t pid)
{
    for (int i = 0; i < s_job_count; i++)
        if (s_jobs[i].pid == pid) return &s_jobs[i];
    return NULL;
}

/* Look up a job by 1-based job_id.  Returns NULL if not found. */
bg_job_t *job_by_id(int job_id)
{
    for (int i = 0; i < s_job_count; i++)
        if (s_jobs[i].pid != 0 && s_jobs[i].job_id == job_id)
            return &s_jobs[i];
    return NULL;
}

/* Return a view of the job table for the jobs builtin. */
bg_job_t *job_table(int *count_out)
{
    *count_out = s_job_count;
    return s_jobs;
}

/* Print and clear all completed jobs (called at each prompt). */
static void notify_done_jobs(void)
{
    for (int i = 0; i < s_job_count; i++)
    {
        if (s_jobs[i].pid == 0) continue;
        if (s_jobs[i].state == JOB_DONE)
        {
            printf("[%d] Done\t\t%s\n", s_jobs[i].job_id, s_jobs[i].cmd);
            s_jobs[i].pid = 0; /* free slot */
        }
        else if (s_jobs[i].state == JOB_SIGNALED)
        {
            printf("[%d] Killed (%d)\t%s\n",
                   s_jobs[i].job_id, s_jobs[i].exit_status, s_jobs[i].cmd);
            s_jobs[i].pid = 0;
        }
    }
}

/* ── signal handling ───────────────────────────────────────────────────── */

/* Set by SIGINT handler; checked in the REPL loop */
static volatile sig_atomic_t g_sigint = 0;

/* SIGINT handler: does NOT exit the shell (AGENTS.md requirement).
   Uses only async-signal-safe functions (write). */
static void signal_handler(int sig)
{
    (void)sig;
    g_sigint = 1;
    /* Write a newline so the next prompt starts on a fresh line */
    write(STDOUT_FILENO, "\n", 1);
}

/* SIGCHLD handler: reap only tracked background children without blocking.
   Foreground children are waited by their caller paths (waitpid in foreground
   dispatch/pipeline), so they must not be consumed here. */
static void sigchld_handler(int sig)
{
    (void)sig;
    int saved_errno = errno;
    int status;

    for (int i = 0; i < s_job_count; i++)
    {
        bg_job_t *j = &s_jobs[i];
        if (j->pid == 0 || j->state != JOB_RUNNING)
            continue;

        pid_t rc = waitpid(j->pid, &status, WNOHANG);
        if (rc == j->pid)
        {
            if (WIFEXITED(status))
            {
                j->state       = JOB_DONE;
                j->exit_status = WEXITSTATUS(status);
            }
            else if (WIFSIGNALED(status))
            {
                j->state       = JOB_SIGNALED;
                j->exit_status = WTERMSIG(status);
            }
            g_sigchld = 1;
        }
    }
    errno = saved_errno;
}

/* ── input ─────────────────────────────────────────────────────────────── */

/* Parse a command line into an argument array.
   Returns the number of arguments found. */
static int parse_command(char *input, char *args[])
{
    int   i     = 0;
    char *token = strtok(input, " \t\n");

    while (token != NULL && i < MAX_ARGS - 1)
    {
        args[i++] = token;
        token = strtok(NULL, " \t\n");
    }
    args[i] = NULL;
    return i;
}

/* ── input ─────────────────────────────────────────────────────────────── */

/* Read one line from stdin.
   Returns a heap-allocated string (caller must free), or NULL on Ctrl-C
   (caller should re-prompt).  Exits on Ctrl-D (EOF) or unrecoverable error. */
static char *read_input(void)
{
    char *input = malloc(BUFFER_SIZE);
    if (input == NULL)
    {
        fprintf(stderr, "malloc: out of memory\n");
        exit(EXIT_FAILURE);
    }

    if (fgets(input, BUFFER_SIZE, stdin) == NULL)
    {
        free(input);
        if (feof(stdin))
        {
            printf("\n");
            exit(EXIT_SUCCESS); /* Ctrl-D: clean exit */
        }
        if (g_sigint)
        {
            clearerr(stdin);
            return NULL; /* Ctrl-C: signal caller to re-prompt */
        }
        exit(EXIT_FAILURE);
    }

    /* Strip trailing newline */
    input[strcspn(input, "\n")] = '\0';
    return input;
}

/* ── command dispatch ──────────────────────────────────────────────────── */

typedef struct
{
    const cmd_spec_t *spec;
    int argc;
    char **argv;
    int status_fd;
} builtin_thread_ctx_t;

static void *builtin_thread_main(void *arg)
{
    builtin_thread_ctx_t *ctx = (builtin_thread_ctx_t *)arg;
    int status = ctx->spec->run(ctx->argc, ctx->argv);
    (void)write(ctx->status_fd, &status, sizeof(status));
    close(ctx->status_fd);
    return NULL;
}

/* Dispatch a built-in command by name.  Returns the command's exit code,
   or 1 if the command is not found. */
static int dispatch_builtin(int argc, char *argv[])
{
    if (argv[0] == NULL)
        return 0;

    const char *cmd_name = argv[0];
    const cmd_spec_t *spec = find_command(cmd_name);
    if (spec == NULL)
    {
        const char *base = argv0_basename(argv[0]);
        if (base != NULL)
        {
            spec = find_command(base);
            if (spec != NULL)
                cmd_name = base;
        }
    }

    if (spec != NULL)
    {
        /* Assignment mode: internal commands run in a pthread and return
           status through a system pipe. */
        int status_pipe[2];
        if (pipe(status_pipe) < 0)
        {
            perror("pipe");
            return 1;
        }

        builtin_thread_ctx_t ctx;
        ctx.spec = spec;
        ctx.argc = argc;
        char *argv_local[MAX_ARGS];
        if (cmd_name != argv[0] && argc > 0 && argc < MAX_ARGS)
        {
            for (int i = 0; i < argc; i++)
                argv_local[i] = argv[i];
            argv_local[argc] = NULL;
            argv_local[0] = (char *)cmd_name;
            ctx.argv = argv_local;
        }
        else
        {
            ctx.argv = argv;
        }
        ctx.status_fd = status_pipe[1];

        pthread_t tid;
        int prc = pthread_create(&tid, NULL, builtin_thread_main, &ctx);
        if (prc != 0)
        {
            fprintf(stderr, "pthread_create: %s\n", strerror(prc));
            close(status_pipe[0]);
            close(status_pipe[1]);
            return 1;
        }

        int status = 1;
        ssize_t n = read(status_pipe[0], &status, sizeof(status));
        close(status_pipe[0]);

        prc = pthread_join(tid, NULL);
        if (prc != 0)
        {
            fprintf(stderr, "pthread_join: %s\n", strerror(prc));
            close(status_pipe[1]);
            return 1;
        }

        close(status_pipe[1]);

        if (n == (ssize_t)sizeof(status))
            return status;

        return 1;
    }

    fprintf(stderr, "CoreShell: unknown command '%s'\n", argv[0]);
    return 1;
}

/* Run an external command via fork/execvp and wait for completion. */
static int dispatch_external(int argc, char *argv[])
{
    (void)argc;

    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork");
        return 1;
    }

    if (pid == 0)
    {
        execvp(argv[0], argv);
        if (errno == ENOENT && strchr(argv[0], '/') == NULL)
            fprintf(stderr, "CoreShell: unknown command '%s'\n", argv[0]);
        else
            perror(argv[0]);
        _exit(127);
    }

    int status;
    if (waitpid(pid, &status, 0) < 0)
    {
        perror("waitpid");
        return 1;
    }

    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    if (WIFSIGNALED(status))
        return 128 + WTERMSIG(status);
    return 1;
}

/* Dispatch a command as either built-in or external. */
static int dispatch_command(int argc, char *argv[])
{
    if (argv[0] == NULL)
        return 0;

    const cmd_spec_t *spec = find_command(argv[0]);
    if (spec == NULL)
    {
        const char *base = argv0_basename(argv[0]);
        if (base != NULL)
            spec = find_command(base);
    }
    if (spec != NULL)
        return dispatch_builtin(argc, argv);

    return dispatch_external(argc, argv);
}

/* Execute one stage inside a pipeline child process. */
static void exec_pipeline_stage(int argc, char *argv[])
{
    if (argv[0] == NULL)
        _exit(0);

    const cmd_spec_t *spec = find_command(argv[0]);
    if (spec != NULL)
    {
        int rc = spec->run(argc, argv);
        fflush(stdout);
        fflush(stderr);
        _exit(rc);
    }

    execvp(argv[0], argv);
    perror(argv[0]);
    _exit(127);
}

/* Expand $VAR and ${VAR} in src into dst (size dst_size).
   Quoted sections are passed through verbatim (expansion still happens
   inside double quotes, but single-quote content is left as-is).
   Returns 0 on success, 1 on overflow or syntax error. */
static int expand_variables_line(const char *src, char *dst, size_t dst_size)
{
    size_t out = 0;
    char quote = 0;

#define EMIT(ch) do { if (out + 1 >= dst_size) { fprintf(stderr, "CoreShell: command too long\n"); return 1; } dst[out++] = (ch); } while(0)
#define EMIT_STR(s) do { for (const char *_p = (s); *_p; _p++) { EMIT(*_p); } } while(0)

    for (const char *p = src; *p; )
    {
        if (quote == '\'')
        {
            if (*p == '\'') { quote = 0; EMIT(*p++); }
            else EMIT(*p++);
            continue;
        }

        if (*p == '\'' && quote == 0)
        {
            quote = '\'';
            EMIT(*p++);
            continue;
        }

        if (*p == '"')
        {
            if (quote == '"') quote = 0; else quote = '"';
            EMIT(*p++);
            continue;
        }

        /* Expand $ in unquoted context OR inside double quotes */
        if (*p == '$' && quote != '\'')
        {
            p++;
            int braced = (*p == '{');
            if (braced) p++;
            char vname[128];
            size_t vlen = 0;
            while (*p && (isalnum((unsigned char)*p) || *p == '_') && vlen < sizeof(vname) - 1)
                vname[vlen++] = *p++;
            vname[vlen] = '\0';
            if (braced)
            {
                if (*p == '}') p++;
                else { fprintf(stderr, "CoreShell: missing }\n"); return 1; }
            }
            const char *val = vlen ? getenv(vname) : "$";
            if (val) EMIT_STR(val);
            continue;
        }

        EMIT(*p++);
    }
#undef EMIT
#undef EMIT_STR
    dst[out] = '\0';
    return 0;
}

/* Trim leading and trailing spaces/tabs in-place. */
static char *trim_ws(char *s)
{
    while (*s == ' ' || *s == '\t')
        s++;

    char *end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t'))
        *--end = '\0';

    return s;
}

typedef struct
{
    int kind;
    char *target;
} redir_spec_t;

enum
{
    REDIR_IN = 1,
    REDIR_OUT_TRUNC,
    REDIR_OUT_APPEND,
    REDIR_ERR_TRUNC,
    REDIR_ERR_APPEND,
    REDIR_ERR_TO_OUT
};

typedef struct
{
    char *argv[MAX_ARGS];
    int argc;
    redir_spec_t redirs[MAX_ARGS];
    int redir_count;
    char storage[BUFFER_SIZE];
    size_t storage_used;
} stage_cmd_t;

/* Return nonzero if p starts with a redirection operator token. */
static int is_redir_start(const char *p)
{
    return *p == '<' || *p == '>' || (*p == '2' && *(p + 1) == '>');
}

/* Return true if this stage contains any stdin redirection. */
static int stage_has_stdin_redir(const stage_cmd_t *s)
{
    for (int i = 0; i < s->redir_count; i++)
    {
        if (s->redirs[i].kind == REDIR_IN)
            return 1;
    }
    return 0;
}

/* Return true if this stage contains any stdout file redirection. */
static int stage_has_stdout_file_redir(const stage_cmd_t *s)
{
    for (int i = 0; i < s->redir_count; i++)
    {
        if (s->redirs[i].kind == REDIR_OUT_TRUNC || s->redirs[i].kind == REDIR_OUT_APPEND)
            return 1;
    }
    return 0;
}

/* Copy token bytes into stage-owned storage and return a stable pointer. */
static char *store_token(stage_cmd_t *out, const char *start, size_t len)
{
    if (len == 0)
        return NULL;

    if (out->storage_used + len + 1 > sizeof(out->storage))
    {
        fprintf(stderr, "CoreShell: command too long\n");
        return NULL;
    }

    char *dst = out->storage + out->storage_used;
    memcpy(dst, start, len);
    dst[len] = '\0';
    out->storage_used += len + 1;
    return dst;
}

/* Parse one shell-like word token with basic quote handling. */
static int parse_word_token(const char **pp, stage_cmd_t *out, char **word_out)
{
    const char *p = *pp;
    char tmp[BUFFER_SIZE];
    size_t len = 0;

    while (*p && isspace((unsigned char)*p))
        p++;

    if (*p == '\0')
    {
        *pp = p;
        *word_out = NULL;
        return 0;
    }

    while (*p && !isspace((unsigned char)*p) && !is_redir_start(p))
    {
        if (*p == '"' || *p == '\'')
        {
            char quote = *p++;
            while (*p && *p != quote)
            {
                /* $VAR expansion inside double quotes */
                if (quote == '"' && *p == '$')
                {
                    p++;
                    int braced = (*p == '{');
                    if (braced) p++;
                    char vname[128];
                    size_t vlen = 0;
                    while (*p && (isalnum((unsigned char)*p) || *p == '_') && vlen < sizeof(vname) - 1)
                        vname[vlen++] = *p++;
                    vname[vlen] = '\0';
                    if (braced)
                    {
                        if (*p == '}') p++;
                        else { fprintf(stderr, "CoreShell: missing }\n"); return 1; }
                    }
                    const char *val = vlen ? getenv(vname) : NULL;
                    if (val)
                    {
                        size_t vl = strlen(val);
                        if (len + vl >= sizeof(tmp)) { fprintf(stderr, "CoreShell: command too long\n"); return 1; }
                        memcpy(tmp + len, val, vl);
                        len += vl;
                    }
                    continue;
                }
                if (quote == '"' && *p == '\\' && *(p + 1) != '\0')
                    p++;
                if (len + 1 >= sizeof(tmp))
                {
                    fprintf(stderr, "CoreShell: command too long\n");
                    return 1;
                }
                tmp[len++] = *p++;
            }
            if (*p != quote)
            {
                fprintf(stderr, "CoreShell: unmatched quote\n");
                return 1;
            }
            p++;
            continue;
        }

        /* $VAR expansion outside quotes */
        if (*p == '$')
        {
            p++;
            int braced = (*p == '{');
            if (braced) p++;
            char vname[128];
            size_t vlen = 0;
            while (*p && (isalnum((unsigned char)*p) || *p == '_') && vlen < sizeof(vname) - 1)
                vname[vlen++] = *p++;
            vname[vlen] = '\0';
            if (braced)
            {
                if (*p == '}') p++;
                else { fprintf(stderr, "CoreShell: missing }\n"); return 1; }
            }
            const char *val = vlen ? getenv(vname) : NULL;
            if (val)
            {
                size_t vl = strlen(val);
                if (len + vl >= sizeof(tmp)) { fprintf(stderr, "CoreShell: command too long\n"); return 1; }
                memcpy(tmp + len, val, vl);
                len += vl;
            }
            continue;
        }

        if (*p == '\\' && *(p + 1) != '\0')
            p++;

        if (len + 1 >= sizeof(tmp))
        {
            fprintf(stderr, "CoreShell: command too long\n");
            return 1;
        }
        tmp[len++] = *p++;
    }

    tmp[len] = '\0';
    *word_out = store_token(out, tmp, len);
    *pp = p;
    if (*word_out == NULL)
        return 1;
    return 0;
}

/* Add one redirection spec preserving left-to-right order. */
static int add_redir(stage_cmd_t *out, int kind, char *target)
{
    if (out->redir_count >= MAX_ARGS)
    {
        fprintf(stderr, "CoreShell: too many redirections\n");
        return 1;
    }

    out->redirs[out->redir_count].kind = kind;
    out->redirs[out->redir_count].target = target;
    out->redir_count++;
    return 0;
}

/* Parse one stage for argv plus optional < and > redirection. */
static int parse_stage(char *stage, stage_cmd_t *out)
{
    memset(out, 0, sizeof(*out));

    const char *p = stage;
    while (*p)
    {
        while (*p && isspace((unsigned char)*p))
            p++;
        if (*p == '\0')
            break;

        if (strncmp(p, "2>&1", 4) == 0)
        {
            if (add_redir(out, REDIR_ERR_TO_OUT, NULL) != 0)
                return 1;
            p += 4;
            continue;
        }

        if (strncmp(p, "2>>", 3) == 0)
        {
            p += 3;
            char *target = NULL;
            if (parse_word_token(&p, out, &target) != 0)
                return 1;
            if (target == NULL)
            {
                fprintf(stderr, "CoreShell: syntax error near unexpected token '2>>'\n");
                return 1;
            }
            if (add_redir(out, REDIR_ERR_APPEND, target) != 0)
                return 1;
            continue;
        }

        if (strncmp(p, "2>", 2) == 0)
        {
            p += 2;
            char *target = NULL;
            if (parse_word_token(&p, out, &target) != 0)
                return 1;
            if (target == NULL)
            {
                fprintf(stderr, "CoreShell: syntax error near unexpected token '2>'\n");
                return 1;
            }
            if (add_redir(out, REDIR_ERR_TRUNC, target) != 0)
                return 1;
            continue;
        }

        if (strncmp(p, ">>", 2) == 0)
        {
            p += 2;
            char *target = NULL;
            if (parse_word_token(&p, out, &target) != 0)
                return 1;
            if (target == NULL)
            {
                fprintf(stderr, "CoreShell: syntax error near unexpected token '>>'\n");
                return 1;
            }
            if (add_redir(out, REDIR_OUT_APPEND, target) != 0)
                return 1;
            continue;
        }

        if (*p == '>')
        {
            p += 1;
            char *target = NULL;
            if (parse_word_token(&p, out, &target) != 0)
                return 1;
            if (target == NULL)
            {
                fprintf(stderr, "CoreShell: syntax error near unexpected token '>'\n");
                return 1;
            }
            if (add_redir(out, REDIR_OUT_TRUNC, target) != 0)
                return 1;
            continue;
        }

        if (*p == '<')
        {
            p += 1;
            char *target = NULL;
            if (parse_word_token(&p, out, &target) != 0)
                return 1;
            if (target == NULL)
            {
                fprintf(stderr, "CoreShell: syntax error near unexpected token '<'\n");
                return 1;
            }
            if (add_redir(out, REDIR_IN, target) != 0)
                return 1;
            continue;
        }

        char *word = NULL;
        if (parse_word_token(&p, out, &word) != 0)
            return 1;
        if (word == NULL)
            continue;

        if (out->argc >= MAX_ARGS - 1)
        {
            fprintf(stderr, "CoreShell: too many arguments\n");
            return 1;
        }
        out->argv[out->argc++] = word;
    }

    out->argv[out->argc] = NULL;
    if (out->argc == 0)
    {
        fprintf(stderr, "CoreShell: invalid null command in pipeline\n");
        return 1;
    }

    return 0;
}

/* Split `line` on unquoted '|' characters, writing NUL terminators in place.
   Returns the number of stages, or -1 on error. */
static int split_pipeline_stages(char *line, char *stages[], int max_stages)
{
    int count = 0;
    char *start = line;
    char quote = 0;

    for (char *p = line; ; p++)
    {
        if (*p == '\0')
        {
            char *trimmed = trim_ws(start);
            if (*trimmed == '\0')
            {
                /* allow trailing empty only if no stages yet (blank line) */
                if (count == 0)
                    return 0;
                fprintf(stderr, "CoreShell: invalid null command in pipeline\n");
                return -1;
            }
            if (count >= max_stages)
            {
                fprintf(stderr, "CoreShell: pipeline too long (max %d stages)\n", max_stages);
                return -1;
            }
            stages[count++] = trimmed;
            break;
        }

        if (quote)
        {
            if (*p == quote)
                quote = 0;
        }
        else if (*p == '\'' || *p == '"')
        {
            quote = *p;
        }
        else if (*p == '|')
        {
            if (count >= max_stages)
            {
                fprintf(stderr, "CoreShell: pipeline too long (max %d stages)\n", max_stages);
                return -1;
            }
            *p = '\0';
            char *trimmed = trim_ws(start);
            if (*trimmed == '\0')
            {
                fprintf(stderr, "CoreShell: invalid null command in pipeline\n");
                return -1;
            }
            stages[count++] = trimmed;
            start = p + 1;
        }
    }

    return count;
}

/* Remove a trailing unquoted '&' from line (in-place, trims surrounding space).
   Returns 1 if background was requested, 0 otherwise. */
static int strip_trailing_ampersand(char *line)
{
    /* Walk to the end, ignoring quoted sections. */
    char quote = 0;
    char *last_amp = NULL;
    for (char *p = line; *p; p++)
    {
        if (quote)
        {
            if (*p == quote) quote = 0;
        }
        else if (*p == '\'' || *p == '"')
        {
            quote = *p;
            last_amp = NULL; /* reset: can't be trailing if quote follows */
        }
        else if (*p == '&')
        {
            last_amp = p;
        }
        else if (!isspace((unsigned char)*p))
        {
            last_amp = NULL; /* non-space non-& after &, not trailing */
        }
    }

    if (last_amp == NULL)
        return 0;

    /* Trim the '&' and any trailing whitespace before it. */
    *last_amp = '\0';
    char *end = last_amp;
    while (end > line && isspace((unsigned char)end[-1]))
        *--end = '\0';
    return 1;
}

/* Execute a pipeline command line with one or more '|' separators.
   background=1: don't wait for children, print [pid] instead. */
static int execute_pipeline(char *line)
{
    char *stages[MAX_PIPE_CMDS];
    stage_cmd_t cmdv[MAX_PIPE_CMDS];
    pid_t pids[MAX_PIPE_CMDS];

    int background = strip_trailing_ampersand(line);

    int stage_count = split_pipeline_stages(line, stages, MAX_PIPE_CMDS);
    if (stage_count <= 0)
        return stage_count == 0 ? 0 : 1;

    for (int i = 0; i < stage_count; i++)
    {
        if (parse_stage(stages[i], &cmdv[i]) != 0)
            return 1;
        if (i < stage_count - 1 && stage_has_stdout_file_redir(&cmdv[i]))
        {
            fprintf(stderr, "CoreShell: output redirection only supported on the last pipeline stage\n");
            return 1;
        }
        if (i > 0 && stage_has_stdin_redir(&cmdv[i]))
        {
            fprintf(stderr, "CoreShell: input redirection only supported on the first pipeline stage\n");
            return 1;
        }
    }

    int in_fd = STDIN_FILENO;
    int pipefd[2] = { -1, -1 };

    /* Block SIGCHLD around fork+job_add to prevent the handler from running
       before the new pid is registered in the job table. */
    sigset_t chld_mask, old_mask;
    sigemptyset(&chld_mask);
    sigaddset(&chld_mask, SIGCHLD);
    if (background)
        sigprocmask(SIG_BLOCK, &chld_mask, &old_mask);

    for (int i = 0; i < stage_count; i++)
    {
        if (i < stage_count - 1)
        {
            if (pipe(pipefd) < 0)
            {
                perror("pipe");
                if (in_fd != STDIN_FILENO)
                    close(in_fd);
                return 1;
            }
        }

        pid_t pid = fork();
        if (pid < 0)
        {
            perror("fork");
            if (in_fd != STDIN_FILENO)
                close(in_fd);
            if (i < stage_count - 1)
            {
                close(pipefd[0]);
                close(pipefd[1]);
            }
            return 1;
        }

        if (pid == 0)
        {
            if (in_fd != STDIN_FILENO)
            {
                if (dup2(in_fd, STDIN_FILENO) < 0)
                {
                    perror("dup2");
                    _exit(127);
                }
            }

            if (i < stage_count - 1)
            {
                if (dup2(pipefd[1], STDOUT_FILENO) < 0)
                {
                    perror("dup2");
                    _exit(127);
                }
            }

            for (int r = 0; r < cmdv[i].redir_count; r++)
            {
                int kind = cmdv[i].redirs[r].kind;
                char *target = cmdv[i].redirs[r].target;

                if (kind == REDIR_IN)
                {
                    int fd = open(target, O_RDONLY);
                    if (fd < 0)
                    {
                        perror(target);
                        _exit(1);
                    }
                    if (dup2(fd, STDIN_FILENO) < 0)
                    {
                        perror("dup2");
                        _exit(127);
                    }
                    close(fd);
                }
                else if (kind == REDIR_OUT_TRUNC || kind == REDIR_OUT_APPEND)
                {
                    int flags = O_WRONLY | O_CREAT | (kind == REDIR_OUT_APPEND ? O_APPEND : O_TRUNC);
                    int fd = open(target, flags, 0644);
                    if (fd < 0)
                    {
                        perror(target);
                        _exit(1);
                    }
                    if (dup2(fd, STDOUT_FILENO) < 0)
                    {
                        perror("dup2");
                        _exit(127);
                    }
                    close(fd);
                }
                else if (kind == REDIR_ERR_TRUNC || kind == REDIR_ERR_APPEND)
                {
                    int flags = O_WRONLY | O_CREAT | (kind == REDIR_ERR_APPEND ? O_APPEND : O_TRUNC);
                    int fd = open(target, flags, 0644);
                    if (fd < 0)
                    {
                        perror(target);
                        _exit(1);
                    }
                    if (dup2(fd, STDERR_FILENO) < 0)
                    {
                        perror("dup2");
                        _exit(127);
                    }
                    close(fd);
                }
                else if (kind == REDIR_ERR_TO_OUT)
                {
                    if (dup2(STDOUT_FILENO, STDERR_FILENO) < 0)
                    {
                        perror("dup2");
                        _exit(127);
                    }
                }
            }

            if (in_fd != STDIN_FILENO)
                close(in_fd);
            if (i < stage_count - 1)
            {
                close(pipefd[0]);
                close(pipefd[1]);
            }

            exec_pipeline_stage(cmdv[i].argc, cmdv[i].argv);
        }

        pids[i] = pid;

        if (in_fd != STDIN_FILENO)
            close(in_fd);

        if (i < stage_count - 1)
        {
            close(pipefd[1]);
            in_fd = pipefd[0];
        }
    }

    if (in_fd != STDIN_FILENO)
        close(in_fd);

    if (background)
    {
        /* Register each child in the job table; use the last pid as the job leader. */
        int job_id = -1;
        for (int i = 0; i < stage_count; i++)
        {
            /* Only the first stage owns the job entry; the rest share its slot. */
            if (i == 0)
                job_id = job_add(pids[i], line);
            /* For multi-stage pipelines the intermediate pids must also be tracked
               so the SIGCHLD reaper can find them — add as sub-entries. */
            else
                job_add(pids[i], line);
        }
        /* Now safe to let SIGCHLD through. */
        sigprocmask(SIG_SETMASK, &old_mask, NULL);

        if (job_id > 0)
            printf("[%d] %d\n", job_id, (int)pids[0]);
        else
            printf("[background] %d\n", (int)pids[0]);
        return 0;
    }

    /* Foreground: SIGCHLD was not blocked, no need to restore mask. */

    int final_status = 0;
    for (int i = 0; i < stage_count; i++)
    {
        int status;
        if (waitpid(pids[i], &status, 0) < 0)
        {
            perror("waitpid");
            final_status = 1;
            continue;
        }

        if (i == stage_count - 1)
        {
            if (WIFEXITED(status))
                final_status = WEXITSTATUS(status);
            else if (WIFSIGNALED(status))
                final_status = 128 + WTERMSIG(status);
            else
                final_status = 1;
        }
    }

    return final_status;
}

/* Execute a suggested shell command after user confirmation. */
static int execute_suggested_command(const char *command)
{
    int status = system(command);
    if (status == -1)
    {
        perror("system");
        return 1;
    }

    if (WIFEXITED(status))
        return WEXITSTATUS(status);

    if (WIFSIGNALED(status))
        return 128 + WTERMSIG(status);

    return 1;
}

/* ── LLM @ handling ────────────────────────────────────────────────────── */

/*
 * handle_llm_line - Process a natural language query prefixed with @
 *
 * This function:
 * 1. Forks and execs the external helper program 'coresh_llm' with the query.
 * 2. Reads the suggested shell command (one line) from coresh_llm's stdout.
 * 3. Displays the suggested command to the user.
 * 4. Asks for confirmation (Run this? (y/n)).
 * 5. If confirmed, parses and executes the command via dispatch_builtin.
 *
 * Input:  query - natural language string (without the '@' prefix)
 * Output: None (side effects: may execute a command, or print error/abort)
 */
static void handle_llm_line(const char *query)
{
    int pipe_fd[2];
    pid_t pid;

    /* Create a pipe for reading the child's stdout */
    if (pipe(pipe_fd) < 0)
    {
        perror("pipe");
        return;
    }

    /* Fork child process */
    pid = fork();
    if (pid < 0)
    {
        perror("fork");
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return;
    }

    if (pid == 0)
    {
        /* Child process: set up to call coresh_llm */
        close(pipe_fd[0]); /* Close read end in child */

        /* Redirect stdout to the pipe */
        if (dup2(pipe_fd[1], STDOUT_FILENO) < 0)
        {
            perror("dup2");
            _exit(127);
        }
        close(pipe_fd[1]);

        /* Try to find coresh_llm next to the CoreShell binary via /proc/self/exe. */
        {
            char self_path[4096];
            ssize_t len = readlink("/proc/self/exe", self_path, sizeof(self_path) - 1);
            if (len > 0) {
                self_path[len] = '\0';
                char *slash = strrchr(self_path, '/');
                if (slash) {
                    slash[1] = '\0';
                    strncat(self_path, "coresh_llm", sizeof(self_path) - strlen(self_path) - 1);
                    execv(self_path, (char *const[]){ "coresh_llm", (char *)query, NULL });
                }
            }
        }

        /* Fallback to PATH lookup. */
        execvp("coresh_llm", (char *const[]){ "coresh_llm", (char *)query, NULL });

        /* If execvp fails, exit with error code */
        perror("coresh_llm");
        _exit(127);
    }

    /* Parent process: read the suggested command */
    close(pipe_fd[1]); /* Close write end in parent */

    char suggested[BUFFER_SIZE];
    ssize_t n = read(pipe_fd[0], suggested, sizeof(suggested) - 1);
    close(pipe_fd[0]);

    /* Wait for the child to complete */
    int status;
    waitpid(pid, &status, 0);

    if (n <= 0)
    {
        fprintf(stderr, "CoreShell: no response from coresh_llm\n");
        return;
    }

    /* Remove trailing newline from suggested command */
    if (n > 0 && suggested[n - 1] == '\n')
        n--;
    suggested[n] = '\0';

    if (n == 0)
    {
        fprintf(stderr, "CoreShell: coresh_llm returned empty response\n");
        return;
    }

    /* Display the suggested command and ask for confirmation */
    printf("Suggested command: %s\n", suggested);
    printf("Run this? (y/n) ");
    fflush(stdout);

    char response[16];
    if (fgets(response, sizeof(response), stdin) == NULL)
    {
        fprintf(stderr, "\nCancelled.\n");
        return;
    }

    /* Check if user confirmed with 'y' or 'yes' */
    if (response[0] != 'y' && response[0] != 'Y')
    {
        printf("Cancelled.\n");
        return;
    }

    /* Execute the suggested command through the system shell so globbing
       and external commands behave the way the LLM suggested. */
    execute_suggested_command(suggested);
}

/* Collect an @ query from argv[1..] for direct command-line invocation. */
static char *join_llm_query(int argc, char *argv[])
{
    size_t total = 0;
    for (int i = 1; i < argc; i++)
        total += strlen(argv[i]) + 1;

    char *query = malloc(total + 1);
    if (query == NULL)
        return NULL;

    query[0] = '\0';
    for (int i = 1; i < argc; i++)
    {
        if (i > 1)
            strcat(query, " ");

        if (i == 1 && argv[i][0] == '@')
            strcat(query, argv[i] + 1);
        else
            strcat(query, argv[i]);
    }

    return query;
}

/* ── main ──────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    signal(SIGINT, signal_handler);

    /* Install SIGCHLD handler to reap background children. */
    {
        struct sigaction sa;
        sa.sa_handler = sigchld_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
        sigaction(SIGCHLD, &sa, NULL);
    }

    /* Populate the command registry */
    register_all_builtin_commands();

    /* Prepend ~/.CoreShell/bin to PATH so installed packages are found */
    {
        const char *home = getenv("HOME");
        const char *path = getenv("PATH");
        if (home && path)
        {
            char newpath[4096];
            snprintf(newpath, sizeof(newpath), "%s/.CoreShell/bin:%s", home, path);
            setenv("PATH", newpath, 1);
        }
    }

    /* ── Multicall dispatch ────────────────────────────────────────────── *
     * Mode 1: invoked via a symlink or hardlink named after a command,    *
     *   e.g. ./ls -la — argv[0] basename is a known command → dispatch    *
     *   directly, no REPL.                                                *
     * Mode 2: invoked as ./CoreShell <cmd> [args...]                      *
     *   argv[1] is a known command → shift argv and dispatch directly.    *
     * Mode 3: ./CoreShell (no args, or argv[0] basename is "CoreShell")   *
     *   → fall through to the interactive REPL (Read-Eval-Print-Loop).    */

    const char *self = argv0_basename(argv[0]);

    /* Mode 1: symlink/hardlink invocation — *self != CoreShell */
    if (strcmp(self, "CoreShell") != 0)
    {
        /* argv[0] is already the command name; pass full argc/argv */
        const cmd_spec_t *spec = find_command(self);
        if (!spec)
            return unknown_command(self);
        return dispatch_builtin(argc, argv); /* Run internal command via thread */
    }

    /* Mode 2: ./CoreShell <cmd> [args...] */
    if (argc > 1)
    {
        if (argv[1][0] == '@')
        {
            char *query = join_llm_query(argc, argv);
            if (query == NULL)
            {
                fprintf(stderr, "malloc: out of memory\n");
                return EXIT_FAILURE;
            }

            if (query[0] != '\0')
                handle_llm_line(query);
            else
                fprintf(stderr, "CoreShell: @ requires a query\n");

            free(query);
            return 0;
        }

        /* Shift: run_argv[0] = command name, run_argc excludes program name. */
        int run_argc = argc - 1;
        char **run_argv = &argv[1];
        return dispatch_command(run_argc, run_argv);
    }

    /* ── Mode 3: Interactive REPL ──────────────────────────────────────── */
    char  *input;
    char  *args[MAX_ARGS];

    printf("CoreShell v2.0 - Simple Linux Shell\n");
    printf("Type 'help' for available commands or 'exit' to quit.\n\n");

    for (;;)
    {
        g_sigint = 0;

        /* Print notifications for any background jobs that finished. */
        if (g_sigchld)
        {
            g_sigchld = 0;
            notify_done_jobs();
        }

        const char *user = getenv("USER");
        printf("%s@CoreShell> ", user ? user : "user");
        fflush(stdout);

        input = read_input();
        if (input == NULL)
        {
            /* Ctrl-C: re-prompt */
            continue;
        }

        /* Skip blank lines */
        if (input[0] == '\0')
        {
            free(input);
            continue;
        }

        /* Check for LLM @ prefix */
        if (input[0] == '@')
        {
            /* Extract the query (everything after '@') */
            const char *query = input + 1;
            /* Skip leading whitespace after @ */
            while (*query && (*query == ' ' || *query == '\t'))
                query++;

            if (*query != '\0')
            {
                handle_llm_line(query);
            }
            else
            {
                fprintf(stderr, "CoreShell: @ requires a query\n");
            }
            free(input);
            continue;
        }

        /* Expand environment variables before routing */
        char expanded[BUFFER_SIZE * 2];
        if (expand_variables_line(input, expanded, sizeof(expanded)) != 0)
        {
            free(input);
            continue;
        }
        free(input);

          /* BNFC grammar currently does not cover 2>&1 combinations or quoted
              command forms; route those lines directly to the legacy parser to
              avoid noisy syntax errors. */
        const char *bnfc_line = NULL;
          if (strstr(expanded, "2>&1") == NULL && strpbrk(expanded, "\"'") == NULL)
            bnfc_line = normalize_line_with_bnfc(expanded);

        char *line_to_run = bnfc_line != NULL ? (char *)bnfc_line : expanded;

        if (strpbrk(line_to_run, "|<>&") != NULL)
        {
            execute_pipeline(line_to_run);
            continue;
        }

        int nargs = parse_command(line_to_run, args);
        if (nargs > 0)
            dispatch_command(nargs, args);
    }

    return 0; /* unreachable */
}
