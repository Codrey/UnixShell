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

// Execution of pipeline between two commands (cmd1 | cmd2)
int launch_pipe(char **args1, char **args2) {
    int pipefd[2];
    pid_t pid1, pid2;
    int status1, status2;

    if (pipe(pipefd) < 0) {
        perror("myshell: pipe");
        return 1;
    }

    pid1 = fork();
    if (pid1 < 0) {
        perror("myshell: fork");
        return 1;
    }

    if (pid1 == 0) {
        // First child: Redirect stdout to pipe write end
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);

        if (execvp(args1[0], args1) == -1) {
            perror("myshell");
        }
        exit(EXIT_FAILURE);
    }

    pid2 = fork();
    if (pid2 < 0) {
        perror("myshell: fork");
        return 1;
    }

    if (pid2 == 0) {
        // Second child: Redirect stdin to pipe read end
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);

        if (execvp(args2[0], args2) == -1) {
            perror("myshell");
        }
        exit(EXIT_FAILURE);
    }

    // Parent closes both ends of the pipe
    close(pipefd[0]);
    close(pipefd[1]);

    // Wait for both children to finish
    waitpid(pid1, &status1, 0);
    waitpid(pid2, &status2, 0);

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
        printf("myshell> ");
        fflush(stdout);

        if (getline(&line, &bufsize, stdin) == -1) {
            if (feof(stdin)) {
                printf("\n");
                break;
            } else {
                perror("myshell: getline");
                break;
            }
        }

        parse_line(line, args);

        if (args[0] == NULL) {
            continue; // Empty line
        }

        // Check for a pipe operator
        int has_pipe = 0;
        int pipe_index = -1;
        for (int i = 0; args[i] != NULL; i++) {
            if (strcmp(args[i], "|") == 0) {
                has_pipe = 1;
                pipe_index = i;
                break;
            }
        }

        if (has_pipe) {
            args[pipe_index] = NULL; // Split args into left side
            char **args2 = &args[pipe_index + 1]; // Right side of pipe
            if (args2[0] == NULL) {
                fprintf(stderr, "myshell: syntax error near unexpected token `|'\n");
            } else {
                launch_pipe(args, args2);
            }
        } else {
            // Execute if not a built-in
            if (!handle_builtin(args)) {
                launch_process(args);
            }
        }

    } while (status);

    free(line);
    return 0;
}
