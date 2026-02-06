#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <limits.h>

#define MAX_INPUT 1024
#define MAX_ARGS 64

int main() {
    char input[MAX_INPUT];
    char* args[MAX_ARGS];

    while (1) {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            char *dir = strrchr(cwd, '/');
            if (dir && *(dir + 1) != '\0')
                dir++;
            else
                dir = cwd;

            printf("sish:%s> ", dir);
        } else {
            perror("getcwd failed");
            printf("sish> ");
        }
        fflush(stdout);

        if (fgets(input, MAX_INPUT, stdin) == NULL) {
            perror("fgets failed");
            continue;
        }

        input[strcspn(input, "\n")] = '\0';

        if (strcmp(input, "exit") == 0) {
            printf("See you soon!!\n");
            break;
        }

        char* token = strtok(input, " ");
        int i = 0;

        while (token != NULL && i < MAX_ARGS - 1) {
            args[i++] = token;
            token = strtok(NULL, " ");
        }
        args[i] = NULL;

        if (args[0] == NULL)
            continue;

        if (strcmp(args[0], "cd") == 0) {
            if (args[1] == NULL) {
                fprintf(stderr, "cd: missing argument\n");
            } else if (chdir(args[1]) != 0) {
                perror("cd failed");
            }
            continue;
        }

        pid_t pid = fork();

        if (pid == 0) {
            execvp(args[0], args);
            perror("execvp failed");
            exit(EXIT_FAILURE);
        } 
        else if (pid > 0) {
            int status;
            waitpid(pid, &status, 0);
        } 
        else {
            perror("fork failed");
        }
    }

    return 0;
}
