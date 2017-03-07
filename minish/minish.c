#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

#define JOB_NUMBER_LIMIT    10
#define PWD_BUFFER_SIZE     1000
#define MAX_ARGS            20

static void sigHandler(int);
void execCmd(char **);
int getcmd(char *, char **, int *); 
int checkBuiltInCommands(char **);
int checkRedir(char **, int);
int checkPipe(char **, int);
void cd(char *);
void pwd();
void fg(char *);
void jobs();

int bgPid[JOB_NUMBER_LIMIT];
char *bgLabel[JOB_NUMBER_LIMIT];
int bgNextFreeJob = 0;

main() {
    char *args[20];
    int bg;

    // handle Ctrl-C, and ignore Ctrl-Z
    if (signal(SIGINT, sigHandler) == SIG_ERR ||
            signal(SIGTSTP, SIG_IGN) == SIG_ERR) {
        printf("Error: Could not handle signals\n");
        exit(1);
    }
        
    while (1) {
        bg = 0;
        int pid, count = getcmd("\n> ", args, &bg);

        // check whether backgrounded processes are done
        int result;
        result = waitpid(-1, NULL, WNOHANG); // returns instantly if no child has exited

        if (result > 0) {
            int i;
            for (i = 0; i < JOB_NUMBER_LIMIT; i++)
                if (bgPid[i] == result) {
                    printf("Job [%d] %d : %s is finished\n", 
                                i, bgPid[i], bgLabel[i]);
                    bgPid[i] = 0;
                    bgLabel[i] = NULL;
                    bgNextFreeJob = i;
                }
        }

        // command was entered
        if (count > 0 && checkBuiltInCommands(args) == 1) {
            if ((pid = fork()) == 0) {   // child process
                // checkRedir needs higher priority than checkPipe
                if (checkRedir(args, count) != 1 && checkPipe(args, count) != 1)
                    execCmd(args);
            } else {                     // parent process
                if (bg == 0)
                    waitpid(pid, NULL, 0);
                else if (bgNextFreeJob < JOB_NUMBER_LIMIT) {
                    bgPid[bgNextFreeJob] = pid;
                    bgLabel[bgNextFreeJob] = args[0];
                    printf("Job [%d] %d : %s sent to background\n", bgNextFreeJob, pid, args[0]);
                    
                    // find next free job. if there are none, bgNextFreeJob
                    // will be over JOB_NUMBER_LIMIT; there are too many jobs
                    bgNextFreeJob = -1;
                    while (bgNextFreeJob < JOB_NUMBER_LIMIT 
                            && bgPid[++bgNextFreeJob] != 0);
                } else
                    printf("Error: Cannot run more than %d jobs a once\n", JOB_NUMBER_LIMIT);
            }
        }
    }
}

/* sigHandler:  catch Ctrl-C, kill the currently running process */
static void sigHandler(int sig) {
    signal(sig, SIG_IGN);
}

/* execCmd: run a command using the execvp system call, which looks for
 *          binaries in $PATH */
void execCmd(char *args[]) {
    int err;
    if ((err = execvp(args[0], args)) < 0)
        printf("%s: Could not execute command\n", args[0]);
}

/* checkBuiltInCommands:    built in commands will be run by the shell
 *                          before forking processes into parent and child */
int checkBuiltInCommands(char *args[]) {
    if ((strcmp(args[0], "cd"))             == 0) {
        cd(args[1]);
        return 0;
    } else if ((strcmp(args[0], "pwd"))     == 0) {
        pwd();
        return 0;
    } else if ((strcmp(args[0], "jobs"))    == 0) {
        jobs();
        return 0;
    } else if ((strcmp(args[0], "fg"))      == 0) {
        fg(args[1]);
        return 0;
    } else if ((strcmp(args[0], "exit"))    == 0) {
        exit(0);
    } else
        return 1;
}

/* checkRedir:  check for an > argument. if found, redirect the output
 *              to the file specified after the >. any arguments after that
 *              are ignored. return 1 if a redirection was found */
