#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <dirent.h>
#include <sys/stat.h>

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

    if (strcmp(args[0], "history") == 0) {
        HIST_ENTRY **the_list = history_list();
        if (the_list) {
            for (int i = 0; the_list[i] != NULL; i++) {
                printf("%d  %s\n", i + history_base, the_list[i]->line);
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

// Search 1.1 for recursive directory search completetion
char *recursive_file_generator(const char *text, int state) {
    static DIR *dir = NULL;
    static char prefix[256];
    struct dirent *entry;
    char fullpath[512];

    // If state equals 0, it's a new completion attempt; open root/current dir
    if (state == 0) {
        dir = opendir(".");
        if (!dir) return NULL;
        // Text contains what user typed after '/'
        strncpy(prefix, text, sizeof(prefix) - 1);
    }

    while ((entry = readdir(dir)) != NULL) {
        // Skips any hidden files/directories (. and ..)
        if (entry->d_name[0] == '.') continue;

        snprintf(fullpath, sizeof(fullpath), "%s", entry->d_name);

        // Check if characters matche the prefix typed thus far
        if (strncmp(fullpath, prefix, strlen(prefix)) == 0) {
            // Allocates a copy for readline to manage
            return strdup(fullpath);
        }
    }

    closedir(dir);
    dir = NULL;
    return NULL;
}

// Readline search completion entry hook
char **my_completion(const char *text, int start, int end) {
    (void)end; // Silence the unused parameter warning
    char **matches = NULL;
    // If the line starts with '/', use the recursive file generator
    if (start > 0 && rl_line_buffer[0] == '/') {
        matches = rl_completion_matches(text, recursive_file_generator);
    }
    return matches;
} 

// main 
int main(void) {
    char *line;
    char *args[MAX_ARGS];

    // Enable search menu completion (Up/Down arrows nav cycling through matches)
    rl_variable_bind("menu-complete-display-prefix", "on");
    rl_bind_key('\t', rl_complete);
    
    // Set the custom completion function
    rl_attempted_completion_function = my_completion;

    // Main REPL Loop using GNU Readline
    while (1) {
        line = readline("myshell> ");

        if (line == NULL) {
            // End of File (Ctrl+D)
            printf("\n");
            break;
        }

        // Ignore empty lines
        if (line[0] == '\0') {
            free(line);
            continue;
        }

        // Adding command history (for up/down arrow navigation)
        add_history(line);

        parse_line(line, args);

        if (args[0] == NULL) {
            free(line);
            continue;
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
            args[pipe_index] = NULL;
            char **args2 = &args[pipe_index + 1];
            if (args2[0] == NULL) {
                fprintf(stderr, "myshell: syntax error near unexpected token `|'\n");
            } else {
                launch_pipe(args, args2);
            }
        } else {
            if (!handle_builtin(args)) {
                launch_process(args);
            }
        }

        free(line);
    }

    return 0;
}
