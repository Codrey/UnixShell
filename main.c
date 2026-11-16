#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_LINE 1024
#define MAX_ARGS 64

// Handles built in shell commands to modify the shell state
int handle_builtin(char **args) {
    if (args[0] == NULL) {
        return 1; // Empty command
    }
    
    if (strcmp(args[0], "exit") == 0) {
        exit(0);
    }
    
    if (strcmp(args[0], "cd") == 0) {
        if (args[1] == NULL) {
            fprintf(stderr, "myshell: expected argument to \"cd\"\n");
        } else {
            if (chdir(args[1]) != 0) {
                perror("myshell");
            }
        }
        return 1;
    }
    
    return 0; // Not a built-in
}

// Parse the line into tokens (args array)
void parse_line(char *line, char **args) {
    int i = 0;
    char *token = strtok(line, " \t\r\n\a");
    while (token != NULL && i < MAX_ARGS - 1) {
        args[i++] = token;
        token = strtok(NULL, " \t\r\n\a");
    }
    args[i] = NULL;
}
