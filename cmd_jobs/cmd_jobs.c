#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cmd_jobs.h"
#include "cmd_registry.h"
#include "argtable3.h"

/* ── run ───────────────────────────────────────────────────────────────── */

static int jobs_run(int argc, char **argv)
{
    struct arg_lit *help     = arg_lit0("h", "help", "show this help and exit");
    struct arg_lit *help_json= arg_lit0(NULL, "help-json", "print argument schema as JSON");
    struct arg_end *end      = arg_end(10);
    void *argtable[] = { help, help_json, end, NULL };

    int nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0)
    {
        printf("Usage: jobs\n");
        printf("List all active background jobs.\n");
        arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]) - 1);
        return 0;
    }

    if (help_json->count > 0)
    {
        cmd_print_help_json(stdout, "jobs", "list background jobs", NULL, argtable);
        arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]) - 1);
        return 0;
    }

    if (nerrors > 0)
    {
        arg_print_errors(stderr, end, "jobs");
        arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]) - 1);
        return 1;
    }

    int count = 0;
    bg_job_t *jt = job_table(&count);
    int printed = 0;

    for (int i = 0; i < count; i++)
    {
        if (jt[i].pid == 0) continue; /* empty slot */

        const char *state_str;
        switch (jt[i].state)
        {
            case JOB_RUNNING:  state_str = "Running";  break;
            case JOB_DONE:     state_str = "Done";     break;
            case JOB_SIGNALED: state_str = "Killed";   break;
            default:           state_str = "Unknown";  break;
        }
        printf("[%d] %-10s %d\t%s\n",
               jt[i].job_id, state_str, (int)jt[i].pid, jt[i].cmd);
        /* Consume done/killed entries so notify_done_jobs won't repeat them. */
        if (jt[i].state != JOB_RUNNING)
            jt[i].pid = 0;
        printed++;
    }

    if (printed == 0)
        printf("No background jobs.\n");

    arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]) - 1);
    return 0;
}

static void jobs_print_usage(FILE *out)
{
    fprintf(out, "Usage: jobs\n");
    fprintf(out, "  List all active background jobs.\n");
}

/* ── registration ──────────────────────────────────────────────────────── */

static const cmd_spec_t s_jobs_spec = {
    .name        = "jobs",
    .summary     = "list background jobs",
    .long_help   = "Display the status of all background jobs started in this shell session.",
    .run         = jobs_run,
    .print_usage = jobs_print_usage,
};

void register_jobs_command(void)
{
    register_command(&s_jobs_spec);
}
