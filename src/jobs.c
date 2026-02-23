/*
 * shelli - Educational Shell
 * jobs.c - Job table management, foreground/background control
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include "jobs.h"

#define MAX_JOBS 64

static Job job_table[MAX_JOBS];
static int next_job_id = 1;
static int last_job_id = 0;

void jobs_init(void) {
    memset(job_table, 0, sizeof(job_table));
    next_job_id = 1;
    last_job_id = 0;
}

int job_add(pid_t pgid, const char *command) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_table[i].id == 0) {
            job_table[i].id = next_job_id++;
            job_table[i].pgid = pgid;
            job_table[i].state = JOB_RUNNING;
            job_table[i].command = strdup(command ? command : "");
            job_table[i].exit_status = 0;
            last_job_id = job_table[i].id;
            return job_table[i].id;
        }
    }
    return -1;
}

void job_set_state(int job_id, JobState state, int exit_status) {
    Job *j = job_find(job_id);
    if (j) {
        j->state = state;
        j->exit_status = exit_status;
    }
}

Job *job_find(int job_id) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_table[i].id == job_id) return &job_table[i];
    }
    return NULL;
}

Job *job_find_by_pgid(pid_t pgid) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_table[i].id > 0 && job_table[i].pgid == pgid) return &job_table[i];
    }
    return NULL;
}

int job_most_recent(void) {
    return last_job_id;
}

void job_reap(void) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_table[i].id > 0 && job_table[i].state == JOB_DONE) {
            free(job_table[i].command);
            memset(&job_table[i], 0, sizeof(Job));
        }
    }
}

void jobs_print(void) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_table[i].id > 0) {
            const char *state_str;
            switch (job_table[i].state) {
            case JOB_RUNNING: state_str = "Running"; break;
            case JOB_STOPPED: state_str = "Stopped"; break;
            case JOB_DONE:    state_str = "Done";    break;
            default:          state_str = "Unknown"; break;
            }
            printf("[%d] %s\t%s\n", job_table[i].id, state_str, job_table[i].command);
        }
    }
}

void jobs_check(void) {
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        /* Find job by pgid */
        Job *j = job_find_by_pgid(pid);
        if (!j) continue;

        if (WIFEXITED(status)) {
            j->state = JOB_DONE;
            j->exit_status = WEXITSTATUS(status);
            fprintf(stderr, "[%d] Done\t%s\n", j->id, j->command);
        } else if (WIFSIGNALED(status)) {
            j->state = JOB_DONE;
            j->exit_status = 128 + WTERMSIG(status);
            fprintf(stderr, "[%d] Terminated\t%s\n", j->id, j->command);
        } else if (WIFSTOPPED(status)) {
            j->state = JOB_STOPPED;
            fprintf(stderr, "[%d] Stopped\t%s\n", j->id, j->command);
        }
    }

    job_reap();
}

int job_foreground(int job_id) {
    Job *j = job_find(job_id);
    if (!j) {
        fprintf(stderr, "fg: %%%d: no such job\n", job_id);
        return 1;
    }

    fprintf(stderr, "%s\n", j->command);

    /* Give terminal to the job's process group */
    if (j->state == JOB_STOPPED) {
        kill(-j->pgid, SIGCONT);
    }
    j->state = JOB_RUNNING;

    /* Wait for the job */
    int status;
    waitpid(-j->pgid, &status, WUNTRACED);

    if (WIFSTOPPED(status)) {
        j->state = JOB_STOPPED;
        fprintf(stderr, "\n[%d] Stopped\t%s\n", j->id, j->command);
        return 128 + WSTOPSIG(status);
    }

    int exit_code = 0;
    if (WIFEXITED(status)) exit_code = WEXITSTATUS(status);
    else if (WIFSIGNALED(status)) exit_code = 128 + WTERMSIG(status);

    j->state = JOB_DONE;
    j->exit_status = exit_code;
    job_reap();

    return exit_code;
}

int job_background(int job_id) {
    Job *j = job_find(job_id);
    if (!j) {
        fprintf(stderr, "bg: %%%d: no such job\n", job_id);
        return 1;
    }

    if (j->state == JOB_STOPPED) {
        kill(-j->pgid, SIGCONT);
        j->state = JOB_RUNNING;
        fprintf(stderr, "[%d] %s &\n", j->id, j->command);
    }

    return 0;
}

void jobs_cleanup(void) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_table[i].id > 0) {
            free(job_table[i].command);
        }
    }
    memset(job_table, 0, sizeof(job_table));
}

int jobs_count(void) {
    int count = 0;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_table[i].id > 0) count++;
    }
    return count;
}
