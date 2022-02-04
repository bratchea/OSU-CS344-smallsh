#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAXLENGTH 2048
#define MAXARGS 512
// max processes to be run in background
#define MAXPROCS 200

// Globals
bool foregroundOnly = false;
int foregroundStatus;
// track pids of background processes
pid_t backgroundProcs[MAXPROCS];
int procNum = 0;  // track number of background processes

// struct to store parsed user input
struct userInput {
    char *command;
    char *userArgs[MAXARGS];
    char *builtArgs[MAXARGS];
    char *inputFile;
    char *outputFile;
    bool inBackground;
    bool redirect;
};

/*
Gets user input and return char pointer
*/
char *getUserInput(void) {
    char *uInput = NULL;
    size_t len = 0;
    printf(":");
    getline(&uInput, &len, stdin);

    // replace newline from end of user input with null terminator
    uInput[strlen(uInput) - 1] = '\0';
    fflush(stdin);
    fflush(stdout);

    return uInput;
}

/*
Clear user input from userInput struct and free memory
that has been allocated
*/
void clearUserInput(struct userInput *ui) {
    // reset boolean values to false
    ui->inBackground = false;
    ui->redirect = false;

    // reset fields that have memory dynamically allocated
    free(ui->command);
    free(ui->inputFile);
    free(ui->outputFile);

    // loop through userArgs to free allocated memory
    for (int i = 0; i < MAXARGS; i++) {
        if (ui->userArgs[i] != NULL) {
            free(ui->userArgs[i]);
        } else {
            break;
        }
    }
    // reset values stored that have set lengths
    memset(ui->userArgs, '\0', sizeof(ui->userArgs));
    memset(ui->builtArgs, '\0', sizeof(ui->builtArgs));
}

/*
Parses user input into a struct userInput
user input example: command [arg1 arg2 ...] [< input_file] [> output_file] [&]
Returns *struct userInput
*/
struct userInput *parseUserInput(char *ui) {
    struct userInput *parsedInput = malloc(sizeof(struct userInput));
    char *token;
    char *posptr;
    char *temp;
    int nArgs = 0;

    // initially set inBackground and redirect to false
    parsedInput->inBackground = false;
    parsedInput->redirect = false;

    // first part of user input should command
    token = strtok_r(ui, " ", &posptr);
    if (token == NULL) {
        parsedInput->command = NULL;
        return parsedInput;
    }

    parsedInput->command = calloc(strlen(token) + 1, sizeof(char));
    strcpy(parsedInput->command, token);

    token = strtok_r(NULL, " ", &posptr);
    while (token != NULL) {
        // check for input redirection
        if (strcmp(token, "<") == 0) {
            token = strtok_r(NULL, " ", &posptr);
            parsedInput->inputFile = calloc(strlen(token) + 1, sizeof(char));
            strcpy(parsedInput->inputFile, token);
            parsedInput->redirect = true;
        }

        else if (strcmp(token, ">") == 0) {
            token = strtok_r(NULL, " ", &posptr);
            parsedInput->outputFile = calloc(strlen(token) + 1, sizeof(char));
            strcpy(parsedInput->outputFile, token);
        }

        else if (strcmp(token, "&") == 0) {
            token = strtok_r(NULL, " ", &posptr);
            if (token == NULL) {
                parsedInput->inBackground = true;
            } else {
                // Add '&' to arguments
                parsedInput->userArgs[nArgs] = calloc(2, sizeof(char));
                strcpy(parsedInput->userArgs[nArgs], "&");
                nArgs++;

                // add token to args
                parsedInput->userArgs[nArgs] = calloc(strlen(token) + 1, sizeof(char));
                strcpy(parsedInput->userArgs[nArgs], token);
                nArgs++;
            }
        }

        else {
            parsedInput->userArgs[nArgs] = calloc(strlen(token) + 1, sizeof(char));
            strcpy(parsedInput->userArgs[nArgs], token);
            nArgs++;
        }

        token = strtok_r(NULL, " ", &posptr);
    }

    return parsedInput;
}

