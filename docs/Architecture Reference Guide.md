# Architecture & Systems Reference Guide: `myshell`

This document aims to provide a detailed technical overview of core Unix systems programming principles and C language concepts I used in the design and implementation of `myshell`. It bridges the theoretical operating system concepts with their practical application used within this application codebase.

---

## 1. Core Architecture & REPL Loop

At its core, `myshell` operates like a classic read eval print loop(REPL). It initializes the environment, displaying an interactive prompt showing a live timestamp(for fun & **utility** ;), it accepts user input via the usual GNU Readline, parses tokens and dispatches executions.

```
┌───────────┐      ┌───────────┐      ┌─────────────┐      ┌─────────────┐
│   Read    │ ───> │   Eval    │ ───> │    Print    │ ───> │    Loop     │
│ readline()│      │ fork/exec │      │ exit status │      │ (next iter) │
└───────────┘      └───────────┘      └─────────────┘      └─────────────┘
```

### Dynamic Timestamp Prompt (`myshell:HH:MM:SS>`)
To provide a modern informative user experience similar to more advanced shells, `myshell` constructs prompts dynamically on every iteration using the standard time librarie (`<time.h>`):
* **`time()`**: Retrieves the local sysme current calendar and time (`time_t`).
* **`localtime()`**: Converts the calendar time into a broken-down `struct tm` representation aligned with local time zones.
* **`strftime()`**: Formats the hour, minute, and second fields into a fixed character buffer, which is subsequently formatted via `snprintf()` into the prompt string passed into GNU Readline.

---

## 2. Unix Systems Programming & Process Lifecycle

The Unix operating system managing system processes as independent execution context that is isolated by a virtual memory space. To implement a shell direct manipulation of process lifecycles through system call are required.

### Process Spawning (`fork()` and `execvp()`)
When a user executes an external binary (e.g., `ls`, `grep`), `myshell` cannot simply run it in the main process without replacing the shell itself. Instead, it utilizes a two-step pattern:
1. **`fork()`**: Clones the calling process. The operating system creates an exact duplicate of the parent process (the shell), returning `0` to the newly created child process and the child's process ID (`PID`) to the parent.
2. **`execvp()`**: Inside the child process branch, `execvp` replaces the current process image with the new executable binary found via the `PATH` environment variable. It takes the command name and an argument vector (`char *argv[]`).

### Process Synchronization (`waitpid()`)
Because `fork()` runs asynchronously, the parent shell must wait for the child process to complete before displaying the next prompt. 
* The parent calls `waitpid(pid, &status, 0)`, blocking execution until the specified child process terminates.
* This ensures sequential command execution and allows the shell to capture and inspect exit statuses or handle abnormal terminations (e.g., signals like SIGINT or Segmentation Faults).

### Inter-Process Communication (IPC) via Pipes (`pipe()`)
Pipelines (`|`) allow the output of one command to flow directly into the input of another without intermediate disk writes. 
* **`pipe(int fildes[2])`**: Creates a unidirectional data channel, returning two file descriptors: `fildes[0]` for reading and `fildes[1]` for writing.
* **Orchestration**: Before spawning processes, the shell creates a pipe, forks child processes for both sides of the pipeline, and redirects file descriptors using `dup2()`.

### File Descriptor Redirection (`dup2()`)
Input (`<`) and output (`>`) redirection modify where a process reads its data or writes its results by manipulating standard file descriptors:
* `STDIN_FILENO` (0)
* `STDOUT_FILENO` (1)
* `STDERR_FILENO` (2)

Using `dup2(oldfd, newfd)`, the shell replaces standard input or output with an opened file descriptor (obtained via `open()`). For example, redirecting output (`ls > output.txt`) opens `output.txt` and duplicates its file descriptor onto `STDOUT_FILENO` prior to calling `execvp()`.

---

## 3. C Programming Mechanics & Memory Management

Programming a system level shell in C requires adherence to memory safety practice, using precise pointer math and well structured resource management.

### String Tokenization (`strtok()`)
Command line parsing relies on breaking down the raw user input strings and into individual arguments that are separated by whitespace. 
* **`strtok()`**: Scans the input string, replacing delimiter characters (spaces, tabs, newlines) with null terminators (`\0`) and returning pointers to the beginning of each token. These pointers populate the `char *args[]` array required by `execvp()`.

### Dynamic Memory Allocation & Safety
GNU Readline does manage the primary input buffer memory internally, althoug some custom data structures, path buffers and completion matching routines need careful allocation:
* **`strdup()`**: Frequently used to duplicate strings during directory traversal and completion generation, requiring explicit callers to manage memory lifetimes to prevent leaks.
* **Header Hygiene & Strict Compilation**: The codebase compiles with `-Wall -Wextra -Werror -pedantic`. This ensures zero tolerance for uninitialized variables, unused parameters (silenced explicitly via casting like `(void)end;`), or implicit type conversions.

### GNU Readline & Custom Completion Integration
To elevate interactivity, `myshell` integrates the GNU Readline library (`libreadline-dev`):
* **Line Editing & History**: Provides out-of-the-box cursor movement, character insertion/deletion, and persistent command history tracking via `history_list()`.
* **Menu Completion Hook (`rl_attempted_completion_function`)**: Intercepts tab presses when a line begins with a `/` character. It registers a custom generator function that scans the local directory recursively (`opendir`, `readdir`, `closedir`), matches user-typed prefixes using `strncmp()`, and presents an interactive menu where users can cycle through options using the up/down arrow keys and select completions via the `Tab` key.

---

## Summary Table: System Calls & Standard Library Mapping

| Component / Feature | Primary C Function / Syscall | Associated Header | Purpose |
| :--- | :--- | :--- | :--- |
| **REPL Input** | `readline()` | `<readline/readline.h>` | Interactive prompt management and line editing |
| **Timestamp Prompt** | `time()`, `localtime()`, `strftime()` | `<time.h>` | Live clock data formatting for shell prompts |
| **Process Creation** | `fork()` | `<unistd.h>` | Spawns duplicate child execution contexts |
| **Binary Execution** | `execvp()` | `<unistd.h>` | Replaces process image with external command |
| **Process Sync** | `waitpid()` | `<sys/wait.h>` | Suspends parent until child execution terminates |
| **File I/O Redirection**| `open()`, `dup2()` | `<fcntl.h>`, `<unistd.h>` | Redirects standard input/output to files |
| **Pipelines** | `pipe()` | `<unistd.h>` | Creates unidirectional IPC data channels |
| **Tab Completion** | `opendir()`, `readdir()` | `<dirent.h>` | Recursive directory scanning for interactive menu completion |