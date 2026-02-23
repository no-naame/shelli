/*
 * shelli - Educational Shell
 * jobs.h - Job control interface
 */

#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>

typedef enum {
    JOB_RUNNING,
    JOB_STOPPED,
    JOB_DONE
} JobState;

typedef struct {
    int id;             /* Job number (1-based) */
    pid_t pgid;         /* Process group ID */
    JobState state;
    char *command;       /* Command string for display */
    int exit_status;
} Job;

/* Initialize job table */
void jobs_init(void);

/* Add a background job. Returns job ID. */
int job_add(pid_t pgid, const char *command);

/* Update job state after waitpid */
void job_set_state(int job_id, JobState state, int exit_status);

/* Find job by ID (returns NULL if not found) */
Job *job_find(int job_id);

/* Find job by PGID */
Job *job_find_by_pgid(pid_t pgid);

/* Get most recent job ID */
int job_most_recent(void);

/* Remove completed jobs from table */
void job_reap(void);

/* Print job table */
void jobs_print(void);

/* Check for completed/stopped background jobs (non-blocking) */
void jobs_check(void);

/* Bring job to foreground */
int job_foreground(int job_id);

/* Continue stopped job in background */
int job_background(int job_id);

/* Clean up job table */
void jobs_cleanup(void);

/* Get job count */
int jobs_count(void);

#endif /* JOBS_H */
