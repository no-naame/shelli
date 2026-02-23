# Operating System Concepts & Shell Theory

> **Understanding what a shell really is and how it works**

---

## Table of Contents
1. [What is an Operating System?](#1-what-is-an-operating-system)
2. [What is a Shell?](#2-what-is-a-shell)
3. [Processes - The Heart of Everything](#3-processes---the-heart-of-everything)
4. [How Commands Actually Execute](#4-how-commands-actually-execute)
5. [Tokenization - Breaking Down Input](#5-tokenization---breaking-down-input)
6. [Parsing - Understanding Structure](#6-parsing---understanding-structure)
7. [Execution - Making Things Happen](#7-execution---making-things-happen)
8. [Pipes - Connecting Processes](#8-pipes---connecting-processes)
9. [Redirection - Controlling I/O](#9-redirection---controlling-io)
10. [The Complete Picture](#10-the-complete-picture)

---

## 1. What is an Operating System?

### The Simple Explanation

Think of your computer like a restaurant:
- **Hardware** = Kitchen equipment (stove, fridge, utensils)
- **Operating System** = Restaurant manager
- **Applications** = Chefs cooking different dishes
- **You** = Customer ordering food

The OS is the **manager** that:
- Decides which chef (program) gets to use the stove (CPU) and when
- Makes sure chefs don't fight over the same fridge space (memory)
- Handles customer requests (your commands)
- Keeps everything organized

### What the OS Actually Does

```
┌─────────────────────────────────────────────────────────┐
│                    YOU (User)                           │
│                        ↓                                │
│              ┌─────────────────┐                        │
│              │     SHELL       │  ← You talk to this    │
│              └────────┬────────┘                        │
│                       ↓                                 │
│         ┌─────────────────────────┐                     │
│         │   OPERATING SYSTEM      │                     │
│         │  (Kernel)               │                     │
│         │  • Process Management   │                     │
│         │  • Memory Management    │                     │
│         │  • File System          │                     │
│         │  • Device Drivers       │                     │
│         └───────────┬─────────────┘                     │
│                     ↓                                   │
│    ┌────────────────────────────────────┐               │
│    │          HARDWARE                  │               │
│    │   CPU  •  RAM  •  Disk  •  Screen  │               │
│    └────────────────────────────────────┘               │
└─────────────────────────────────────────────────────────┘
```

### The Kernel

The **kernel** is the core of the OS that:
- Runs with full hardware access (privileged mode)
- Manages all system resources
- Provides **system calls** for programs to request services

**System calls** are how programs ask the OS to do things:
```
Program: "Hey OS, I need to read a file"
         ↓
    read() system call
         ↓
Kernel: Accesses disk, returns data
         ↓
Program: Gets the file contents
```

---

## 2. What is a Shell?

### Definition

A **shell** is a program that:
1. Takes commands from the user (text input)
2. Interprets what the user wants
3. Asks the OS to execute those commands
4. Shows the results

**It's called a "shell"** because it's the outer layer that wraps around the OS kernel - you interact with the shell, which then talks to the kernel.

### Types of Shells

```
┌──────────────────────────────────────────────────────┐
│  GRAPHICAL SHELL (GUI)                               │
│  • Windows Explorer                                  │
│  • macOS Finder                                      │
│  • Click icons, drag files                           │
└──────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────┐
│  COMMAND-LINE SHELL (CLI)                            │
│  • bash (Bourne Again Shell) - most common on Linux  │
│  • zsh (Z Shell) - default on macOS                  │
│  • sh (Bourne Shell) - original Unix shell           │
│  • fish, tcsh, ksh, etc.                             │
│  • shelli - our educational shell!                   │
└──────────────────────────────────────────────────────┘
```

### Why Use Command-Line Shells?

| Advantage | Explanation |
|-----------|-------------|
| **Speed** | Type `rm *.tmp` faster than clicking 100 files |
| **Automation** | Write scripts to repeat tasks |
| **Remote Access** | SSH into servers (no GUI available) |
| **Power** | Combine commands in ways GUIs can't |
| **Resources** | Uses almost no memory/CPU |

### The REPL Loop

Every shell runs a **REPL** (Read-Eval-Print Loop):

```
┌─────────────────────────────────────────┐
│                                         │
│  ┌─────────┐                            │
│  │  READ   │  ← Wait for user input     │
│  └────┬────┘                            │
│       ↓                                 │
│  ┌─────────┐                            │
│  │  EVAL   │  ← Parse and execute       │
│  └────┬────┘                            │
│       ↓                                 │
│  ┌─────────┐                            │
│  │  PRINT  │  ← Show result             │
│  └────┬────┘                            │
│       ↓                                 │
│  ┌─────────┐                            │
│  │  LOOP   │  ← Go back to READ         │
│  └─────────┘                            │
│                                         │
└─────────────────────────────────────────┘
```

In our shell (simplified):
```c
while (1) {
    char *input = read_line();           // READ
    Pipeline *cmd = parse(tokenize(input)); // EVAL (part 1)
    int result = execute(cmd);           // EVAL (part 2)
    show_result(result);                 // PRINT
    // automatically loops                // LOOP
}
```

---

## 3. Processes - The Heart of Everything

### What is a Process?

A **process** is a running program. When you run `ls`, the OS creates a process.

**Program vs Process**:
- **Program**: Code sitting on disk (like a recipe in a cookbook)
- **Process**: Program actually running (like a chef cooking that recipe)

You can run the same program multiple times → multiple processes.

### Process Anatomy

Each process has:

```
┌─────────────────────────────────────────────────────┐
│                    PROCESS                          │
├─────────────────────────────────────────────────────┤
│  PID: 1234                   (Process ID - unique)  │
│  PPID: 5678                  (Parent Process ID)    │
│                                                     │
│  ┌─────────────────────────────────────────────┐    │
│  │              MEMORY SPACE                   │    │
│  │  ┌─────────┐  ┌─────────┐  ┌─────────────┐  │    │
│  │  │  Code   │  │  Data   │  │    Stack    │  │    │
│  │  │ (text)  │  │ (vars)  │  │ (functions) │  │    │
│  │  └─────────┘  └─────────┘  └─────────────┘  │    │
│  └─────────────────────────────────────────────┘    │
│                                                     │
│  ┌─────────────────────────────────────────────┐    │
│  │          FILE DESCRIPTORS                   │    │
│  │  0 = stdin  (keyboard input)                │    │
│  │  1 = stdout (screen output)                 │    │
│  │  2 = stderr (error output)                  │    │
│  │  3, 4, ... (open files)                     │    │
│  └─────────────────────────────────────────────┘    │
│                                                     │
│  Environment Variables: PATH, HOME, USER, etc.      │
│  Current Directory: /home/user/projects             │
└─────────────────────────────────────────────────────┘
```

### File Descriptors - Super Important!

Every process has **file descriptors** (small numbers) that point to I/O streams:

| FD | Name | Default | Purpose |
|----|------|---------|---------|
| 0 | stdin | Keyboard | Input |
| 1 | stdout | Screen | Normal output |
| 2 | stderr | Screen | Error output |

This is how piping and redirection work - we change what these FDs point to!

### The Process Tree

All processes form a tree structure:

```
                    init (PID 1)
                         │
         ┌───────────────┼───────────────┐
         │               │               │
      systemd        login            sshd
         │               │               │
    ┌────┴────┐       terminal        session
    │         │          │               │
  nginx    postgres    bash           bash
                         │               │
                    ┌────┴────┐         vim
                    │         │
                   ls      shelli
                              │
                           ┌──┴──┐
                          ls   grep
```

Every process (except init/PID 1) has a **parent**.

---

## 4. How Commands Actually Execute

### The fork() + exec() Pattern

This is **THE** most important concept for understanding shells.

When you type `ls` in a shell:

```
┌──────────────────────────────────────────────────────────┐
│  BEFORE fork()                                           │
│                                                          │
│  ┌────────────────────┐                                  │
│  │   SHELL (bash)     │                                  │
│  │   PID: 1000        │                                  │
│  │   Running...       │                                  │
│  └────────────────────┘                                  │
└──────────────────────────────────────────────────────────┘
                         │
                    fork()
                         │
                         ▼
┌──────────────────────────────────────────────────────────┐
│  AFTER fork() - TWO identical processes!                 │
│                                                          │
│  ┌────────────────────┐    ┌────────────────────┐        │
│  │   SHELL (parent)   │    │   SHELL (child)    │        │
│  │   PID: 1000        │    │   PID: 1001        │        │
│  │   fork() returned  │    │   fork() returned  │        │
│  │   1001 (child PID) │    │   0 (I'm child)    │        │
│  └────────────────────┘    └────────────────────┘        │
└──────────────────────────────────────────────────────────┘
                                      │
                                 exec("ls")
                                      │
                                      ▼
┌──────────────────────────────────────────────────────────┐
│  AFTER exec() - Child is now ls!                         │
│                                                          │
│  ┌────────────────────┐    ┌────────────────────┐        │
│  │   SHELL (parent)   │    │   ls               │        │
│  │   PID: 1000        │    │   PID: 1001        │        │
│  │   Waiting...       │    │   Running...       │        │
│  └────────────────────┘    └────────────────────┘        │
└──────────────────────────────────────────────────────────┘
                                      │
                                   exits
                                      │
                                      ▼
┌──────────────────────────────────────────────────────────┐
│  AFTER child exits                                       │
│                                                          │
│  ┌────────────────────┐                                  │
│  │   SHELL (parent)   │    Child is gone,                │
│  │   PID: 1000        │    shell continues               │
│  │   wait() returns   │                                  │
│  └────────────────────┘                                  │
└──────────────────────────────────────────────────────────┘
```

### Why fork() + exec() instead of just "run"?

1. **Safety**: Child is a copy - if it crashes, parent (shell) survives
2. **Setup**: Between fork and exec, we can redirect I/O, set up pipes
3. **Control**: Parent can wait, or run child in background
4. **Unix Philosophy**: Small, composable tools

### The Code

```c
pid_t pid = fork();  // Create child process

if (pid == 0) {
    // CHILD PROCESS
    // This code runs in the child only

    // Setup (redirect I/O, etc.)
    dup2(pipe_fd[1], STDOUT_FILENO);  // Redirect stdout to pipe

    // Replace myself with the actual program
    execvp("ls", ["ls", "-la", NULL]);

    // If exec fails, we get here
    perror("exec failed");
    exit(1);
}

// PARENT PROCESS (pid > 0)
// This code runs in the parent only

waitpid(pid, &status, 0);  // Wait for child to finish
// Now child is done, continue with shell
```

### Why Doesn't exec() Return?

`exec()` **replaces** the current process with a new program:
- Same PID
- Same file descriptors
- But completely new code, data, stack

If exec succeeds, the old code is gone - there's nothing to return to!

```
Before exec():              After exec():
┌────────────────────┐      ┌────────────────────┐
│  fork'd shell copy │  →   │  ls program        │
│  Code: shell code  │      │  Code: ls code     │
│  Data: shell data  │      │  Data: ls data     │
│  PID: 1001         │      │  PID: 1001 (same!) │
└────────────────────┘      └────────────────────┘
```

---

## 5. Tokenization - Breaking Down Input

### What is Tokenization?

**Tokenization** (or lexical analysis) is breaking a string into meaningful pieces.

It's like how you read a sentence:
- You don't see "Thequickbrownfox"
- You see "The | quick | brown | fox" (separate words)

### How the Lexer Works

Input: `ls -la | grep "hello world" > output.txt`

```
Character stream:
l s   - l a   |   g r e p   " h e l l o   w o r l d "   >   o u t p u t . t x t
│                                                                              │
└──────────────────────────────────────────────────────────────────────────────┘
                                    │
                              TOKENIZER
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│  Token 1: [WORD]     value="ls"                                             │
│  Token 2: [WORD]     value="-la"                                            │
│  Token 3: [PIPE]     value="|"                                              │
│  Token 4: [WORD]     value="grep"                                           │
│  Token 5: [WORD]     value="hello world"  ← Quotes removed, space preserved │
│  Token 6: [REDIR_OUT] value=">"                                             │
│  Token 7: [WORD]     value="output.txt"                                     │
│  Token 8: [EOF]      (end of input)                                         │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Tokenizer State Machine

The tokenizer scans character by character:

```
         ┌─────────────────────────────────────────┐
         │                                         │
         ▼                                         │
    ┌─────────┐    whitespace    ┌─────────────┐   │
    │  START  │ ───────────────► │ (skip them) │ ──┘
    └────┬────┘                  └─────────────┘
         │
         ├── letter/number ──────► [WORD state]
         │                         Keep reading until whitespace/special
         │
         ├── " or ' ──────────────► [QUOTE state]
         │                          Read until closing quote
         │
         ├── | ───────────────────► Emit [PIPE] token
         │
         ├── > ───────────────────► Check for >> or just >
         │                          Emit [REDIR_OUT] or [REDIR_APPEND]
         │
         └── < ───────────────────► Emit [REDIR_IN] token
```

### Why Tokenize?

Makes parsing easier:
- Parser doesn't deal with whitespace
- Parser doesn't deal with quotes
- Parser works with clean, structured tokens

---

## 6. Parsing - Understanding Structure

### What is Parsing?

**Parsing** (or syntax analysis) is understanding the STRUCTURE of the input.

Tokenizer: "Here are the words"
Parser: "Here's what they MEAN together"

### From Tokens to Pipeline

Input tokens:
```
[WORD:ls] [WORD:-la] [PIPE] [WORD:grep] [WORD:foo] [REDIR_OUT] [WORD:out.txt] [EOF]
```

Output structure:
```
Pipeline
├── Command 1
│   ├── argv: ["ls", "-la"]
│   ├── argc: 2
│   ├── redir_in: (none)
│   ├── redir_out: (none)
│   └── next: → Command 2
│
└── Command 2
    ├── argv: ["grep", "foo"]
    ├── argc: 2
    ├── redir_in: (none)
    ├── redir_out: "out.txt"
    └── next: NULL (end of pipeline)
```

### Parser Grammar (Simplified)

What our parser understands:

```
pipeline  = command ( "|" command )*

command   = word+ redirect*

redirect  = "<" filename
          | ">" filename
          | ">>" filename

word      = [any word token]
filename  = [any word token]
```

Translation:
- A **pipeline** is one or more commands separated by pipes
- A **command** is one or more words, optionally followed by redirects
- A **redirect** is <, >, or >> followed by a filename

### What Parser Detects as Errors

```bash
|                     # Error: pipe at start, no command
ls |                  # Error: pipe at end, nothing after
ls > > file           # Error: two redirects, no filename
ls >                  # Error: redirect without filename
```

### Parser vs Interpreter

Our parser only understands **structure**, not **meaning**:
- ✅ `ls -la | grep foo` → Valid structure
- ✅ `asdfgh qwerty` → Valid structure (two words)
- ✅ `nonexistent_command arg` → Valid structure

The parser doesn't know or care if `asdfgh` is a real command.
The **executor** will fail later when trying to run it.

---

## 7. Execution - Making Things Happen

### Single Command Execution

For `ls -la`:

```
┌─────────────────────────────────────────────────────────────────┐
│ 1. Shell calls fork()                                           │
│    └── Creates child process (copy of shell)                    │
│                                                                  │
│ 2. Child process:                                                │
│    a. Set up redirects (if any)                                 │
│    b. Call execvp("ls", ["ls", "-la", NULL])                    │
│    c. Kernel loads /bin/ls program                              │
│    d. ls runs, prints to stdout                                 │
│    e. ls exits with status code                                 │
│                                                                  │
│ 3. Parent (shell):                                               │
│    a. Calls waitpid() - blocks until child done                 │
│    b. Gets exit status                                          │
│    c. Continues with next command                               │
└─────────────────────────────────────────────────────────────────┘
```

### The execvp() Function

```c
execvp("ls", argv);
//      │      │
//      │      └── Array: ["ls", "-la", NULL]
//      │                                 │
//      │                       Must end with NULL!
//      │
//      └── Program name (searches in PATH)
```

What execvp does:
1. Searches PATH for "ls" → finds `/bin/ls`
2. Loads `/bin/ls` binary into memory
3. Replaces current process entirely
4. Starts executing from ls's main()

### Exit Codes

Every process returns an exit code (0-255):
- **0** = Success
- **Non-zero** = Failure (different values for different errors)
- **127** = Command not found (convention)
- **128+N** = Killed by signal N

```bash
$ ls
file.txt
$ echo $?    # $? is last exit code
0

$ ls /nonexistent
ls: /nonexistent: No such file or directory
$ echo $?
1
```

---

## 8. Pipes - Connecting Processes

### What is a Pipe?

A **pipe** is a one-way communication channel between processes.

```
┌──────────────┐     PIPE      ┌──────────────┐
│   Process A  │ ───────────►  │   Process B  │
│  (producer)  │    bytes      │  (consumer)  │
│              │    flow       │              │
│  stdout ─────┼───────────────┼─────► stdin  │
└──────────────┘               └──────────────┘
```

### How Pipes Work

The pipe() system call creates two file descriptors:

```c
int pipe_fd[2];
pipe(pipe_fd);

// pipe_fd[0] = READ end (like a faucet)
// pipe_fd[1] = WRITE end (like a hose)
```

```
                    ┌─────────────────────┐
   write() ──────►  │        PIPE         │  ──────► read()
                    │  (kernel buffer)    │
   pipe_fd[1]       └─────────────────────┘       pipe_fd[0]
```

### Connecting Two Commands with a Pipe

For `ls | grep foo`:

```
┌───────────────────────────────────────────────────────────────────┐
│  STEP 1: Create pipe                                              │
│                                                                   │
│           ┌─────────────────────┐                                 │
│           │        PIPE         │                                 │
│           │  pipe_fd[1]  [0]    │                                 │
│           └─────────────────────┘                                 │
└───────────────────────────────────────────────────────────────────┘

┌───────────────────────────────────────────────────────────────────┐
│  STEP 2: Fork child 1 (ls)                                        │
│                                                                   │
│  ┌─────────┐          ┌───────┐                                   │
│  │   ls    │  stdout ─┼──►[1] │                                   │
│  │  child  │  dup2()  │ PIPE  │                                   │
│  └─────────┘          └───────┘                                   │
│                                                                   │
│  Child 1's stdout now points to pipe write end                    │
└───────────────────────────────────────────────────────────────────┘

┌───────────────────────────────────────────────────────────────────┐
│  STEP 3: Fork child 2 (grep)                                      │
│                                                                   │
│          ┌───────┐          ┌───────────┐                         │
│          │ PIPE  │ [0]──────┼─► stdin   │                         │
│          │       │  dup2()  │   grep    │                         │
│          └───────┘          │   child   │                         │
│                             └───────────┘                         │
│                                                                   │
│  Child 2's stdin now points to pipe read end                      │
└───────────────────────────────────────────────────────────────────┘

┌───────────────────────────────────────────────────────────────────┐
│  STEP 4: Close unused pipe ends, exec, run                        │
│                                                                   │
│  ┌─────────┐    bytes flow    ┌───────────┐                       │
│  │   ls    │ ───────────────► │   grep    │                       │
│  │  output │                  │   input   │                       │
│  └─────────┘                  └───────────┘                       │
│                                                                   │
│  ls prints file names → pipe → grep reads and filters             │
└───────────────────────────────────────────────────────────────────┘
```

### Why Close Unused Pipe Ends?

This is crucial and often confusing:

```c
// In child 1 (ls - the writer):
close(pipe_fd[0]);  // Close read end - we don't need it
dup2(pipe_fd[1], STDOUT_FILENO);  // stdout → pipe write
close(pipe_fd[1]);  // Close original (dup2 made a copy)

// In child 2 (grep - the reader):
close(pipe_fd[1]);  // Close write end - we don't need it
dup2(pipe_fd[0], STDIN_FILENO);  // stdin ← pipe read
close(pipe_fd[0]);  // Close original

// In parent:
close(pipe_fd[0]);  // Parent doesn't use pipe
close(pipe_fd[1]);
```

Why?
- If write end isn't closed everywhere, read() never gets EOF
- grep would wait forever thinking more data might come

### The dup2() Function

```c
dup2(old_fd, new_fd);
```

Makes `new_fd` point to the same thing as `old_fd`:

```
Before dup2(pipe_fd[1], 1):
  FD 1 (stdout) → Screen
  pipe_fd[1]    → Pipe write end

After dup2(pipe_fd[1], 1):
  FD 1 (stdout) → Pipe write end  ← Changed!
  pipe_fd[1]    → Pipe write end

Now printf() goes to the pipe instead of screen!
```

---

## 9. Redirection - Controlling I/O

### What is Redirection?

Changing where stdin/stdout/stderr point to.

```bash
ls > file.txt      # stdout → file (overwrite)
ls >> file.txt     # stdout → file (append)
cat < file.txt     # stdin ← file
ls 2> errors.txt   # stderr → file
ls > out.txt 2>&1  # both stdout and stderr → file
```

### How Redirection Works

For `ls > output.txt`:

```
Normal (no redirect):
┌─────────┐
│   ls    │
│ stdout ─┼───────────────► Screen
└─────────┘

With > output.txt:
┌─────────┐
│   ls    │
│ stdout ─┼───────────────► output.txt (file)
└─────────┘
```

### Implementation

```c
// In child process, before exec():

// Open the file
int fd = open("output.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
//                          │          │         │         │
//                          │          │         │         └─ permissions (rw-r--r--)
//                          │          │         └─ truncate (empty file first)
//                          │          └─ create if doesn't exist
//                          └─ write only

// Make stdout point to this file
dup2(fd, STDOUT_FILENO);  // STDOUT_FILENO = 1

// Close original fd (dup2 made a copy)
close(fd);

// Now exec - ls will write to the file!
execvp("ls", argv);
```

### Input Redirection

For `sort < data.txt`:

```c
int fd = open("data.txt", O_RDONLY);
dup2(fd, STDIN_FILENO);  // STDIN_FILENO = 0
close(fd);
execvp("sort", argv);
// sort reads from data.txt instead of keyboard
```

---

## 10. The Complete Picture

### What Happens When You Type `ls -la | grep foo > out.txt`

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ STEP 1: INPUT                                                               │
│                                                                             │
│   You type: ls -la | grep foo > out.txt                                     │
│   Shell receives this as a string                                           │
└─────────────────────────────────────────────────────────────────────────────┘
                                      │
                                      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ STEP 2: TOKENIZATION (Lexer)                                                │
│                                                                             │
│   "ls -la | grep foo > out.txt"                                             │
│                 │                                                           │
│                 ▼                                                           │
│   [WORD:ls] [WORD:-la] [PIPE] [WORD:grep] [WORD:foo] [REDIR:>] [WORD:out]   │
└─────────────────────────────────────────────────────────────────────────────┘
                                      │
                                      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ STEP 3: PARSING                                                             │
│                                                                             │
│   Pipeline                                                                  │
│   ├── Command 1: argv=["ls", "-la"], redirect_out=NULL                      │
│   │      └── next ──────────────────────────────────┐                       │
│   └── Command 2: argv=["grep", "foo"], redirect_out="out.txt"               │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
                                      │
                                      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ STEP 4: EXECUTION SETUP                                                     │
│                                                                             │
│   Shell (parent) creates:                                                   │
│   - pipe() for connecting ls → grep                                         │
│   - Will fork two children                                                  │
└─────────────────────────────────────────────────────────────────────────────┘
                                      │
                                      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ STEP 5: FORK CHILD 1 (ls)                                                   │
│                                                                             │
│   fork() creates child                                                      │
│   Child:                                                                    │
│     - dup2(pipe_write, stdout)   # stdout → pipe                            │
│     - close unused pipe ends                                                │
│     - execvp("ls", ["ls", "-la", NULL])                                     │
│     - Child is now running /bin/ls                                          │
└─────────────────────────────────────────────────────────────────────────────┘
                                      │
                                      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ STEP 6: FORK CHILD 2 (grep)                                                 │
│                                                                             │
│   fork() creates child                                                      │
│   Child:                                                                    │
│     - dup2(pipe_read, stdin)     # stdin ← pipe                             │
│     - open("out.txt")                                                       │
│     - dup2(file_fd, stdout)      # stdout → file                            │
│     - close unused ends                                                     │
│     - execvp("grep", ["grep", "foo", NULL])                                 │
│     - Child is now running /bin/grep                                        │
└─────────────────────────────────────────────────────────────────────────────┘
                                      │
                                      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ STEP 7: EXECUTION                                                           │
│                                                                             │
│   ┌───────────┐                    ┌───────────┐                            │
│   │    ls     │ ════════════════►  │   grep    │ ═══════► out.txt           │
│   │           │    (via pipe)      │           │   (via redirect)           │
│   └───────────┘                    └───────────┘                            │
│                                                                             │
│   1. ls lists files, writes to pipe                                         │
│   2. grep reads from pipe, filters lines containing "foo"                   │
│   3. grep writes matches to out.txt                                         │
└─────────────────────────────────────────────────────────────────────────────┘
                                      │
                                      ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ STEP 8: CLEANUP                                                             │
│                                                                             │
│   - ls finishes, exits                                                      │
│   - Pipe write end closes (EOF sent to grep)                                │
│   - grep sees EOF, finishes processing, exits                               │
│   - Parent's waitpid() returns for both children                            │
│   - Shell shows result (exit code)                                          │
│   - Ready for next command                                                  │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Key OS Concepts We Used

| Concept | What It Is | How We Use It |
|---------|------------|---------------|
| **Process** | Running program instance | Each command runs in its own process |
| **fork()** | Create child process | Shell creates children to run commands |
| **exec()** | Replace process with new program | Child becomes the actual command |
| **wait()** | Parent waits for child | Shell waits before prompting again |
| **Pipe** | Inter-process communication | Connect stdout → stdin between commands |
| **File Descriptor** | Handle to file/stream | stdin=0, stdout=1, stderr=2 |
| **dup2()** | Redirect file descriptor | Make stdout go to file or pipe |
| **Signal** | Async notification | Ctrl+C sends SIGINT to stop command |

### What Makes a Shell a Shell?

1. **REPL** - Read-Eval-Print Loop
2. **Command Parsing** - Understanding what user wants
3. **Process Creation** - fork() + exec() pattern
4. **I/O Redirection** - Controlling stdin/stdout/stderr
5. **Pipelines** - Connecting commands together
6. **Built-in Commands** - cd, exit (must run in shell itself)
7. **Environment** - Variables, PATH, current directory
8. **Job Control** - Background processes (advanced shells)

### What Our Shell (Shelli) Does

| Feature | Implemented? | Notes |
|---------|--------------|-------|
| Basic commands | ✅ | ls, cat, echo, etc. |
| Arguments | ✅ | ls -la /path |
| Pipes | ✅ | cmd1 \| cmd2 \| cmd3 |
| Redirects | ✅ | >, <, >> |
| Quotes | ✅ | "hello world" |
| Built-ins | ✅ | cd, pwd, exit, help |
| Variables | ❌ | $HOME, $PATH not expanded |
| Globbing | ❌ | *.txt not expanded |
| && and \|\| | ❌ | Logical operators |
| Background | ❌ | & for background jobs |
| Job control | ❌ | fg, bg, Ctrl+Z |

### Why This Matters

Understanding shells means understanding:
- How your terminal works
- How programs communicate
- How the OS manages processes
- The Unix philosophy of small, composable tools
- Foundation for learning containers, servers, system programming

---

## Quick Reference

```
┌─────────────────────────────────────────────────────────────────────┐
│                    SHELL EXECUTION FLOW                             │
│                                                                     │
│  Input ──► Tokenize ──► Parse ──► Execute ──► Display               │
│  "ls|wc"   [ls][|][wc]  Pipeline  fork/exec   "3 files"             │
├─────────────────────────────────────────────────────────────────────┤
│                    KEY SYSTEM CALLS                                 │
│                                                                     │
│  fork()   - Create child process (copy of parent)                   │
│  exec()   - Replace current process with new program                │
│  wait()   - Parent waits for child to finish                        │
│  pipe()   - Create communication channel                            │
│  dup2()   - Redirect file descriptor                                │
│  open()   - Open file for reading/writing                           │
├─────────────────────────────────────────────────────────────────────┤
│                    FILE DESCRIPTORS                                 │
│                                                                     │
│  0 = stdin  (input)   ← Keyboard by default                         │
│  1 = stdout (output)  → Screen by default                           │
│  2 = stderr (errors)  → Screen by default                           │
├─────────────────────────────────────────────────────────────────────┤
│                    PROCESS RELATIONSHIPS                            │
│                                                                     │
│  Parent ──fork()──► Child                                           │
│     │                  │                                            │
│     │               exec()                                          │
│     │                  │                                            │
│   wait()◄─────────── exit()                                         │
└─────────────────────────────────────────────────────────────────────┘
```

---

*Good luck with your presentation! You now understand more about shells and OS than many CS students. 🚀*
