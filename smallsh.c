#include <dirent.h>
#include <fcntl.h>
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

/*
Globals
*/
bool foregroundOnly = false;

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
    parsedInput->command = token;

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

void runProcess(struct userInput *ui) {
    pid_t spawnid = -5;
    int childStatus;
    int childPid;

    // If fork is successful, the value of spawnpid will be 0 in the child, the child's pid in the parent
    spawnid = fork();
    switch (spawnid) {
        case -1:
            printf("Oops! Something went wrong during fork!\n");
            exit(1);
            break;

        case 0:
            execvp(ui->command, ui->builtArgs);
            printf("Oops! Error while running shell command!\n");
            exit(1);

        default:
            if (ui->inBackground == true && foregroundOnly == false) {
                printf("Backgroud pid is %d\n", spawnid);
            } else {
                // waits for the child process to execute
                waitpid(spawnid, &childStatus, 0);
            }
    }
}

int main(void) {
    struct userInput *ui;

    char *uInput;
    int i = 0;

    while (true) {
        uInput = getUserInput();
        ui = parseUserInput(uInput);
        buildArgs(ui);

        // check if user entered a blank line or comment
        if (ui->command == NULL || strncmp(ui->command, "#", 1) == 0) {
            continue;
        }
        // check for exit command
        else if (strcmp(ui->command, "exit") == 0) {
            break;
        } else if (strcmp(ui->command, "status") == 0) {
            printf("status not implemented");
        } else if (strcmp(ui->command, "cd") == 0) {
            printf("cd command not implemented");
        } else {
            runProcess(ui);
        }

        // printf("command: %s\ninput file: %s\noutputfile: %s\nbackground: %d\nredirect: %d\n", ui->command, ui->inputFile, ui->outputFile, ui->inBackground, ui->redirect);

        // while (ui->builtArgs[i] != NULL) {
        //     printf("%s ", ui->builtArgs[i]);
        //     i++;
        // }
        // i = 0;

        fflush(stdin);
        fflush(stdout);

        // free memory from userInput struct
        free(ui);
    }

    return EXIT_SUCCESS;
}
