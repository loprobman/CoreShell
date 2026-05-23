#ifndef CMD_JOBS_H
#define CMD_JOBS_H

#include <sys/types.h>

typedef enum { JOB_RUNNING = 0, JOB_DONE, JOB_SIGNALED } job_state_t;

typedef struct
{
    pid_t       pid;
    int         job_id;
    job_state_t state;
    int         exit_status;
    char        cmd[256];
} bg_job_t;

/* Provided by main.c */
bg_job_t       *job_table(int *count_out);
int             job_add(pid_t pid, const char *cmd);
bg_job_t       *job_by_pid(pid_t pid);
bg_job_t       *job_by_id(int job_id);

void register_jobs_command(void);

#endif /* CMD_JOBS_H */
