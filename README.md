# myshell — Quirky POSIX Compliant Unix Shell

`myshell` is a simple light weight, Unix command line shell with some fun and interesting feature written from scratch in C, and using a WSL2/Ubuntu environment. Myshell was built with a focus on practicing the fundamentals of systems programming as a craft, it adhering to compilation standards (`-Wall -Wextra -Werror -pedantic`).

---

## Feature & Implementation Stages

The shell was developed progressing through iterations, from a basic REPL(read, eval, print Loop) common for modern programming languages to more robust process orchestration:

1. **Core Infrastructure Components & REPL:**
* A continuous read, eval, print Loop using the GNU Readline.
* Token parsing with `strtok()` to split the command line input into argument vectors (`char **args`).
* For the external binary execution with POSIX system calls, the following was used: `fork()` for process creation and `execvp()` for binary replacement.


2. **Built in Commands:**
* `cd [path]`: Changing the working directory using `chdir()`.
* `exit`: Terminating the shell efficiently.
* `history`: Displaying an indexed list of the commands entered during that current session using Readline's `history_list()`.


3. **I/O Redirection (`>`, `<`):**
* Output redirection (`>`) opens or can create files with `open()`, and truncating existing content, and then hooks them with `dup2(fd, STDOUT_FILENO)` inside that child process.
* Input redirection (`<`) hooking files to `STDIN_FILENO` so they can be read.


4. **Command Piping (`|`):**
* For implementing inter process communication (IPC) using a `pipe()` system call.
* It's able to spawns two child processes, and bridging the output of the left command to the standard input of the right command with file descriptor duplication and proper clean up of any unused pipe ends.


5. **GNU Readline & Advanced UX:**
* Integrated `libreadline-dev` that supports cursor navigation (left/right arrow movement) and command history cycling using (up/down arrows).
* Recursive directory completion engine (`opendir`, `readdir`, `closedir`) mapped to the Readline menu completion hook, enabling a basic type of interactive search and tab selection.



---

## Development Challenges & Solutions

### Strict Compilation & The `-Werror` Constraint

Because thia type of project enforced `-Werror` (treating all the compiler warnings as errors), minor syntax or code hygiene oversights did at times halt the build process.

A notable instance occurred while implementating the recursive directory tab completion feature:

* **The Error:** GCC threw a compilation failure:
```text
main.c: In function ‘recursive_file_generator’:
main.c:190:17: error: unused variable ‘st’ [-Werror=unused-variable]
  190 |     struct stat st;
      |                 ^~
cc1: all warnings being treated as errors

```


* **The Fix:** The `struct stat st;` variable was declared for previous file status checks that were now being handled with name matching logic instead. Satisfying strict compiler checks, just needed the unused variable to be stripped from the `recursive_file_generator`, allowing `make` to compile `myshell` successfully. Also other unused parameters like `end` in the custom completion hooks were explicitly silenced using `(void)end;`.

---

## Installation & Quick Start

### Prerequisites

Install the required development toolchains, Readline headers, and optional utility packages in your WSL2 Ubuntu terminal:

```bash
sudo apt update && sudo apt install build-essential libreadline-dev poppler-utils -y

```

### Build & Run

1. Clone or navigate to the repository directory.
2. Compile the project using GNU Make:
```bash
make

```


3. Launch your custom shell:
```bash
./myshell

```

## Documentation
To help explain the core systems concepts used in this project, check out our reference guides:
- [Unix Systems Guide](docs/unix-systems.pdf): Deep dive into POSIX syscalls, process spawning (`fork`/`execvp`), file descriptor redirection (`dup2`), and IPC pipes.
- [C Programming Guide](docs/c-programming.pdf): Reference notes on memory safety, string tokenization, and pointer arithmetic.

## PDF Text Extraction & Viewing

`myshell` supports text extraction from a PDF documents using standard Unix pipelines combined with `poppler-utils` (`pdftotext`). 

`poppler-utils` **installation:** sudo apt update && sudo apt install poppler-utils -y

### Example Usage

 **Print extracted text to standard output (acting as a filter):**
  ```bash
  pdftotext document.pdf -

**Searching for conecpt or keywords inside the PDF can be done with using a pipe:**
  ```bash
  pdftotext document.pdf - | grep "keyword"

**To extract and save text to a new file type use the follwoing**
  ```bash
  pdftotext document.pdf extracted_output.txt

