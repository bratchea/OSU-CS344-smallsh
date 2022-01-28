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

// struct to store parsed user input
struct userInput {
    char *command;
    char *userArgs[MAXARGS];
    char *inputFile;
    char *outputFile;
    bool inBackground;
    bool redirect;
};

void printUserInput(struct userInput ui) {
    printf("command: %s\n", ui.command);
    printf("args: %s\n", ui.userArgs[0]);
    printf("input file: %s\n", ui.inputFile);
    printf("output file: %s\n", ui.outputFile);
    printf("%d\n", ui.inBackground);
}

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

    return uInput;
}

/*
Parse user input into a struct userInput
command [arg1 arg2 ...] [< input_file] [> output_file] [&]
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

    parsedInput->command = calloc(strlen(token) + 1, sizeof(char));
    parsedInput->command = token;

    // parsedInput->inBackground = false;
    // parsedInput->inputFile = "inputfile";
    // parsedInput->outputFile = "outputfile";
    // parsedInput->userArgs[0] = calloc(5, sizeof(char));
    // strcpy(parsedInput->userArgs[0], "arg1");

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

int main(void) {
    struct userInput ui;

    char *uInput;
    int i = 0;

    // printUserInput(ui);

    uInput = getUserInput();
    ui = *parseUserInput(uInput);
    printf("\n%s\n", ui.command);
    printf("\n%s\n", ui.inputFile);
    printf("\n%s\n", ui.outputFile);
    printf("\n%d\n", ui.inBackground);
    printf("\n%d\n", ui.redirect);

    while (ui.userArgs[i] != NULL) {
        printf("%s ", ui.userArgs[i]);
        i++;
    }

    return EXIT_SUCCESS;
}
