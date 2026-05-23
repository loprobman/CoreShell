#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <signal.h>
#include <ctype.h>
#include "cmd_kill.h"
#include "cmd_jobs.h"
#include "cmd_registry.h"
#include "argtable3.h"

#define KILL_MAX_TARGETS 256

/* Map a signal name (e.g. "TERM", "KILL", "HUP") to signal number.
   Returns -1 if unknown. */
static int signal_name_to_num(const char *name)
{
    static const struct { const char *name; int num; } table[] = {
        { "HUP",  SIGHUP  }, { "INT",  SIGINT  }, { "QUIT", SIGQUIT },
        { "KILL", SIGKILL }, { "TERM", SIGTERM }, { "STOP", SIGSTOP },
        { "CONT", SIGCONT }, { "USR1", SIGUSR1 }, { "USR2", SIGUSR2 },
        { "PIPE", SIGPIPE }, { "ALRM", SIGALRM }, { "CHLD", SIGCHLD },
        { NULL, 0 }
    };
    for (int i = 0; table[i].name; i++)
        if (strcasecmp(name, table[i].name) == 0)
            return table[i].num;
    return -1;
}

/* ── run ───────────────────────────────────────────────────────────────── */

static int kill_run(int argc, char **argv)
{
    struct arg_lit *help      = arg_lit0("h", "help",      "show this help and exit");
    struct arg_lit *help_json = arg_lit0(NULL, "help-json", "print argument schema as JSON");
    struct arg_str *sig_opt   = arg_str0("s", "signal",    "SIGNAL", "signal name (e.g. TERM, KILL)");
      struct arg_rex *targets   = arg_rexn(NULL, NULL, "(%[0-9]+|-?[0-9]+)", "<pid|%job>", 1, KILL_MAX_TARGETS, 0,
                                         "PID or %%jobid to signal");
    struct arg_end *end       = arg_end(10);
    void *argtable[] = { help, help_json, sig_opt, targets, end, NULL };

    int nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0)
    {
        printf("Usage: kill [-s SIGNAL] <pid|%%jobid> ...\n");
        printf("Send a signal to a process or background job.\n");
        printf("Default signal: SIGTERM\n");
        printf("Examples:\n");
        printf("  kill 1234          # send SIGTERM to pid 1234\n");
        printf("  kill %%1            # send SIGTERM to job 1\n");
        printf("  kill -s KILL %%2   # send SIGKILL to job 2\n");
        arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]) - 1);
        return 0;
    }

    if (help_json->count > 0)
    {
        cmd_print_help_json(stdout, "kill", "send signal to process", NULL, argtable);
        arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]) - 1);
        return 0;
    }

    if (nerrors > 0)
    {
        arg_print_errors(stderr, end, "kill");
        fprintf(stderr, "Usage: kill [-s SIGNAL] <pid|%%jobid> ...\n");
        arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]) - 1);
        return 1;
    }

    /* Determine signal number */
    int signum = SIGTERM;
    if (sig_opt->count > 0)
    {
        const char *sname = sig_opt->sval[0];
        /* Allow numeric signal */
        if (isdigit((unsigned char)*sname))
        {
            signum = atoi(sname);
            if (signum <= 0 || signum >= 64)
            {
                fprintf(stderr, "kill: invalid signal number: %s\n", sname);
                arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]) - 1);
                return 1;
            }
        }
        else
        {
            signum = signal_name_to_num(sname);
            if (signum < 0)
            {
                fprintf(stderr, "kill: unknown signal: %s\n", sname);
                arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]) - 1);
                return 1;
            }
        }
    }

    /* Also handle -SIGNAME / -N passed as bare argument (e.g. kill -9 1234).
       argtable captures these in targets as negative numbers like "-9". */

    int ret = 0;
    for (int i = 0; i < targets->count; i++)
    {
        const char *t = targets->sval[i];

        /* -N short form: bare negative integer as signal */
        if (t[0] == '-' && isdigit((unsigned char)t[1]))
        {
            signum = atoi(t + 1);
            if (signum <= 0 || signum >= 64)
            {
                fprintf(stderr, "kill: invalid signal: %s\n", t);
                ret = 1;
            }
            continue;
        }

        pid_t target_pid = 0;

        if (t[0] == '%')
        {
            /* Job id */
            int job_id = atoi(t + 1);
            bg_job_t *j = job_by_id(job_id);
            if (j == NULL)
            {
                fprintf(stderr, "kill: %%%d: no such job\n", job_id);
                ret = 1;
                continue;
            }
            target_pid = j->pid;
        }
        else
        {
            target_pid = (pid_t)atoi(t);
            if (target_pid <= 0)
            {
                fprintf(stderr, "kill: invalid pid: %s\n", t);
                ret = 1;
                continue;
            }
        }

        if (kill(target_pid, signum) < 0)
        {
            perror("kill");
            ret = 1;
        }
    }

    arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]) - 1);
    return ret;
}

static void kill_print_usage(FILE *out)
{
    fprintf(out, "Usage: kill [-s SIGNAL] <pid|%%jobid> ...\n");
    fprintf(out, "  Send a signal to a process or background job.\n");
}

/* ── registration ──────────────────────────────────────────────────────── */

static const cmd_spec_t s_kill_spec = {
    .name        = "kill",
    .summary     = "send signal to a process or background job",
    .long_help   = "Send a signal to a process by PID or by job number (%%N). "
                   "Default signal is SIGTERM. Use -s KILL to force-kill.",
    .run         = kill_run,
    .print_usage = kill_print_usage,
};

void register_kill_command(void)
{
    register_command(&s_kill_spec);
}
