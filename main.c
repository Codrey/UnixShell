#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

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

// for external binary using fork and execvp execution
int launch_process(char **args) {
    pid_t pid;
    int status;

    pid = fork();
    if (pid == 0) {
        // Child process
        if (execvp(args[0], args) == -1) {
            perror("myshell");
        }
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        // Error forking
        perror("myshell");
    } else {
        // Parent process waits for child to finish
        do {
            waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    return 1;
}

// main 
int main(void) {
    char *line = NULL;
    size_t bufsize = 0;
    char *args[MAX_ARGS];
    int status = 1;

    // Main REPL Loop
    do {
        // Print prompt
        printf("myshell> ");
        fflush(stdout);

        // Read line input
        if (getline(&line, &bufsize, stdin) == -1) {
            if (feof(stdin)) {
                // End of File (Ctrl+D)
                printf("\n");
                break;
            } else {
                perror("myshell: getline");
                break;
            }
        }

        // Parse arguments
        parse_line(line, args);

        // Execute if not a built-in
        if (!handle_builtin(args)) {
            launch_process(args);
        }

    } while (status);

    free(line);
    return 0;
}
