# Operating System Concepts & Shell Theory
# The Complete Guide for Shelli

> Everything you need to understand what happens inside a shell,
> from the operating system up to Shelli's architecture.
> Written from scratch - assumes no prior OS knowledge.

---

## Table of Contents

1. [What is an Operating System?](#1-what-is-an-operating-system)
2. [The Kernel & System Calls](#2-the-kernel--system-calls)
3. [What is a Shell?](#3-what-is-a-shell)
4. [Processes - The Foundation of Everything](#4-processes---the-foundation-of-everything)
5. [File Descriptors - How Programs Do I/O](#5-file-descriptors---how-programs-do-io)
6. [The fork() + exec() Pattern](#6-the-fork--exec-pattern)
7. [Signals - Asynchronous Notifications](#7-signals---asynchronous-notifications)
8. [Environment Variables](#8-environment-variables)
9. [The Shell Pipeline: Input to Output](#9-the-shell-pipeline-input-to-output)
10. [Stage 1: Lexical Analysis (Tokenization)](#10-stage-1-lexical-analysis-tokenization)
11. [Stage 2: Parsing & The Abstract Syntax Tree (AST)](#11-stage-2-parsing--the-abstract-syntax-tree-ast)
12. [Stage 3: Word Expansion](#12-stage-3-word-expansion)
13. [Stage 4: Execution](#13-stage-4-execution)
14. [Pipes - Connecting Processes](#14-pipes---connecting-processes)
15. [I/O Redirection](#15-io-redirection)
16. [Built-in Commands - Why They Exist](#16-built-in-commands---why-they-exist)
17. [Shell Functions & Variable Scoping](#17-shell-functions--variable-scoping)
18. [Job Control](#18-job-control)
19. [Heredoc Handling](#19-heredoc-handling)
20. [Command Substitution](#20-command-substitution)
21. [Multi-line Input & RC Files](#21-multi-line-input--rc-files)
22. [History & Smart Suggestions](#22-history--smart-suggestions)
23. [Memory Management in C](#23-memory-management-in-c)
24. [The TUI (Terminal User Interface) - Overview](#24-the-tui-terminal-user-interface---overview)
25. [The Complete Picture: End-to-End Walkthrough](#25-the-complete-picture-end-to-end-walkthrough)
26. [What Changed: Flat Pipeline vs. Full AST](#26-what-changed-flat-pipeline-vs-full-ast)
27. [Quick Reference Card](#27-quick-reference-card)

---

## 1. What is an Operating System?

An **Operating System (OS)** is the software that sits between your hardware (CPU, RAM, disk) and your applications (browser, terminal, games). It is the manager of your computer.

Think of your computer as a restaurant:

| Analogy | Computer |
|---------|----------|
| Kitchen equipment (stove, fridge) | **Hardware** (CPU, RAM, Disk) |
| Restaurant manager | **Operating System** |
| Chefs cooking dishes | **Applications/Programs** |
| You, the customer ordering food | **The User** |

### What the OS does:

1. **Process Management** - Decides which program gets CPU time and when. Your computer runs hundreds of programs simultaneously, but the CPU can only do one thing at a time per core. The OS rapidly switches between them (context switching), giving the illusion of parallelism.

2. **Memory Management** - Each program gets its own private memory space. The OS makes sure programs can't read or write each other's memory (protection). It also handles virtual memory - using disk space when RAM is full.

3. **File System** - Organizes data on disk into files and directories. Provides a uniform interface so programs don't need to know about disk sectors or blocks.

4. **Device Management** - Provides drivers so programs can talk to keyboards, screens, network cards, etc. without knowing hardware details.

5. **Security & Access Control** - File permissions, user accounts, process isolation.

```
┌─────────────────────────────────────────────────────────┐
│                    YOU (User)                            │
│                        │                                │
│              ┌─────────▼─────────┐                      │
│              │     SHELL         │  ← You talk to this  │
│              └────────┬──────────┘                      │
│                       │                                 │
│         ┌─────────────▼─────────────┐                   │
│         │   OPERATING SYSTEM        │                   │
│         │   (Kernel)                │                   │
│         │  • Process Management     │                   │
│         │  • Memory Management      │                   │
│         │  • File System            │                   │
│         │  • Device Drivers         │                   │
│         └───────────┬───────────────┘                   │
│                     │                                   │
│    ┌────────────────▼────────────────────┐              │
│    │          HARDWARE                   │              │
│    │   CPU  •  RAM  •  Disk  •  Screen   │              │
│    └─────────────────────────────────────┘              │
└─────────────────────────────────────────────────────────┘
```

---

## 2. The Kernel & System Calls

### The Kernel

The **kernel** is the core part of the OS. It runs in **privileged mode** (also called kernel mode or ring 0), meaning it has unrestricted access to all hardware. Regular programs run in **user mode** (ring 3), where they CANNOT directly access hardware.

Why this separation? **Safety**. If any program could directly write to disk or read any memory address, one buggy program could destroy your entire system. The kernel acts as a gatekeeper.

### System Calls (syscalls)

Since programs can't access hardware directly, they ask the kernel to do things for them through **system calls**. A system call is a controlled entry point into the kernel.

```
Your Program (user mode)
        │
        │  "Hey OS, I need to read a file"
        │
        ▼  ──── System Call Boundary ────
        │
      Kernel (privileged mode)
        │
        │  Accesses disk, reads data
        │
        ▼  ──── Return to user mode ────
        │
Your Program gets the data
```

### System Calls Used in Shelli

| System Call | What it does | Where Shelli uses it |
|-------------|-------------|---------------------|
| `fork()` | Create a copy of the current process | Creating child processes to run commands |
| `execvp()` | Replace current process with a new program | Loading the actual command (ls, grep, etc.) |
| `waitpid()` | Wait for a child process to finish | Parent shell waits for command to complete |
| `pipe()` | Create a one-way communication channel | Connecting `cmd1 | cmd2` |
| `dup2()` | Redirect a file descriptor | Making stdout go to a file or pipe |
| `open()` | Open a file | For `>`, `<`, `>>` redirections |
| `close()` | Close a file descriptor | Cleanup after dup2, pipe operations |
| `read()` | Read data from a file descriptor | Reading pipe output, capturing command output |
| `write()` | Write data to a file descriptor | Writing heredoc content to pipe |
| `chdir()` | Change the current working directory | The `cd` builtin |
| `getcwd()` | Get the current working directory | The `pwd` builtin |
| `kill()` | Send a signal to a process | Forwarding Ctrl+C, job control (SIGCONT) |
| `signal()` / `sigaction()` | Set up signal handlers | Handling Ctrl+C (SIGINT), ignoring SIGQUIT |
| `getpid()` | Get the current process ID | Expanding `$$` (process ID variable) |
| `setenv()` | Set an environment variable | `export` builtin |
| `getenv()` | Get an environment variable | Variable expansion, `$HOME`, `$PATH` |
| `unsetenv()` | Remove an environment variable | `unset` builtin |
| `access()` | Check file permissions | `type` builtin, checking if files exist |
| `stat()` / `lstat()` | Get file information | `test -f`, `test -d` file checks |
| `glob()` | Pathname pattern matching | Expanding `*.c` into matching filenames |
| `_exit()` | Terminate a process immediately | Child process exits after exec failure |
| `fopen()`/`fclose()`/`fgets()` | File I/O (C library wrappers) | Reading `.shellirc`, `source` command |

---

## 3. What is a Shell?

A **shell** is a program that:
1. **Reads** a command from the user (text input)
2. **Interprets** what the user wants (parses the command)
3. **Asks the OS** to execute it (through system calls)
4. **Shows the results**
5. **Repeats** forever until the user exits

It's called a "shell" because it's the outer layer around the kernel - the kernel is the nut, the shell wraps around it. You interact with the shell, which then talks to the kernel on your behalf.

### Types of Shells

```
GUI Shells:  Windows Explorer, macOS Finder (point and click)
CLI Shells:  bash, zsh, sh, fish, ksh, tcsh
             shelli (our educational shell!)
```

### Why CLI Shells?

| Advantage | Example |
|-----------|---------|
| **Speed** | Type `rm *.tmp` vs clicking 100 files |
| **Automation** | Write scripts that repeat tasks |
| **Remote Access** | SSH into servers (no GUI available) |
| **Power** | Pipe commands together: `cat log | grep ERROR | wc -l` |
| **Low Resources** | Almost no memory or CPU needed |

### The REPL Loop

Every shell runs a **REPL** - Read, Eval, Print, Loop:

```
┌──────────────────────────────────────┐
│   ┌──────────┐                       │
│   │   READ   │  Wait for user input  │
│   └────┬─────┘                       │
│        ▼                             │
│   ┌──────────┐                       │
│   │   EVAL   │  Lex → Parse → AST    │
│   │          │  → Expand → Execute   │
│   └────┬─────┘                       │
│        ▼                             │
│   ┌──────────┐                       │
│   │  PRINT   │  Show result/output   │
│   └────┬─────┘                       │
│        ▼                             │
│   ┌──────────┐                       │
│   │   LOOP   │  Go back to READ      │
│   └──────────┘                       │
└──────────────────────────────────────┘
```

**In Shelli's main.c** (simplified):
```c
while (!should_exit) {
    char *line = tui_read_line();           // READ
    lexer_tokenize(line, &tokens, ...);     // EVAL step 1: tokenize
    parser_parse_ast(&tokens, &ast, ...);   // EVAL step 2: parse into AST
    capture_expansions(&sd, ast);           // EVAL step 3: expand variables/globs
    executor_run_ast_capture(ast, output, ...); // EVAL step 4: execute
    tui_stage_browser(&sd);                 // PRINT: show results
    // automatically loops                   // LOOP
}
```

---

## 4. Processes - The Foundation of Everything

### What is a Process?

A **process** is a running instance of a program.

- **Program** = code sitting on disk (like a recipe in a cookbook)
- **Process** = that program actually running in memory (like a chef cooking that recipe)

You can run the same program multiple times, creating multiple processes. Each has its own independent state.

### Process Anatomy

Every process has:

```
┌──────────────────────────────────────────────────┐
│                    PROCESS                        │
├──────────────────────────────────────────────────┤
│  PID: 1234              (Process ID - unique)    │
│  PPID: 5678             (Parent Process ID)      │
│                                                  │
│  ┌────────────────────────────────────────────┐  │
│  │            MEMORY SPACE                    │  │
│  │  ┌─────────┐ ┌─────────┐ ┌────────────┐   │  │
│  │  │  Code   │ │  Data   │ │   Stack    │   │  │
│  │  │ (text)  │ │  (vars) │ │(functions) │   │  │
│  │  └─────────┘ └─────────┘ └────────────┘   │  │
│  └────────────────────────────────────────────┘  │
│                                                  │
│  ┌────────────────────────────────────────────┐  │
│  │         FILE DESCRIPTORS                   │  │
│  │  0 = stdin   (keyboard input)              │  │
│  │  1 = stdout  (screen output)               │  │
│  │  2 = stderr  (error output)                │  │
│  │  3, 4, ...   (open files, pipes)           │  │
│  └────────────────────────────────────────────┘  │
│                                                  │
│  Environment Variables: PATH, HOME, USER, etc.   │
│  Current Directory: /Users/zebra/shelli          │
│  Exit Code: 0 (success) or 1-255 (error)        │
└──────────────────────────────────────────────────┘
```

### PID (Process ID)

Every process gets a unique integer ID from the kernel. The very first process (init/launchd) gets PID 1. All other processes form a tree:

```
              init (PID 1)
                  │
      ┌───────────┼────────────┐
      │           │            │
   systemd     login        sshd
      │           │            │
 ┌────┴────┐   terminal     session
 │         │      │            │
nginx  postgres  zsh          zsh
                  │
             ┌────┴────┐
             │         │
          shelli     vim
             │
         ┌───┴───┐
         ls    grep
```

Every process (except PID 1) has a **parent** (PPID). When you run `ls` in shelli, shelli is the parent and `ls` is the child.

### Exit Codes

Every process returns an **exit code** (0-255) when it finishes:
- **0** = Success
- **Non-zero** = Failure (different values mean different errors)
- **127** = Command not found (convention)
- **128 + N** = Killed by signal N (e.g., 130 = killed by SIGINT/Ctrl+C)

Shelli tracks exit codes in `last_exit` and makes them available as `$?`.

---

## 5. File Descriptors - How Programs Do I/O

### What is a File Descriptor?

A **file descriptor (FD)** is a small non-negative integer that acts as a handle (reference) to an open file, pipe, socket, or other I/O resource. The kernel maintains a table of open file descriptors for each process.

Every process starts with three standard file descriptors:

| FD | Name | C Constant | Default Target | Purpose |
|----|------|-----------|----------------|---------|
| 0 | stdin | `STDIN_FILENO` | Keyboard | Where input comes from |
| 1 | stdout | `STDOUT_FILENO` | Screen/Terminal | Where normal output goes |
| 2 | stderr | `STDERR_FILENO` | Screen/Terminal | Where error messages go |

### Why FDs Matter for Shells

**Piping** and **redirection** work by changing what file descriptors point to:

```
Normal:               ls > file.txt:          ls | grep foo:
┌────┐                ┌────┐                  ┌────┐        ┌──────┐
│ ls │                │ ls │                  │ ls │        │ grep │
│    │                │    │                  │    │        │      │
│ FD0│→ keyboard      │ FD0│→ keyboard        │ FD0│→ kbd   │ FD0 │→ PIPE
│ FD1│→ screen        │ FD1│→ file.txt        │ FD1│→ PIPE  │ FD1 │→ screen
│ FD2│→ screen        │ FD2│→ screen          │ FD2│→ scrn  │ FD2 │→ screen
└────┘                └────┘                  └────┘        └──────┘
```

The key insight: **programs don't know** their stdout was redirected. `ls` just writes to FD 1. Whether FD 1 points to the screen, a file, or a pipe - `ls` doesn't care. This is the elegance of the Unix design.

---

## 6. The fork() + exec() Pattern

This is **THE** most important OS concept for understanding shells.

### The Problem

When you type `ls` in a shell, the shell needs to run the `ls` program. But the shell itself needs to keep running afterward. How?

### The Solution: fork() + exec()

**Step 1: fork()** - The shell creates a **copy** of itself.

```
Before fork():                    After fork():
┌──────────────────┐              ┌──────────────────┐   ┌──────────────────┐
│   SHELL          │              │   SHELL (parent) │   │   SHELL (child)  │
│   PID: 1000      │              │   PID: 1000      │   │   PID: 1001      │
│   Running...     │              │   fork() returned │   │   fork() returned│
│                  │              │   1001 (child PID)│   │   0 (I am child) │
└──────────────────┘              └──────────────────┘   └──────────────────┘
```

`fork()` returns **twice** - once in each process:
- In the **parent**: returns the child's PID (positive number)
- In the **child**: returns 0

This is how the program knows which process it is.

**Step 2: exec()** - The child **replaces** itself with the actual program.

```
After fork():                     After exec("ls"):
┌──────────────────┐              ┌──────────────────┐
│   SHELL (child)  │              │   ls             │
│   PID: 1001      │    ────►     │   PID: 1001      │ (same PID!)
│   Code: shell    │              │   Code: ls       │
│   Data: shell    │              │   Data: ls       │
└──────────────────┘              └──────────────────┘
```

`exec()` does NOT create a new process. It **replaces** the current process's code, data, and stack with the new program. Same PID, same file descriptors, but completely different code.

**Step 3: wait()** - The parent waits for the child to finish.

```
┌──────────────────┐              ┌──────────────────┐
│   SHELL (parent) │              │   ls (child)     │
│   PID: 1000      │              │   PID: 1001      │
│   waitpid(1001)  │  ◄── waits   │   Running...     │
│   Blocked...     │              │   Outputs files  │
│                  │              │   Exits (code 0) │
└──────────────────┘              └──────────────────┘
                    │
          child done│
                    ▼
┌──────────────────┐
│   SHELL (parent) │
│   PID: 1000      │
│   Continues...   │
│   Shows prompt   │
└──────────────────┘
```

### Why fork + exec, not just "run"?

1. **Safety**: The child is a separate process. If it crashes, the parent (shell) survives.
2. **Setup window**: Between fork() and exec(), we can redirect file descriptors, set up pipes, change signals - all before the program starts.
3. **Control**: The parent can wait (foreground) or continue (background).
4. **Unix Philosophy**: Small, composable tools connected together.

### The Code Pattern (used in Shelli's executor.c)

```c
pid_t pid = fork();

if (pid == 0) {
    // ─── CHILD PROCESS ───
    // This code only runs in the child

    signal(SIGINT, SIG_DFL);   // Reset signal handling
    signal(SIGQUIT, SIG_DFL);

    // Setup: redirect I/O BEFORE exec
    dup2(pipe_fd[1], STDOUT_FILENO);  // stdout → pipe

    // Replace myself with the actual program
    execvp("ls", argv);

    // If exec fails, we get here (exec doesn't return on success)
    fprintf(stderr, "shelli: ls: command not found\n");
    _exit(127);
}

// ─── PARENT PROCESS ───
// This code only runs in the parent (pid > 0)

fg_child_pid = pid;          // Track foreground child for signal forwarding
int status;
waitpid(pid, &status, 0);   // Block until child finishes
fg_child_pid = 0;            // Child is done

int exit_code = WEXITSTATUS(status);  // Extract exit code
```

### Why _exit() and not exit()?

In the child process, we use `_exit()` instead of `exit()`. The regular `exit()` flushes stdio buffers and runs cleanup handlers registered by the parent. Since the child is a copy of the parent, calling `exit()` could flush the parent's buffered output a second time, causing duplicated output. `_exit()` terminates immediately without any cleanup.

---

## 7. Signals - Asynchronous Notifications

### What is a Signal?

A **signal** is a software interrupt - a way for the OS (or another process) to notify a process that something happened. Think of it like someone tapping you on the shoulder while you're working.

### Common Signals

| Signal | Number | Triggered by | Default Action |
|--------|--------|-------------|----------------|
| SIGINT | 2 | Ctrl+C | Terminate process |
| SIGQUIT | 3 | Ctrl+\\ | Terminate with core dump |
| SIGTERM | 15 | `kill` command | Terminate gracefully |
| SIGKILL | 9 | `kill -9` | **Force** terminate (cannot be caught!) |
| SIGSTOP | 19 | Ctrl+Z | Suspend/stop process |
| SIGCONT | 18 | `fg`/`bg` | Resume stopped process |
| SIGCHLD | 17 | Child exits | Notify parent |

### How Shelli Handles Signals

Shelli sets up signal handlers in `main.c`:

```c
// Custom handler for SIGINT (Ctrl+C)
struct sigaction sa;
sa.sa_handler = handle_sigint;
sigemptyset(&sa.sa_mask);
sa.sa_flags = 0;
sigaction(SIGINT, &sa, NULL);

// Ignore SIGQUIT (Ctrl+\) in the shell itself
signal(SIGQUIT, SIG_IGN);
```

**The SIGINT handler**:
```c
static void handle_sigint(int sig) {
    interrupted = 1;
    pid_t fg = executor_get_fg_pid();
    if (fg > 0) {
        kill(fg, SIGINT);  // Forward Ctrl+C to the foreground child
    }
}
```

The key insight: When you press Ctrl+C, you want to kill the **running command** (like `cat` or `sleep`), NOT the shell itself. So:
- The shell catches SIGINT and forwards it to the foreground child process
- The shell ignores SIGQUIT entirely
- Child processes reset to default signal behavior (`SIG_DFL`) before exec()

### sig_atomic_t

The `interrupted` variable is declared as `volatile sig_atomic_t`. This is important because:
- `volatile`: Tells the compiler this variable can change at any time (from a signal handler), so don't optimize away reads
- `sig_atomic_t`: A type guaranteed to be readable/writable atomically (in one CPU instruction), so the signal handler and main code can safely share it

---

## 8. Environment Variables

### What Are They?

**Environment variables** are key-value pairs (`NAME=VALUE`) that every process inherits from its parent. They configure how programs behave.

| Variable | Purpose | Example |
|----------|---------|---------|
| `PATH` | Directories to search for commands | `/usr/bin:/bin:/usr/local/bin` |
| `HOME` | User's home directory | `/Users/zebra` |
| `USER` | Current username | `zebra` |
| `PWD` | Current working directory | `/Users/zebra/shelli` |
| `SHELL` | User's default shell | `/bin/zsh` |

### How They Work

When a process calls `fork()`, the child **inherits** all environment variables from the parent. When `execvp()` is called, the new program also gets these variables.

In Shelli:
- `getenv("HOME")` reads an environment variable
- `setenv("VAR", "value", 1)` sets one (the 1 means overwrite if exists)
- `unsetenv("VAR")` removes one
- The `export` builtin uses `setenv()` to make a shell variable visible to child processes
- The `unset` builtin uses `unsetenv()` to remove a variable

### The PATH Variable

When you type `ls`, how does the shell find `/bin/ls`? It uses the `PATH` environment variable:

1. Split `PATH` by `:` → get directories like `/usr/bin`, `/bin`, `/usr/local/bin`
2. For each directory, check if `<dir>/ls` exists and is executable
3. First match wins → pass the full path to `execvp()`

Actually, `execvp()` does this PATH search automatically (the `p` in `execvp` stands for "path search").

---

## 9. The Shell Pipeline: Input to Output

Here is what happens when you type a command in Shelli:

```
User types: "ls -la | grep foo > out.txt"
                    │
                    ▼
    ┌───────────────────────────────┐
    │  STAGE 1: LEXER (Tokenizer)  │
    │  "ls -la | grep foo > out"   │
    │            ↓                 │
    │  [WORD:ls] [WORD:-la] [PIPE] │
    │  [WORD:grep] [WORD:foo]      │
    │  [REDIR_OUT] [WORD:out.txt]  │
    └───────────────────────────────┘
                    │
                    ▼
    ┌───────────────────────────────┐
    │  STAGE 2: PARSER → AST       │
    │                              │
    │  Pipeline                    │
    │  ├─ Command: [ls, -la]       │
    │  └─ Command: [grep, foo]     │
    │       └─ redir_out: out.txt  │
    └───────────────────────────────┘
                    │
                    ▼
    ┌───────────────────────────────┐
    │  STAGE 3: EXPANSION          │
    │  (tilde, $VAR, glob, etc.)   │
    │  ~ → /Users/zebra            │
    │  $HOME → /Users/zebra        │
    │  *.c → file1.c file2.c       │
    └───────────────────────────────┘
                    │
                    ▼
    ┌───────────────────────────────┐
    │  STAGE 4: EXECUTOR           │
    │  fork() child processes      │
    │  pipe() to connect them      │
    │  dup2() to redirect I/O      │
    │  execvp() to run programs    │
    │  waitpid() for completion    │
    └───────────────────────────────┘
                    │
                    ▼
              Result + Exit Code
```

Each stage is a separate module in Shelli. This separation is a classic compiler/interpreter design pattern.

---

## 10. Stage 1: Lexical Analysis (Tokenization)

### What is Tokenization?

**Tokenization** (lexical analysis, or "lexing") is breaking a raw string into a sequence of meaningful pieces called **tokens**. It's like how you read a sentence - you don't see "Thequickbrownfox", you see separate words: "The", "quick", "brown", "fox".

### Token Types in Shelli

| Token Type | Symbol | Example |
|-----------|--------|---------|
| `TOK_WORD` | (any word) | `ls`, `-la`, `foo`, `"hello world"` |
| `TOK_PIPE` | `\|` | connects two commands |
| `TOK_REDIR_IN` | `<` | input redirection |
| `TOK_REDIR_OUT` | `>` | output redirection |
| `TOK_REDIR_APP` | `>>` | append redirection |
| `TOK_HEREDOC` | `<<` | here-document |
| `TOK_SEMI` | `;` | command separator |
| `TOK_AND` | `&&` | logical AND |
| `TOK_OR` | `\|\|` | logical OR |
| `TOK_BG` | `&` | background execution |
| `TOK_EOF` | (end) | end of input marker |

### How the Lexer State Machine Works

The lexer scans character by character using a **state machine** with four states:

```
          ┌─────────────────────────────────────────────┐
          │                                             │
          ▼                                             │
     ┌──────────┐    whitespace    ┌───────────────┐    │
     │  START   │ ───────────────► │  (skip them)  │ ───┘
     └────┬─────┘                  └───────────────┘
          │
          ├── letter/digit ────► STATE_WORD
          │                       Keep reading until whitespace or special char
          │                       Then emit TOK_WORD
          │
          ├── ' (single quote) ─► STATE_SQUOTE
          │                       Read everything literally until closing '
          │                       Nothing is special inside single quotes
          │
          ├── " (double quote) ─► STATE_DQUOTE
          │                       Read until closing "
          │                       Handle escape sequences: \" \\ \$ \`
          │
          ├── | ────────────────► Check next char:
          │                       || → emit TOK_OR
          │                       |  → emit TOK_PIPE
          │
          ├── & ────────────────► Check next char:
          │                       && → emit TOK_AND
          │                       &  → emit TOK_BG
          │
          ├── < ────────────────► Check next char:
          │                       << → emit TOK_HEREDOC
          │                       <  → emit TOK_REDIR_IN
          │
          └── > ────────────────► Check next char:
                                  >> → emit TOK_REDIR_APP
                                  >  → emit TOK_REDIR_OUT
```

### Quoting

Each token tracks whether it was **quoted** (the `quoted` field). This matters later for glob expansion:
- `*.c` (unquoted) → expands to matching filenames
- `"*.c"` (quoted) → stays as the literal string `*.c`

### Example

Input: `ls -la | grep "hello world" > out.txt`

```
Token 1: [WORD]      value="ls"           quoted=0
Token 2: [WORD]      value="-la"          quoted=0
Token 3: [PIPE]      value="|"            quoted=0
Token 4: [WORD]      value="grep"         quoted=0
Token 5: [WORD]      value="hello world"  quoted=1  ← quotes removed, space preserved
Token 6: [REDIR_OUT] value=">"            quoted=0
Token 7: [WORD]      value="out.txt"      quoted=0
Token 8: [EOF]                            (end marker)
```

Notice: the quotes around `"hello world"` are **removed** by the lexer, but it marks the token as `quoted=1`. The content `hello world` is preserved as a single token (with the space inside).

---

## 11. Stage 2: Parsing & The Abstract Syntax Tree (AST)

### What is Parsing?

**Parsing** (syntax analysis) takes the flat list of tokens and builds a **tree structure** that represents the meaning and hierarchy of the command.

Tokenizer says: "Here are the words."
Parser says: "Here's what they MEAN together, and how they're structured."

### What is an AST?

An **Abstract Syntax Tree (AST)** is a tree data structure where:
- Each **node** represents a construct in the language
- **Children** represent sub-components
- The **tree structure** captures nesting and hierarchy

### AST Node Types in Shelli

| Node Type | What it represents | Example |
|-----------|-------------------|---------|
| `NODE_COMMAND` | A simple command with args and redirects | `ls -la > out.txt` |
| `NODE_PIPELINE` | Commands connected by pipes | `ls \| grep foo \| wc` |
| `NODE_LIST` | Commands chained by `;`, `&&`, `\|\|`, `&` | `cmd1 && cmd2; cmd3` |
| `NODE_IF` | if/elif/else/fi construct | `if test -f x; then echo yes; fi` |
| `NODE_WHILE` | while/do/done loop | `while true; do echo x; done` |
| `NODE_UNTIL` | until/do/done loop | `until test -f x; do sleep 1; done` |
| `NODE_FOR` | for/in/do/done loop | `for x in a b c; do echo $x; done` |
| `NODE_FUNCTION_DEF` | Function definition | `greet() { echo hello; }` |
| `NODE_NOT` | Invert exit code | `! grep error log.txt` |
| `NODE_SUBSHELL` | Execute in child process | `( cd /tmp && ls )` |

### How the Parser Works: Recursive Descent

Shelli uses a **recursive descent parser**. This means there's a function for each grammar rule, and they call each other recursively to build the tree.

The grammar (simplified):
```
program      ::= list EOF
list         ::= and_or ((';' | '&') and_or)* [';' | '&']
and_or       ::= pipeline (('&&' | '||') pipeline)*
pipeline     ::= ['!'] command ('|' command)*
command      ::= simple_command | if_clause | while_clause | for_clause | ...
simple_cmd   ::= word+ (with redirects interspersed)
```

The parsing functions correspond directly to grammar rules:
```
parse_list()          ← handles ; and & chaining
  └─ parse_and_or()   ← handles && and ||
      └─ parse_pipeline()  ← handles |
          └─ parse_command()   ← dispatches to compound or simple
              ├─ parse_simple_command()  ← word + redirect parsing
              ├─ parse_if()
              ├─ parse_while()
              ├─ parse_until()
              ├─ parse_for()
              └─ parse_brace_group()
```

### Example: Building an AST

Input: `ls -la | grep foo && echo "found it"`

Tokens: `[ls] [-la] [|] [grep] [foo] [&&] [echo] [found it]`

AST:
```
NODE_LIST
├── entry[0]: (sep = &&)
│   └── NODE_PIPELINE
│       ├── NODE_COMMAND: argv=["ls", "-la"]
│       └── NODE_COMMAND: argv=["grep", "foo"]
│
└── entry[1]: (sep = NONE)
    └── NODE_COMMAND: argv=["echo", "found it"]
```

### Example: Nested Control Flow

Input: `if test -f config.txt; then echo "found"; else echo "missing"; fi`

AST:
```
NODE_IF
├── condition: NODE_COMMAND argv=["test", "-f", "config.txt"]
├── then_body: NODE_COMMAND argv=["echo", "found"]
└── else_body: NODE_COMMAND argv=["echo", "missing"]
```

### PARSE_INCOMPLETE: Multi-line Input

When the parser hits EOF inside a compound command (like `if` without `fi`), it returns `PARSE_INCOMPLETE` instead of an error. This tells main.c to prompt for more input, accumulating lines until the command is complete.

```c
if (result == PARSE_INCOMPLETE) {
    // User typed "if test -f x; then" and pressed Enter
    // Need more input - keep accumulating
    multiline = 1;
    continue;
}
```

The `in_compound` counter tracks nesting depth so the parser knows whether EOF means "incomplete" or "error".

---

## 12. Stage 3: Word Expansion

### What is Expansion?

**Expansion** transforms the raw words in a command into their final values before execution. This happens **after** parsing but **before** execution.

### Expansion Order (matching real shells)

Shelli follows the POSIX-defined expansion order:

| Order | Type | Before | After |
|-------|------|--------|-------|
| 1 | **Tilde expansion** | `~/docs` | `/Users/zebra/docs` |
| 2 | **Variable expansion** | `$HOME` | `/Users/zebra` |
| 3 | **Arithmetic expansion** | `$((2 + 3))` | `5` |
| 4 | **Command substitution** | `$(whoami)` | `zebra` |
| 5 | **Glob expansion** | `*.c` | `main.c lexer.c parser.c` |

### Tilde Expansion

`~` at the start of a word is replaced with the value of `$HOME`:
- `~/Documents` → `/Users/zebra/Documents`
- `~` → `/Users/zebra`
- `~user` → not supported (only `~` and `~/...`)

### Variable Expansion

Variables starting with `$` are replaced with their values:
- `$HOME` → `/Users/zebra`
- `${HOME}` → `/Users/zebra` (braces for clarity)
- `$?` → last command's exit code
- `$$` → current process ID (via `getpid()`)
- `$#` → number of positional parameters
- `$1`, `$2`, ... `$9` → positional parameters (function arguments)
- `$@`, `$*` → all positional parameters

### Arithmetic Expansion

`$(( expression ))` evaluates a mathematical expression:
- `$((2 + 3))` → `5`
- `$((x * 2))` → doubles the value of variable `x`
- Supports: `+`, `-`, `*`, `/`, `%`, `**` (power), `~` (bitwise NOT), `!` (logical NOT)

Shelli has a full **recursive descent arithmetic evaluator** built into `expand.c`. It follows standard operator precedence.

### Command Substitution

`$(command)` runs a command and substitutes its output:
- `echo "Today is $(date)"` → `echo "Today is Tue Feb 24 ..."`
- Implemented via a callback function that runs the full lex→parse→execute pipeline on the inner command

### Glob Expansion (Pathname Matching)

Unquoted words with `*`, `?`, or `[` are matched against filenames:
- `*.c` → all `.c` files in the current directory
- `file?.txt` → `file1.txt`, `file2.txt`, etc.
- `[abc].txt` → `a.txt`, `b.txt`, or `c.txt`
- **Quoted** words skip glob expansion: `"*.c"` stays as `*.c`

Shelli uses the POSIX `glob()` function for this. Glob can **grow** the argv array (one pattern can expand into many files).

### Expansion Happens Per-Command at Execution Time

This is important: expansion doesn't happen during parsing. It happens when the executor is about to run each command. This means:
- In a loop like `for x in *.c`, the glob is expanded once before the loop
- In `while ... do echo $x; done`, `$x` is expanded fresh each iteration
- The AST stores the **unexpanded** words; expansion creates working copies

---

## 13. Stage 4: Execution

### Tree-Walking Executor

The executor **walks** the AST tree, handling each node type:

```c
static int execute_node(AstNode *node) {
    switch (node->type) {
    case NODE_COMMAND:   return execute_simple_command(...);
    case NODE_PIPELINE:  return execute_pipeline_node(...);
    case NODE_LIST:      return execute_list_node(...);
    case NODE_IF:        return execute_if_node(...);
    case NODE_WHILE:     return execute_while_node(...);
    case NODE_UNTIL:     return execute_until_node(...);
    case NODE_FOR:       return execute_for_node(...);
    case NODE_NOT:       return execute_not_node(...);
    case NODE_SUBSHELL:  return execute_subshell_node(...);
    case NODE_FUNCTION_DEF: /* register function */ ...
    }
}
```

### Executing a Simple Command

For a command like `ls -la`:

1. **Clone** the command (so expansion doesn't modify the AST)
2. **Expand** words (tilde, variables, globs)
3. **Check** if it's a builtin → run in parent process
4. **Check** if it's a shell function → push scope, execute body
5. Otherwise: **fork()** → child does **exec()** → parent does **wait()**

### Executing a List (`;`, `&&`, `||`, `&`)

For `cmd1 && cmd2 || cmd3`:

```c
for each entry in list:
    if previous sep was && and previous failed → skip
    if previous sep was || and previous succeeded → skip
    execute the pipeline
    save exit code
```

- `;` → always execute next command
- `&&` → execute next only if previous **succeeded** (exit code 0)
- `||` → execute next only if previous **failed** (exit code non-zero)
- `&` → run in background (don't wait)

### Executing Control Flow

**if/elif/else/fi:**
```c
int cond = execute_node(condition);
if (cond == 0) {        // 0 = success in Unix!
    return execute_node(then_body);
} else {
    return execute_node(else_body);  // else_body may be another NODE_IF (elif)
}
```

**while/do/done:**
```c
while (1) {
    int cond = execute_node(condition);
    if (cond != 0) break;          // condition failed → stop
    execute_node(body);
    if (shell_break_count > 0) { shell_break_count--; break; }
    if (shell_continue_count > 0) { shell_continue_count--; continue; }
}
```

**for/in/do/done:**
```c
for (int i = 0; i < word_count; i++) {
    char *expanded = expand_word(words[i], 0);  // expand each word
    var_set(var_name, expanded);                 // set the loop variable
    execute_node(body);                          // run the body
    // check break/continue
}
```

### Executing a Subshell

`( commands )` runs in a child process (separate memory space):

```c
pid_t pid = fork();
if (pid == 0) {
    // Child: execute the body
    int ret = execute_node(body);
    _exit(ret);
}
// Parent: wait for child
waitpid(pid, &status, 0);
```

Changes inside a subshell (like `cd`) don't affect the parent shell.

---

## 14. Pipes - Connecting Processes

### What is a Pipe?

A **pipe** is a one-way communication channel between two processes. Data written to one end can be read from the other.

```
┌──────────────┐      PIPE       ┌──────────────┐
│   Process A  │ ──────────────► │   Process B  │
│  (producer)  │    bytes flow   │  (consumer)  │
│  stdout ─────┼─────────────────┼──► stdin     │
└──────────────┘                 └──────────────┘
```

### The pipe() System Call

```c
int pipe_fd[2];
pipe(pipe_fd);
// pipe_fd[0] = READ end  (the faucet - where data comes out)
// pipe_fd[1] = WRITE end (the hose - where data goes in)
```

The kernel allocates a buffer (typically 64KB) between the two file descriptors.

### How Shelli Connects `ls | grep foo`

```
STEP 1: Create pipe
    pipe(pipes[0])  →  pipes[0][0] = read end, pipes[0][1] = write end

STEP 2: Fork child 1 (ls)
    fork()
    Child 1:
        dup2(pipes[0][1], STDOUT_FILENO)  // stdout → pipe write end
        close(pipes[0][0])                 // close read end (don't need)
        close(pipes[0][1])                 // close original (dup2 made copy)
        execvp("ls", ...)                  // become ls

STEP 3: Fork child 2 (grep)
    fork()
    Child 2:
        dup2(pipes[0][0], STDIN_FILENO)   // stdin ← pipe read end
        close(pipes[0][1])                 // close write end
        close(pipes[0][0])                 // close original
        execvp("grep", ...)                // become grep

STEP 4: Parent closes all pipe ends
    close(pipes[0][0])
    close(pipes[0][1])
    waitpid() for all children
```

### Why Close Unused Pipe Ends?

This is critical and often confusing:

- If the **write end** isn't closed in ALL processes that don't need it, the reader **never gets EOF** and blocks forever
- `grep` would wait forever thinking more data might come, because the parent still has the write end open
- Rule: every process must close every pipe end it doesn't use

### The dup2() Function

```c
dup2(old_fd, new_fd);
```

Makes `new_fd` point to the same thing as `old_fd`:

```
Before dup2(pipe_fd[1], STDOUT_FILENO):
  FD 1 (stdout) → Screen
  pipe_fd[1]    → Pipe write end

After dup2(pipe_fd[1], STDOUT_FILENO):
  FD 1 (stdout) → Pipe write end    ← Changed!
  pipe_fd[1]    → Pipe write end    (still points there too)

Now printf() goes to the pipe instead of screen!
```

### Multi-Command Pipelines

For `cmd1 | cmd2 | cmd3`, Shelli creates `N-1` pipes:

```
pipes[0]:  cmd1.stdout ──► cmd2.stdin
pipes[1]:  cmd2.stdout ──► cmd3.stdin

┌──────┐  pipe[0]  ┌──────┐  pipe[1]  ┌──────┐
│ cmd1 │ ────────► │ cmd2 │ ────────► │ cmd3 │
└──────┘           └──────┘           └──────┘
```

The exit code of a pipeline is the exit code of the **last** command (cmd3).

---

## 15. I/O Redirection

### Output Redirection: `>`

Redirect stdout to a file (overwrite):

```c
int fd = open("output.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
//                           write      create     empty     permissions
dup2(fd, STDOUT_FILENO);   // stdout → file
close(fd);                  // close original fd
execvp(...);                // program writes to file
```

### Append Redirection: `>>`

Same as `>` but doesn't erase existing content:

```c
int fd = open("output.txt", O_WRONLY | O_CREAT | O_APPEND, 0644);
//                                                 append (don't truncate)
dup2(fd, STDOUT_FILENO);
close(fd);
```

### Input Redirection: `<`

Read input from a file instead of keyboard:

```c
int fd = open("data.txt", O_RDONLY);
dup2(fd, STDIN_FILENO);   // stdin ← file
close(fd);
execvp("sort", ...);      // sort reads from file
```

### Redirect Setup Timing

Redirects are set up **in the child process**, after `fork()` but before `exec()`. This is the "setup window" that makes fork+exec so powerful:

```c
pid_t pid = fork();
if (pid == 0) {
    // ── SETUP WINDOW ──
    // We can change file descriptors here without affecting the parent

    setup_redirects(cmd);   // Opens files, calls dup2()

    execvp(cmd->argv[0], cmd->argv);  // Now run the program
}
```

---

## 16. Built-in Commands - Why They Exist

### The Problem

Some commands **must** run inside the shell process itself, not in a child. The most important example is `cd`.

**Why can't `cd` be external?**

If we fork + exec'd `cd`:
1. Shell (PID 1000) forks → Child (PID 1001)
2. Child changes directory with `chdir("/tmp")`
3. Child exits
4. Shell (PID 1000) is **still in the original directory**!

Because each process has its own current directory, changing it in a child has no effect on the parent. So `cd` MUST call `chdir()` in the shell's own process.

### Shelli's Built-in Commands

| Builtin | Why it's builtin | System call / mechanism |
|---------|-----------------|------------------------|
| `cd` | Must change shell's own directory | `chdir()` |
| `pwd` | Reads shell's current directory | `getcwd()` |
| `exit` | Must terminate the shell process | Sets `should_exit` flag |
| `export` | Must modify shell's environment | `setenv()` + `var_set()` |
| `unset` | Must modify shell's environment | `unsetenv()` + `var_unset()` |
| `local` | Must modify shell's variable scope | `var_set_local()` |
| `return` | Must set shell's return flag | Sets `shell_return_flag` |
| `source` / `.` | Must execute in shell's context | Reads file, runs lex→parse→exec |
| `history` | Accesses shell's history | `tui_history_print()` |
| `echo` | Builtin for performance | Direct `printf()` / `putchar()` |
| `test` / `[` | Conditional expression evaluator | `stat()`, `access()`, string ops |
| `type` | Shows command type | Checks builtins, functions, PATH |
| `true`/`false` | Return 0/1 | Just `return 0` / `return 1` |
| `jobs` | Lists background jobs | Reads job table |
| `fg` / `bg` | Job control | `kill(SIGCONT)`, `waitpid()` |
| `break`/`continue` | Loop control | Sets `shell_break_count`/`shell_continue_count` |

### How Builtins Run Differently

In `execute_simple_command()`:
```c
// Builtins that MUST run in parent (they modify shell state)
if (is_builtin && (strcmp(cmd, "cd") == 0 || strcmp(cmd, "export") == 0 || ...)) {
    return builtin_execute(cmd, &should_exit);  // Run directly, no fork
}

// Regular commands
pid_t pid = fork();
if (pid == 0) {
    execvp(cmd, argv);  // Child becomes the program
}
waitpid(pid, &status, 0);
```

Some builtins CAN run in a child (like `echo` in a pipeline: `echo hello | grep h`). When a builtin appears in a pipeline, it runs in a forked child.

---

## 17. Shell Functions & Variable Scoping

### Shell Functions

Shell functions let you define reusable command groups:

```bash
greet() {
    echo "Hello, $1!"
}
greet World    # prints: Hello, World!
```

### How Functions Work in Shelli

1. **Definition**: When the parser encounters `name() { body }`, it creates a `NODE_FUNCTION_DEF` AST node
2. **Registration**: The executor calls `func_define(name, body)` which stores the function name and a **clone** of its AST body in the function table
3. **Invocation**: When a command word matches a function name, instead of fork+exec:
   - Push a new variable scope (`var_push_scope()`)
   - Set positional parameters (`$1`, `$2`, etc.) from the function arguments
   - Execute the function body (the stored AST)
   - Check for `return` statement
   - Pop the variable scope (`var_pop_scope()`)

### Variable Scoping

Shelli implements a **scope stack** for variables:

```
┌──────────────────────────────────────┐
│  Scope Stack                         │
│                                      │
│  ┌──────────────────────────┐        │
│  │ Local Scope (depth 2)   │ ← top  │  innermost function
│  │  x = "inner"            │        │
│  └──────────────────────────┘        │
│  ┌──────────────────────────┐        │
│  │ Local Scope (depth 1)   │        │  outer function
│  │  y = "outer"            │        │
│  └──────────────────────────┘        │
│  ┌──────────────────────────┐        │
│  │ Global Scope (depth 0)  │ ← base │  main shell
│  │  PATH = "/usr/bin:..."  │        │
│  │  HOME = "/Users/zebra"  │        │
│  └──────────────────────────┘        │
└──────────────────────────────────────┘
```

**Variable lookup** (`var_get`) searches from innermost scope outward:
1. Check local scopes (innermost first)
2. Check global scope
3. Fall through to `getenv()` (environment variables)

**`local` builtin** creates a variable in the current local scope only. When the function returns and the scope is popped, the variable disappears.

### Positional Parameters

Inside a function, `$1`, `$2`, etc. refer to the function's arguments:

```bash
greet() {
    echo "Hello, $1 and $2"
}
greet Alice Bob    # $1=Alice, $2=Bob
```

These are stored separately from regular variables in a `positional[]` array.

---

## 18. Job Control

### What is Job Control?

**Job control** lets you run commands in the background, suspend them, and bring them back to the foreground.

```bash
sleep 100 &        # Run in background (& operator)
jobs               # List background jobs
fg %1              # Bring job 1 to foreground
bg %1              # Continue stopped job in background
```

### How Jobs Work in Shelli

Each background process is tracked in a **job table**:

```c
typedef struct {
    int id;            // Job number (1, 2, 3, ...)
    pid_t pgid;        // Process Group ID
    JobState state;    // JOB_RUNNING, JOB_STOPPED, JOB_DONE
    char *command;     // Original command string
    int exit_status;   // Exit code when done
} Job;
```

### Key Job Control Operations

**Background (`&`)**: When a command ends with `&`, the shell forks the child but does NOT call `waitpid()`. Instead, it adds the child to the job table and immediately shows the prompt.

**jobs_check()**: Called at the start of each REPL iteration, this uses `waitpid(-1, &status, WNOHANG | WUNTRACED)` to non-blocking check if any background children have finished or stopped. `WNOHANG` means "don't block if nothing happened".

**fg (foreground)**: Sends `SIGCONT` to a stopped job's process group, then calls `waitpid()` to wait for it.

**bg (background)**: Sends `SIGCONT` to resume a stopped job, but doesn't wait.

### Process Status Macros

When `waitpid()` fills in the `status` variable, you use macros to interpret it:

| Macro | Purpose |
|-------|---------|
| `WIFEXITED(status)` | Did the process exit normally? |
| `WEXITSTATUS(status)` | What was the exit code? (0-255) |
| `WIFSIGNALED(status)` | Was the process killed by a signal? |
| `WTERMSIG(status)` | Which signal killed it? |
| `WIFSTOPPED(status)` | Was the process stopped (Ctrl+Z)? |
| `WSTOPSIG(status)` | Which signal stopped it? |

---

## 19. Heredoc Handling

### What is a Heredoc?

A **here-document** (heredoc) feeds multi-line text to a command's stdin:

```bash
cat <<EOF
Hello World
This is line 2
EOF
```

The text between `<<EOF` and `EOF` is sent to `cat`'s stdin.

### How Shelli Implements Heredocs

1. **Parsing**: The lexer produces `TOK_HEREDOC` + `TOK_WORD` (the delimiter). The parser stores the delimiter string in `cmd->heredoc_delim`.

2. **Collection**: After parsing, `collect_heredocs_ast()` walks the entire AST looking for commands with `heredoc_delim` set. For each one:
   - Creates a `pipe()`
   - Prompts the user for lines (displaying `heredoc>`)
   - Writes each line to the pipe's write end
   - Stops when the user types the delimiter
   - Closes the write end
   - Stores the read end as `cmd->heredoc_fd`

3. **Execution**: When `setup_redirects()` runs in the child process, it does:
   ```c
   dup2(cmd->heredoc_fd, STDIN_FILENO);  // stdin ← pipe read end
   close(cmd->heredoc_fd);
   ```
   Now the command reads from the pipe, which contains the heredoc text.

### Why Pipes for Heredocs?

The heredoc content needs to be available as the command's stdin. A pipe is the natural mechanism: write the content to one end, let the command read from the other.

---

## 20. Command Substitution

### What is Command Substitution?

**Command substitution** runs a command and replaces it with its output:

```bash
echo "Today is $(date)"
# Runs date, captures its output, substitutes it inline
```

### How Shelli Implements It

1. **Detection**: During variable expansion in `expand.c`, when the expander encounters `$(`, it extracts the command string inside the parentheses.

2. **Callback**: The expander calls `cmd_subst_fn()`, which is set to `cmd_subst_callback()` in main.c. This avoids circular dependencies (expand.c doesn't need to know about the lexer/parser/executor).

3. **Full Pipeline**: The callback runs the complete pipeline on the inner command:
   ```c
   static char *cmd_subst_callback(const char *cmd_str) {
       lexer_tokenize(cmd_str, &tokens, ...);
       parser_parse_ast(&tokens, &ast, ...);
       executor_run_ast_capture(ast, output, 8192);
       return output;
   }
   ```

4. **Capture**: `executor_run_ast_capture()` captures stdout into a buffer by:
   - Creating a pipe
   - Forking a child
   - In the child: `dup2(pipe_write, STDOUT_FILENO)` then execute
   - In the parent: `read()` from the pipe read end into the buffer

5. **Substitution**: The captured output replaces the `$(...)` in the original word. Trailing newlines are stripped (standard shell behavior).

---

## 21. Multi-line Input & RC Files

### Multi-line Input

When you type an incomplete command (like `if test -f x; then` without `fi`), Shelli needs more input. This is handled with:

1. The parser returns `PARSE_INCOMPLETE`
2. Main.c sets `multiline = 1`
3. The next `tui_read_line()` appends to an accumulation buffer (`DynBuf accumulated`)
4. The combined input is re-tokenized and re-parsed
5. This continues until the command is complete or there's an error

Similarly, unclosed quotes cause the lexer to return an error, which also triggers multi-line accumulation.

### RC Files (~/.shellirc)

When Shelli starts, it checks for `~/.shellirc`:
```c
const char *home = getenv("HOME");
snprintf(rcpath, sizeof(rcpath), "%s/.shellirc", home);
if (access(rcpath, R_OK) == 0) {
    // Read each line, tokenize, parse, execute
}
```

This lets users define functions, set variables, and configure the shell on startup. Each line goes through the full lex→parse→execute pipeline.

The `source` / `.` builtin does the same thing for any file.

---

## 22. History & Smart Suggestions

### Command History

Shelli maintains a persistent command history:
- `tui_history_load()` reads history from disk on startup
- `tui_history_save()` writes history to disk on exit
- Up/Down arrow keys navigate through history
- `!!` expands to the last command
- `!n` expands to command number n
- The `history` builtin prints all saved commands

### Smart Suggestions

Shelli tracks command usage frequency:
- `suggest_record_usage()` records each command entered
- `suggest_build_freq_table()` builds a frequency table from history
- Tab completion uses this data to rank suggestions
- More frequently used commands appear first

---

## 23. Memory Management in C

### Manual Memory Management

C does NOT have garbage collection. Every `malloc()` must have a matching `free()`, or you get a **memory leak**.

### Key Patterns in Shelli

**Ownership**: When one function creates memory, it "owns" it and is responsible for freeing it.

```c
// Parser creates commands - AST owns them
Command *cmd = command_new();     // malloc inside
AstNode *node = ast_new_command(cmd);  // AST takes ownership

// When done, ast_free() recursively frees everything
ast_free(ast);  // Frees all nodes, commands, strings, etc.
```

**Clone before modify**: The executor clones commands before expansion so the AST stays clean:
```c
Command *cmd = clone_cmd_for_exec(orig_cmd);  // Deep copy
expand_command(cmd);  // Modifies the copy, not the original
// ... use cmd ...
command_free(cmd);    // Free the copy
```

**DynBuf**: A dynamic buffer utility that grows automatically:
```c
DynBuf buf;
dynbuf_init(&buf);
dynbuf_push(&buf, 'h');          // Add single char
dynbuf_append_str(&buf, "ello"); // Add string
char *result = dynbuf_steal(&buf); // Get string, caller owns it
free(result);                       // Must free when done
```

---

## 24. The TUI (Terminal User Interface) - Overview

Shelli has a rich educational TUI that **visualizes each stage** of command processing. This is what makes it an educational shell.

### Architecture

The TUI is built from several modules in `src/tui/`:

| Module | Purpose |
|--------|---------|
| `tui_core.c` | Terminal raw mode, alternate screen buffer |
| `tui_input.c` | Line editing, cursor movement, tab completion, history |
| `tui_render.c` | Main drawing, screen layout |
| `tui_widgets.c` | Reusable components: boxes, progress bars |
| `tui_theme.c` | Catppuccin color palette |
| `tui_logo.c` | Animated splash screen |
| `tui_anim.c` | Easing functions, animations |
| `tui_icons.c` | Nerd Font icons with ASCII fallback |
| `tui_mascot.c` | Shell mascot character |
| `tui_suggest.c` | Smart command suggestions |
| `tui_pages.c` | Stage browser navigation |
| `tui_stage_*.c` | Individual stage display pages |

### Stage Browser

After each command, the TUI shows a **stage browser** where you can navigate between pages showing:
1. **Tokenization** - How the input was broken into tokens
2. **AST** - The tree structure the parser built
3. **Expansion** - What variables/globs were expanded
4. **Execution** - What system calls were made (fork, exec, pipe, dup2)
5. **Result** - Output and exit code

### Raw Mode

Normally, the terminal waits for Enter before sending input. **Raw mode** (set via `termios`) gives the shell character-by-character input, enabling:
- Arrow key navigation
- Tab completion
- Real-time rendering
- Ctrl+A/E/K shortcuts

---

## 25. The Complete Picture: End-to-End Walkthrough

Let's trace exactly what happens when you type `ls -la | grep foo > out.txt` in Shelli:

### Step 1: Input (main.c)
```
tui_read_line() returns "ls -la | grep foo > out.txt"
```

### Step 2: Tokenization (lexer.c)
```
lexer_tokenize() scans character by character:
  'l','s'       → accumulate word → WORD "ls"
  ' '           → whitespace, emit word
  '-','l','a'   → accumulate word → WORD "-la"
  ' '           → whitespace
  '|'           → PIPE
  ' '           → whitespace
  'g','r','e','p' → WORD "grep"
  ' '           → whitespace
  'f','o','o'   → WORD "foo"
  ' '           → whitespace
  '>'           → REDIR_OUT
  ' '           → whitespace
  'o','u','t',... → WORD "out.txt"
  '\0'          → EOF
```

Result: `[WORD:ls] [WORD:-la] [PIPE] [WORD:grep] [WORD:foo] [REDIR_OUT] [WORD:out.txt] [EOF]`

### Step 3: Parsing (parser.c)
```
parser_parse_ast() calls:
  parse_list() calls:
    parse_and_or() calls:
      parse_pipeline():
        parse_command() → parse_simple_command():
          reads WORD "ls", WORD "-la" → Command{argv=["ls","-la"]}
          → NODE_COMMAND
        sees PIPE, advances
        parse_command() → parse_simple_command():
          reads WORD "grep", WORD "foo" → Command{argv=["grep","foo"]}
          reads REDIR_OUT, WORD "out.txt" → redir_out={type=REDIR_OUT, filename="out.txt"}
          → NODE_COMMAND
        → NODE_PIPELINE with 2 commands
```

Result AST:
```
NODE_PIPELINE (negated=0)
├── cmds[0]: NODE_COMMAND argv=["ls", "-la"]
└── cmds[1]: NODE_COMMAND argv=["grep", "foo"] redir_out="out.txt"
```

### Step 4: Expansion (expand.c)
No variables, tildes, or globs in this command - expansion is a no-op.

### Step 5: Execution (executor.c)
```
execute_node(pipeline) → execute_pipeline_node():
  cmd_count = 2, so multi-command pipeline path

  1. pipe(pipes[0])              ← Create pipe
     pipes[0][0] = read end
     pipes[0][1] = write end

  2. fork() → child 1 (ls):
     dup2(pipes[0][1], STDOUT)   ← stdout → pipe write
     close(pipes[0][0])          ← close read end
     close(pipes[0][1])          ← close original
     execvp("ls", ["ls","-la"])  ← become ls

  3. fork() → child 2 (grep):
     dup2(pipes[0][0], STDIN)    ← stdin ← pipe read
     close(pipes[0][0])          ← close original
     close(pipes[0][1])          ← close write end
     open("out.txt", O_WRONLY|O_CREAT|O_TRUNC, 0644)  ← output redirect
     dup2(fd, STDOUT)            ← stdout → file
     close(fd)
     execvp("grep", ["grep","foo"])  ← become grep

  4. Parent closes pipe ends
  5. waitpid(child1, ...)
  6. waitpid(child2, ...)        ← last child's exit code is the result
```

### Step 6: Result Display (TUI)
```
tui_stage_browser() shows the result in the stage browser:
  - Input: "ls -la | grep foo > out.txt"
  - Tokens: [ls][-la][|][grep][foo][>][out.txt]
  - AST: Pipeline with 2 commands
  - Execution log: pipe(), fork()→pid 123, fork()→pid 124, waitpid...
  - Output: (captured from grep's stdout)
  - Exit code: 0
```

---

## 26. What Changed: Flat Pipeline vs. Full AST

### The Old Architecture (Mid-term)

In the previous version, the parser produced a flat `Pipeline` structure:
- A `Pipeline` was a linked list of `Command` structs
- Commands were connected by pipes
- No support for `if`/`while`/`for`/functions/`&&`/`||`

```
Old: "ls -la | grep foo"

Pipeline {
    cmd[0]: Command { argv=["ls", "-la"], next → cmd[1] }
    cmd[1]: Command { argv=["grep", "foo"], next → NULL }
    cmd_count: 2
}
```

This was simple but limited. You couldn't represent:
- `cmd1 && cmd2` (conditional execution)
- `if ... then ... fi` (control flow)
- `for x in ...; do ...; done` (loops)
- `greet() { ... }` (functions)

### The New Architecture (Current)

Now the parser produces a **tree** (AST) with 10 node types. The same pipeline becomes:

```
New: "ls -la | grep foo"

NODE_PIPELINE {
    cmds: [
        NODE_COMMAND { cmd: {argv=["ls", "-la"]} },
        NODE_COMMAND { cmd: {argv=["grep", "foo"]} }
    ],
    cmd_count: 2,
    negated: 0
}
```

And complex commands that were impossible before now have proper representations:

```
"if test -f x; then echo yes && echo done; else echo no; fi"

NODE_IF {
    condition: NODE_COMMAND {argv=["test", "-f", "x"]}
    then_body: NODE_LIST {
        entries: [
            {pipeline: NODE_COMMAND {argv=["echo", "yes"]}, sep: AND},
            {pipeline: NODE_COMMAND {argv=["echo", "done"]}, sep: NONE}
        ]
    }
    else_body: NODE_COMMAND {argv=["echo", "no"]}
}
```

### Summary of Changes

| Feature | Old (Mid-term) | New (Current) |
|---------|---------------|---------------|
| Data structure | Flat Pipeline (linked list) | AST (tree) |
| Parsing | Linear token scan | Recursive descent parser |
| Pipe support | Yes | Yes |
| `;` chaining | No | Yes (NODE_LIST) |
| `&&` / `\|\|` | No | Yes (LIST_SEP_AND/OR) |
| `if/elif/else/fi` | No | Yes (NODE_IF) |
| `while/until` | No | Yes (NODE_WHILE/UNTIL) |
| `for` loops | No | Yes (NODE_FOR) |
| Functions | No | Yes (NODE_FUNCTION_DEF) |
| `!` negation | No | Yes (NODE_NOT) |
| Subshells | No | Yes (NODE_SUBSHELL) |
| `&` background | No | Yes (LIST_SEP_BG) |
| Variable expansion | Basic | Full ($VAR, ${VAR}, $?, $$, $#, $@, $1-$9) |
| Arithmetic | No | Yes ($(( ))) |
| Command substitution | No | Yes ($( )) |
| Glob expansion | No | Yes (*, ?, []) |
| Tilde expansion | No | Yes (~) |
| Variable scoping | No | Yes (scope stack) |
| Shell functions | No | Yes (function table + AST clone) |
| Job control | No | Yes (job table, fg, bg) |
| Heredoc | No | Yes (<< DELIM) |
| Multi-line input | No | Yes (PARSE_INCOMPLETE) |
| test/[ builtin | No | Yes (file tests, string ops, numeric ops) |
| History | Basic | Persistent + !! and !n expansion |
| Tab completion | No | Yes (with frequency ranking) |

---

## 27. Quick Reference Card

```
┌──────────────────────────────────────────────────────────────┐
│                SHELLI EXECUTION FLOW                          │
│                                                               │
│  Input ──► Lexer ──► Parser ──► AST ──► Expand ──► Execute   │
│  "ls|wc"   tokens    tree      tree    $VAR→val  fork/exec   │
├──────────────────────────────────────────────────────────────┤
│                KEY SYSTEM CALLS                               │
│                                                               │
│  fork()     Create child process (copy of parent)             │
│  execvp()   Replace process with new program (PATH search)    │
│  waitpid()  Wait for child process to finish                  │
│  pipe()     Create one-way communication channel              │
│  dup2()     Redirect file descriptor (make fd point elsewhere)│
│  open()     Open file for reading/writing                     │
│  close()    Close a file descriptor                           │
│  kill()     Send signal to a process                          │
│  signal()   Set up signal handler                             │
│  sigaction() Set up signal handler (more control)             │
│  chdir()    Change current directory                          │
│  getcwd()   Get current directory                             │
│  getpid()   Get current process ID                            │
│  setenv()   Set environment variable                          │
│  getenv()   Get environment variable                          │
│  read()     Read bytes from file descriptor                   │
│  write()    Write bytes to file descriptor                    │
│  stat()     Get file information (type, size, permissions)    │
│  access()   Check file permissions                            │
│  glob()     Expand filename patterns (*.c)                    │
├──────────────────────────────────────────────────────────────┤
│                FILE DESCRIPTORS                               │
│                                                               │
│  0 = stdin   (input)    ← Keyboard by default                 │
│  1 = stdout  (output)   → Screen by default                   │
│  2 = stderr  (errors)   → Screen by default                   │
├──────────────────────────────────────────────────────────────┤
│                PROCESS LIFECYCLE                               │
│                                                               │
│  Parent ──fork()──► Child                                     │
│     │                 │                                       │
│     │              exec() replaces child with program         │
│     │                 │                                       │
│  waitpid() ◄──────  exit()                                    │
│     │                                                         │
│  continues                                                    │
├──────────────────────────────────────────────────────────────┤
│                AST NODE TYPES                                 │
│                                                               │
│  NODE_COMMAND       ls -la > out.txt                          │
│  NODE_PIPELINE      cmd1 | cmd2 | cmd3                        │
│  NODE_LIST          cmd1 ; cmd2 && cmd3 || cmd4               │
│  NODE_IF            if cond; then body; elif...; else; fi     │
│  NODE_WHILE         while cond; do body; done                 │
│  NODE_UNTIL         until cond; do body; done                 │
│  NODE_FOR           for var in words; do body; done           │
│  NODE_FUNCTION_DEF  name() { body; }                          │
│  NODE_NOT           ! command                                 │
│  NODE_SUBSHELL      ( commands )                              │
├──────────────────────────────────────────────────────────────┤
│                EXPANSION ORDER                                │
│                                                               │
│  1. Tilde:       ~ → $HOME                                    │
│  2. Variables:   $VAR → value                                 │
│  3. Arithmetic:  $((2+3)) → 5                                │
│  4. Cmd subst:   $(cmd) → output                             │
│  5. Glob:        *.c → matching files                        │
├──────────────────────────────────────────────────────────────┤
│                SPECIAL VARIABLES                              │
│                                                               │
│  $?   Exit code of last command                               │
│  $$   Current process ID                                      │
│  $#   Number of positional parameters                         │
│  $@   All positional parameters                               │
│  $1   First argument (function/script)                        │
└──────────────────────────────────────────────────────────────┘
```

---

*This guide covers every OS concept used in Shelli. Read this and you'll understand the complete architecture from operating system fundamentals through to the implementation.*