int checkRedir(char *args[], int numberOfArgs) {
    int i;
    for (i = 0; i < numberOfArgs; i++) {
        if (strcmp(args[i], ">") == 0) {
            FILE *fp; 
            if ((fp = fopen(args[i + 1], "a+")) == NULL)
                    printf("Error: Could not open file\n");
            dup2(fileno(fp), fileno(stdout));

            if (fclose(fp) == EOF)
                printf("Error: Could not close file\n");

            // new array for use by execvp
            int j;
            char *newArgs[i + 1];
            for (j = 0; j < i; j++)
                newArgs[j] = args[j];
            newArgs[j] = NULL; // execvp requires NULL last arg
            
            if (checkPipe(newArgs, j) != 1)
                execCmd(newArgs);

            return 1;
        }
    }
    return 0;
}

/* checkPipe:   check for pipes from left to right. called recursively to allow
 *              for multiple pipes and a final file redirection. return 1 if
 *              a pipe is found */
int checkPipe(char *args[], int numberOfArgs) {
    int i;
    for (i = 0; i < numberOfArgs; i++) {
        if (strcmp(args[i], "|") == 0) {
            int fd[2], pid;
            pipe(fd);

            if ((pid = fork()) == 0) {
                dup2(fd[0], fileno(stdin));
                close(fd[1]); // child doesn't write to stdout

                // new array for use by execvp
                int j, pos = 0;
                char *newArgs[numberOfArgs - i + 1];
                for (j = i + 1; j < numberOfArgs; j++)
                    newArgs[pos++] = args[j];
                newArgs[pos] = NULL; // execvp requires NULL last arg
            
                // recursively check for pipes
                if (checkPipe(newArgs, pos) != 1)
                    execCmd(newArgs);
            } else {
                dup2(fd[1], fileno(stdout));
                close(fd[0]); // parent doesn't read from stdin

                // new array for use by execvp
                int j;
                char *newArgs[i + 1];
                for (j = 0; j < i; j++)
                    newArgs[j] = args[j];
                newArgs[j] = NULL; // execvp requires NULL last arg
            
                // since we check for pipes from left to right, the parent
                // pipes have been parsed already.
                execCmd(newArgs);
            }

            return 1;
        }
    }
    return 0;
}

/* cd:  builtin command. changes directory to target using the chdir
 *      system call. */
void cd(char *path) {
    int err;
    if (path == NULL)
        err = chdir(getenv("HOME"));
    else
        err = chdir(path);
    
    if (err < 0)
        printf("cd: Could not change directory to %s\n", path);
    else
        printf("cd: Changed directory to %s\n", path);
}

/* pwd: builtin command. prints current working directory to stdout */
void pwd() {
    char *buffer[PWD_BUFFER_SIZE];
    printf("Current working directory: %s\n", getcwd(*buffer, PWD_BUFFER_SIZE));
}

/* jobs:    builtin command. prints current jobs to stdout. jobs are added and
 *          removed in main */
void jobs() {
    int i = 0;
    for (i = 0; i < JOB_NUMBER_LIMIT; i++)
        if (bgPid[i] > 0)
            printf("[%d] %d : %s\n", i, bgPid[i], bgLabel[i]);
}

/* fg:  builtin command. brings the specified job to the foreground, or bring
 *      the first one on the list if none is specified. */
void fg(char *index) {
    int final;
    if (index == NULL) {
        int i;
        for (i = 0; i < JOB_NUMBER_LIMIT; i++) 
            if (bgPid[i] > 0) 
                final = i;
    } else 
        final = atoi(index);

    if (0 <= final && final < JOB_NUMBER_LIMIT && bgPid[final] != 0) {
        printf("Job [%d] %d : %s brought to foreground\n", 
                final, bgPid[final], bgLabel[final]);
        waitpid(bgPid[final], NULL, 0);
    } else
        printf("Job does not exist\n");
}

/* getcmd:  print prompt and parse stdin input into tokens for use by 
 *          other commands. */
int getcmd(char *prompt, char *args[], int *background) {
    int length, i = 0;
    char *token, *loc;
    char *line = NULL;
    size_t linecap = 0;

    printf("%s", prompt);
    length = getline(&line, &linecap, stdin);

    if (length <= 0)
        exit(-1);

    // check bg status
    if ((loc = index(line, '&')) != NULL) {
        *background = 1;
        *loc = ' ';
    } else
        *background = 0;

    while ((token = strsep(&line, " \t\n")) != NULL) {
        int j;
        for (j = 0; j < strlen(token); j++)
            if (token[j] <= 32)
                token[j] = '\0';
        if (strlen(token) > 0)
                args[i++] = token;
    }

    args[i] = NULL; // execvp needs last arg to be NULL

    return i;
}