/*
Redirects I/O if input and/or output is passed to commands
*/
void redirect(struct userInput *ui) {
    int inputfd = STDIN_FILENO;
    int outputfd = STDOUT_FILENO;

    if (ui->inputFile != NULL) {
        inputfd = open(ui->inputFile, O_RDONLY);
        // check if valid file name and display error if not
        if (inputfd < 0) {
            printf("Input file does not exist\n");
            fflush(stdout);
            exit(1);
        }
        // redirect input
        dup2(inputfd, 0);
        close(inputfd);
    }

    if (ui->outputFile != NULL) {
        outputfd = open(ui->outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (outputfd < 0) {
            printf("Error opening and/or creating output file\n");
            fflush(stdout);
            exit(1);
        }
        // redirect output
        dup2(outputfd, 1);
        close(outputfd);
    }
}

/*
Creates an array of arguments to be passed to execvp
using the arguments stored in struct userInput.userArgs
*/
void buildArgs(struct userInput *ui) {
    int argCount = 0;

    // add command as first element of built args
    ui->builtArgs[argCount] = ui->command;
    argCount++;

    while (ui->userArgs[argCount - 1] != NULL) {
        ui->builtArgs[argCount] = ui->userArgs[argCount - 1];
        argCount++;
    }
}

/*
Catches SIGSTP signal. The shell then enters a state where subsequent
commands can no longer be run in the background.
*/
void catchSigstp(int signo) {
    if (foregroundOnly) {
        char *message = "Exiting foreground only mode (& is no longer ignored)\n";
        write(STDOUT_FILENO, message, 56);
        foregroundOnly = false;
    } else {
        char *message = "Entering foreground-only mode (& is now ignored)\n";
        write(STDOUT_FILENO, message, 51);
        foregroundOnly = true;
    }
}

/*
Checks for completed background processes and prints pid and exit if completed
*/
void checkBackground(void) {
    int childStatus;
    pid_t pid;

    for (int i = 0; i < procNum; i++) {
        pid = waitpid(backgroundProcs[i], &childStatus, WNOHANG);
        if (pid != 0 && WIFEXITED(childStatus) != 0) {
            printf("\nbackground pid %d is done: exit value %d\n", backgroundProcs[i], WEXITSTATUS(childStatus));
            fflush(stdout);

            // remove pid from background procs and shift left
            for (int j = i; j < procNum - 1; j++) {
                backgroundProcs[j] = backgroundProcs[j + 1];
            }
            // decrement total number of background processes
            procNum--;
        } else if (pid != 0) {
            printf("\nbackground pid %d is done: terminated by signal %d\n", backgroundProcs[i], WTERMSIG(childStatus));
            fflush(stdout);

            // remove pid from background procs and shift left
            for (int j = i; j < procNum - 1; j++) {
                backgroundProcs[j] = backgroundProcs[j + 1];
            }
            // decrement total number of background processes
            procNum--;
        }
    }
}

void runProcess(struct userInput *ui) {
    pid_t spawnid = -5;
    int childStatus;
    int childPid;
    int termsig;
    struct sigaction sigintAction;

    // If fork is successful, the value of spawnpid will be 0 in the child, the child's pid in the parent
    spawnid = fork();
    switch (spawnid) {
        case -1:
            printf("Oops! Something went wrong during fork!\n");
            fflush(stdout);
            exit(1);
            break;

        case 0:
            // set sa_handler back to default behaviour for child process if running in foreground
            if (ui->inBackground == false) {
                sigintAction.sa_handler = SIG_DFL;
                sigintAction.sa_flags = 0;
                sigaction(SIGINT, &sigintAction, NULL);
            }

            // handles redirection of I/O
            redirect(ui);
            // executes command
            execvp(ui->command, ui->builtArgs);
            printf("Oops! Error while running shell command!\n");
            fflush(stdout);
            exit(1);

        default:
            if (ui->inBackground && !foregroundOnly) {
                // add to array to track
                backgroundProcs[procNum] = spawnid;
                // increment count of background processes
                procNum++;

                printf("Background pid is %d\n", spawnid);
                fflush(stdout);
            } else {
                // waits for the child process to execute
                waitpid(spawnid, &childStatus, 0);
                foregroundStatus = childStatus;

                // prints signal that terminated child process
                if (WIFSIGNALED(foregroundStatus)) {
                    termsig = WTERMSIG(foregroundStatus);
                    printf("\nterminated by signal: %d\n", termsig);
                }
            }
    }
}

/*
handles builtin command cd. If no arguments passed then
sets cwd to HOME env variable.
*/
void changeDirectory(struct userInput *ui) {
    char *home = getenv("HOME");

    // navigate to home if no path specified
    if (ui->userArgs[0] == NULL) {
        if (chdir(home) != 0) {
            printf("Directory not found\n");
            fflush(stdout);
        }
    } else {
        if (chdir(ui->userArgs[0]) != 0) {
            printf("Directory: %s not found\n", ui->userArgs[0]);
            fflush(stdout);
        }
    }
}

/*
handles builtin command exit. Kills all processes running in
background
*/
void exitProcess(void) {
    for (int i = procNum; i > -1; i--) {
        kill(backgroundProcs[i], SIGINT);
    }
}

int main(void) {
    struct userInput *ui;
    int fstat;
    char *uInput;
    int i = 0;

    // initialize and fill sigaction struct for SIGINT
    struct sigaction ignoreAction;
    ignoreAction.sa_handler = SIG_IGN;
    ignoreAction.sa_flags = 0;

    //initialize and fill sigaction strcut for SIGTSTP
    struct sigaction sigtstpAction;
    sigtstpAction.sa_handler = catchSigstp;
    sigtstpAction.sa_flags = SA_RESTART;

    while (true) {
        // ignore SIGINT for parent process
        sigaction(SIGINT, &ignoreAction, NULL);
        // handle SIGTSTP
        sigaction(SIGTSTP, &sigtstpAction, NULL);

        uInput = getUserInput();
        ui = parseUserInput(uInput);
        buildArgs(ui);

        // check if user entered a blank line or comment
        if (ui->command == NULL || strncmp(ui->command, "#", 1) == 0) {
            fflush(stdin);
            fflush(stdout);
            clearUserInput(ui);
            continue;
        }
        // check for exit command
        else if (strcmp(ui->command, "exit") == 0) {
            exitProcess();
            break;
        } else if (strcmp(ui->command, "status") == 0) {
            // Get exit status of last foreground process run in shell
            if (WIFEXITED(foregroundStatus)) {
                fstat = WEXITSTATUS(foregroundStatus);
            } else {
                fstat = WTERMSIG(foregroundStatus);
            }
            printf("exit status %d\n", fstat);
            fflush(stdout);
        } else if (strcmp(ui->command, "cd") == 0) {
            changeDirectory(ui);
        } else {
            runProcess(ui);
        }

        // check for completed background processes
        checkBackground();

        fflush(stdin);
        fflush(stdout);

        // clear input from struct and free memory
        clearUserInput(ui);
    }

    return EXIT_SUCCESS;
}
