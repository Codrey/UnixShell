#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
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

// External binary execution using fork, execvp, handle I/O redirection
int launch_process(char **args) {
    pid_t pid;
    int status;

    pid = fork();
    if (pid == 0) {
        // Child process: Scan for I/O redirection operators
        for (int i = 0; args[i] != NULL; i++) {
            if (strcmp(args[i], ">") == 0) {
                // Output Redirection
                if (args[i + 1] == NULL) {
                    fprintf(stderr, "myshell: syntax error near unexpected token `newline'\n");
                    exit(EXIT_FAILURE);
                }
                int fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) {
                    perror("myshell: open");
                    exit(EXIT_FAILURE);
                }
                if (dup2(fd, STDOUT_FILENO) < 0) {
                    perror("myshell: dup2");
                    exit(EXIT_FAILURE);
                }
                close(fd);
                args[i] = NULL; // Cut off args so execvp ignores '>' and filename
                break;
            } else if (strcmp(args[i], "<") == 0) {
                // Input Redirection
                if (args[i + 1] == NULL) {
                    fprintf(stderr, "myshell: syntax error near unexpected token `newline'\n");
                    exit(EXIT_FAILURE);
                }
                int fd = open(args[i + 1], O_RDONLY);
                if (fd < 0) {
                    perror("myshell: open");
                    exit(EXIT_FAILURE);
                }
                if (dup2(fd, STDIN_FILENO) < 0) {
                    perror("myshell: dup2");
                    exit(EXIT_FAILURE);
                }
                close(fd);
                args[i] = NULL; // Cut off args
                break;
            }
        }

        // Execute binary
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
