# C Language & Shelli Codebase Guide

> **For developers coming from Python/JavaScript**

---

## Table of Contents
1. [C vs Python/JavaScript - Quick Comparison](#1-c-vs-pythonjavascript---quick-comparison)
2. [How C Compilation Works](#2-how-c-compilation-works)
3. [Understanding .c and .h Files](#3-understanding-c-and-h-files)
4. [The Makefile Explained](#4-the-makefile-explained)
5. [C Syntax Crash Course](#5-c-syntax-crash-course)
6. [Shelli Codebase Structure](#6-shelli-codebase-structure)
7. [File-by-File Breakdown](#7-file-by-file-breakdown)
8. [Key Functions Reference](#8-key-functions-reference)

---

## 1. C vs Python/JavaScript - Quick Comparison

### The Big Differences

| Aspect | Python/JavaScript | C |
|--------|-------------------|---|
| **Execution** | Interpreted (runs line by line) | Compiled (converted to machine code first) |
| **Memory** | Automatic (garbage collected) | Manual (you manage memory yourself) |
| **Types** | Dynamic (`x = 5` then `x = "hello"`) | Static (`int x = 5` - x is always int) |
| **Speed** | Slower | Very fast (close to hardware) |
| **Syntax** | Flexible, forgiving | Strict, must follow rules exactly |

### Why C for a Shell?
- Shells need to be **fast** (you run thousands of commands)
- Shells need **direct access to OS** (system calls like `fork`, `exec`)
- C is what Unix/Linux is written in - perfect fit!

---

## 2. How C Compilation Works

### Python/JS Way (What You Know)
```
python script.py  →  Python interpreter reads and runs it
node script.js    →  Node.js interprets and runs it
```

### C Way (Compilation)
```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│  Source     │     │ Preprocessor│     │  Compiler   │     │   Linker    │
│  Code (.c)  │ ──► │  (#include) │ ──► │  (Assembly) │ ──► │  (Binary)   │
└─────────────┘     └─────────────┘     └─────────────┘     └─────────────┘
     main.c              ↓                    ↓                   ↓
                   Expands macros      Converts to          Combines all
                   Includes headers    machine code         into executable
                                                                  ↓
                                                             ./shelli
```

### Step-by-Step Compilation

**Step 1: Preprocessing** (`gcc -E`)
- Handles lines starting with `#` (like `#include`, `#define`)
- `#include <stdio.h>` → Copies entire stdio.h content into your file
- Think of it as "copy-paste" before compiling

**Step 2: Compilation** (`gcc -c`)
- Converts C code to assembly/machine code
- Creates `.o` files (object files) - binary but not runnable yet
- Each `.c` file becomes one `.o` file

**Step 3: Linking** (`gcc`)
- Combines all `.o` files into one executable
- Resolves references between files (e.g., main.c calling a function from lexer.c)
- Creates the final `./shelli` binary

### Try It Yourself
```bash
# See preprocessor output (huge!)
gcc -E src/main.c | head -100

# Compile to object file
gcc -c src/main.c -o main.o

# Link everything (what make does)
gcc main.o lexer.o parser.o ... -o shelli
```

---

## 3. Understanding .c and .h Files

### The Concept

Think of it like this (JavaScript analogy):
```javascript
// utils.js (the implementation - like .c file)
export function add(a, b) {
    return a + b;
}

// types.d.ts (TypeScript declaration - like .h file)
declare function add(a: number, b: number): number;
```

### .h Files (Header Files) = "The Menu"
- **Declarations** - tells WHAT exists
- Like a restaurant menu: lists dishes but doesn't contain recipes
- Contains: function signatures, type definitions, constants

```c
// executor.h - "The Menu"
// Says: "Hey, these functions exist. Here's how to call them."

int executor_run(Pipeline *pipeline);  // Declaration only
// Notice: no { } body, just a semicolon
```

### .c Files (Source Files) = "The Kitchen"
- **Definitions** - tells HOW it works
- Like the actual kitchen: contains the recipes
- Contains: actual function implementations

```c
// executor.c - "The Kitchen"
// The actual implementation

int executor_run(Pipeline *pipeline) {
    // Actual code here
    if (!pipeline) return 0;
    return execute_pipeline(pipeline);
}
```

### Why Separate Them?

1. **Compilation Speed**: Change one .c file → only recompile that file
2. **Organization**: Easy to see what a module offers (just read .h)
3. **Hiding Complexity**: Other files only see the "menu", not messy kitchen code

### The #include Mechanism
```c
// In main.c
#include "lexer.h"    // Your own headers (quotes)
#include <stdio.h>    // System headers (angle brackets)

// This literally copy-pastes lexer.h content here
// Now main.c knows about lexer's functions
```

---

## 4. The Makefile Explained

### What is Make?
Make is a build automation tool. Instead of typing:
```bash
gcc -c src/main.c -o build/main.o
gcc -c src/lexer.c -o build/lexer.o
gcc -c src/parser.c -o build/parser.o
# ... 10 more lines
gcc build/*.o -o shelli
```

You just type: `make`

### Anatomy of Our Makefile

```makefile
# ============ VARIABLES ============
CC = cc                          # Compiler to use (cc = system's C compiler)
CFLAGS = -Wall -Wextra -O2       # Compiler flags

# -Wall   = Show all warnings
# -Wextra = Show extra warnings
# -O2     = Optimize for speed

SRCDIR = src                     # Where source files live
TUIDIR = src/tui
OBJDIR = build                   # Where compiled .o files go

# List of source files
SOURCES = $(SRCDIR)/main.c \
          $(SRCDIR)/lexer.c \
          $(SRCDIR)/parser.c \
          ...

# List of object files (same names, but .o in build/)
OBJECTS = $(OBJDIR)/main.o \
          $(OBJDIR)/lexer.o \
          ...

# ============ RULES ============
# Format: target: dependencies
#             command

# Default target (first one)
all: $(TARGET)

# Link all object files into executable
$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^
#   $@ = target name (shelli)
#   $^ = all dependencies (all .o files)

# Compile each .c to .o
$(OBJDIR)/main.o: $(SRCDIR)/main.c
	$(CC) $(CFLAGS) -c -o $@ $<
#   -c = compile only, don't link
#   $< = first dependency (the .c file)

# Clean up
clean:
	rm -rf build shelli
```

### Make Commands
```bash
make          # Build everything
make clean    # Delete compiled files
make rebuild  # Clean + build (we defined this)
```

### Why Make is Smart
- Only recompiles files that **changed**
- Checks file timestamps
- If lexer.c changed but parser.c didn't → only recompile lexer.c

---

## 5. C Syntax Crash Course

### Variables & Types (vs Python)

```python
# Python - dynamic typing
x = 5           # x is int
x = "hello"     # now x is string (fine!)
```

```c
// C - static typing
int x = 5;           // x is int forever
char* x = "hello";   // ERROR! x is already int

// Common types
int count = 42;              // Integer (whole number)
float price = 19.99;         // Decimal number
char letter = 'A';           // Single character
char* name = "John";         // String (pointer to characters)
int arr[5] = {1,2,3,4,5};    // Array of 5 integers
```

### Functions

```python
# Python
def add(a, b):
    return a + b
```

```c
// C - must specify types for everything
int add(int a, int b) {
    return a + b;
}
// ↑ return type   ↑ parameter types
```

### Pointers (The Scary Part - Made Simple)

**What's a pointer?**
A variable that stores a memory address (location of another variable).

```c
int x = 42;         // x contains the value 42
int* p = &x;        // p contains the ADDRESS of x
                    // & means "address of"

printf("%d", x);    // Prints: 42
printf("%p", p);    // Prints: 0x7fff5a8 (memory address)
printf("%d", *p);   // Prints: 42
                    // * means "value at this address"
```

**Analogy**:
- `x` is like a house with a person inside
- `p` is like a piece of paper with the house's address written on it
- `&x` = "give me the address of this house"
- `*p` = "go to this address and tell me who's inside"

### Strings in C

```python
# Python - strings are easy
name = "John"
print(len(name))  # 4
```

```c
// C - strings are arrays of characters ending with \0
char name[] = "John";
// Actually stored as: ['J', 'o', 'h', 'n', '\0']
//                                        ↑ null terminator

// String functions need #include <string.h>
int length = strlen(name);        // 4
strcmp(name, "John");             // 0 if equal
strcpy(dest, src);                // Copy string
strcat(dest, src);                // Concatenate
```

### Structs (Like Python Classes, but Data Only)

```python
# Python
class Token:
    def __init__(self, type, value):
        self.type = type
        self.value = value

tok = Token("WORD", "hello")
print(tok.value)
```

```c
// C - struct is just data, no methods
typedef struct {
    int type;
    char* value;
} Token;

Token tok;                    // Create a Token
tok.type = TOKEN_WORD;
tok.value = "hello";

// Or with pointers
Token* tok_ptr = &tok;
tok_ptr->value;               // -> is shorthand for (*tok_ptr).value
```

### Memory Management

```python
# Python - automatic
my_list = [1, 2, 3]  # Memory allocated automatically
# When not used, garbage collector frees it
```

```c
// C - manual
int* arr = malloc(3 * sizeof(int));  // Allocate memory for 3 ints
arr[0] = 1;
arr[1] = 2;
arr[2] = 3;

free(arr);  // YOU must free it, or memory leaks!
```

### Control Flow (Same as Python/JS mostly)

```c
// If-else
if (x > 0) {
    printf("positive");
} else if (x < 0) {
    printf("negative");
} else {
    printf("zero");
}

// Loops
for (int i = 0; i < 10; i++) {
    printf("%d\n", i);
}

while (condition) {
    // do stuff
}

// Switch (like JS)
switch (token_type) {
    case TOKEN_WORD:
        handle_word();
        break;           // Must have break!
    case TOKEN_PIPE:
        handle_pipe();
        break;
    default:
        handle_other();
}
```

### Common Functions You'll See

```c
#include <stdio.h>    // Standard I/O
printf("Hello %s, you are %d years old\n", name, age);
//      %s = string, %d = integer, %f = float, %p = pointer

scanf("%d", &x);      // Read input into x

#include <stdlib.h>   // Standard library
malloc(size);         // Allocate memory
free(ptr);            // Free memory
exit(0);              // Exit program

#include <string.h>   // String functions
strlen(str);          // String length
strcmp(a, b);         // Compare strings (0 if equal)
strcpy(dest, src);    // Copy string

#include <unistd.h>   // Unix functions
fork();               // Create new process
exec();               // Replace process with new program
pipe();               // Create pipe for IPC
```

---

## 6. Shelli Codebase Structure

```
shelli/
├── Makefile              # Build configuration
├── src/
│   ├── main.c            # Entry point, REPL loop
│   ├── lexer.c/.h        # Tokenizer (text → tokens)
│   ├── parser.c/.h       # Parser (tokens → AST/Pipeline)
│   ├── executor.c/.h     # Executes commands (fork/exec)
│   ├── builtins.c/.h     # Built-in commands (cd, pwd, exit)
│   └── tui/              # Terminal User Interface
│       ├── tui.h         # TUI header (all declarations)
│       ├── tui_core.c    # Terminal setup, raw mode
│       ├── tui_input.c   # Line editing, history
│       ├── tui_render.c  # Drawing the UI
│       ├── tui_widgets.c # Reusable UI components
│       ├── tui_theme.c   # Colors and styling
│       ├── tui_logo.c    # Splash screen logo
│       ├── tui_anim.c    # Animations
│       └── tui_icons.c   # Nerd Font icons
└── build/                # Compiled .o files (generated)
```

### Data Flow Through the System

```
User types: "ls -la | grep foo"
              │
              ▼
┌─────────────────────────────┐
│          LEXER              │  Input: raw string
│   "ls -la | grep foo"       │  Output: list of tokens
│           ↓                 │
│  [WORD:ls] [WORD:-la]       │
│  [PIPE] [WORD:grep]         │
│  [WORD:foo] [EOF]           │
└─────────────────────────────┘
              │
              ▼
┌─────────────────────────────┐
│          PARSER             │  Input: tokens
│   Builds command structure  │  Output: Pipeline struct
│           ↓                 │
│  Pipeline {                 │
│    cmd[0]: ls -la           │
│    cmd[1]: grep foo         │
│    (connected by pipe)      │
│  }                          │
└─────────────────────────────┘
              │
              ▼
┌─────────────────────────────┐
│         EXECUTOR            │  Input: Pipeline
│   Actually runs commands    │  Output: exit code
│           ↓                 │
│  fork() → child: ls -la     │
│  fork() → child: grep foo   │
│  pipe connects them         │
│  wait for completion        │
└─────────────────────────────┘
              │
              ▼
         Shows result in TUI
```

---

## 7. File-by-File Breakdown

### main.c - The Entry Point

**Purpose**: Program starts here. Contains the REPL (Read-Eval-Print Loop).

**Key things it does**:
1. Initialize TUI (terminal setup)
2. Show splash screen
3. Loop forever:
   - Read user input
   - Tokenize it (lexer)
   - Parse it (parser)
   - Execute it (executor)
   - Show result
4. Cleanup on exit

**Key code sections**:
```c
int main(int argc, char *argv[]) {
    // 1. Setup
    tui_init();                    // Initialize terminal

    // 2. Main loop
    while (!should_exit) {
        char *line = tui_read_line();        // Get input
        lexer_tokenize(line, &tokens);       // Tokenize
        Pipeline *pipeline = parser_parse(&tokens, ...);  // Parse
        executor_run_capture(pipeline, output, ...);      // Execute
        tui_show_result(exit_code, output);  // Display
    }

    // 3. Cleanup
    tui_cleanup();
}
```

---

### lexer.c / lexer.h - The Tokenizer

**Purpose**: Break input string into meaningful pieces (tokens).

**Input**: `"ls -la | grep foo"`

**Output**:
```
[WORD: "ls"]
[WORD: "-la"]
[PIPE]
[WORD: "grep"]
[WORD: "foo"]
[EOF]
```

**Key concepts**:
- Scans character by character
- Identifies token types (WORD, PIPE, REDIRECT, etc.)
- Handles quoted strings ("hello world" is one token)

**Token types** (defined in lexer.h):
```c
typedef enum {
    TOKEN_WORD,      // Regular word: ls, -la, foo
    TOKEN_PIPE,      // |
    TOKEN_REDIR_IN,  // <
    TOKEN_REDIR_OUT, // >
    TOKEN_REDIR_APPEND, // >>
    TOKEN_EOF,       // End of input
} TokenType;
```

**Key function**:
```c
int lexer_tokenize(const char *input, TokenList *tokens);
// Returns 0 on success, -1 on error (like unclosed quote)
```

---

### parser.c / parser.h - The Parser

**Purpose**: Build a structured representation (AST) from tokens.

**Input**: Token list from lexer

**Output**: `Pipeline` structure (linked list of commands)

**Key structures**:
```c
// Single command: "ls -la"
typedef struct Command {
    char *argv[MAX_ARGS];      // ["ls", "-la", NULL]
    int argc;                  // 2
    Redirect redir_in;         // Input redirect (if any)
    Redirect redir_out;        // Output redirect (if any)
    struct Command *next;      // Next command in pipeline
} Command;

// Full pipeline: "ls | grep foo"
typedef struct Pipeline {
    Command *first;            // First command
    int cmd_count;             // Number of commands
} Pipeline;
```

**What parser handles**:
- Groups arguments with their commands
- Detects pipes (|) and creates linked list
- Handles redirects (>, <, >>)
- Reports syntax errors

---

### executor.c / executor.h - The Executor

**Purpose**: Actually run the commands using OS system calls.

**Key operations**:
1. `fork()` - Create child process
2. `exec()` - Replace child with actual program
3. `pipe()` - Create communication channel between processes
4. `wait()` - Parent waits for child to finish

**Key functions**:
```c
// Run a pipeline and capture output
int executor_run_capture(Pipeline *pipeline, char *output, int output_size);

// Internal: run single command
static int execute_single_capture(Command *cmd, char *output, int output_size);

// Internal: setup redirects (>, <)
static int setup_redirects(Command *cmd);
```

**How it handles pipelines**:
```
"ls | grep foo"

1. Create pipe: pipe_fd[0] (read), pipe_fd[1] (write)
2. Fork child 1 (ls):
   - Redirect stdout → pipe_fd[1]
   - exec("ls")
3. Fork child 2 (grep):
   - Redirect stdin ← pipe_fd[0]
   - exec("grep", "foo")
4. Parent waits for both to finish
```

---

### builtins.c / builtins.h - Built-in Commands

**Purpose**: Commands that must run in the shell itself (not forked).

**Why builtins?**
- `cd` must change the SHELL's directory, not a child's
- If we forked `cd`, only the child would change directory, then exit
- Shell would stay in original directory!

**Built-in commands**:
```c
cd [dir]     // Change directory
pwd          // Print working directory
exit [n]     // Exit shell with status
help         // Show help message
```

**Key function**:
```c
int builtin_execute(Command *cmd, int *should_exit);
// Returns exit code, sets should_exit if exit command
```

---

### TUI Files (src/tui/)

#### tui.h - The Header
All declarations for TUI functions. Other files include this to use TUI.

#### tui_core.c - Terminal Setup
- Puts terminal in "raw mode" (character-by-character input)
- Switches to alternate screen buffer (like vim does)
- Saves/restores terminal state

```c
tui_init()      // Setup terminal
tui_cleanup()   // Restore terminal
```

#### tui_input.c - Line Editor
- Handles keyboard input
- Arrow keys for cursor movement
- Up/Down for history
- Ctrl+A, Ctrl+E, etc. for editing

#### tui_render.c - Main Drawing
- Draws the entire UI frame
- Boxes, borders, panels
- Stage indicators
- Result display

Key functions:
```c
tui_draw_frame()           // Redraw entire screen
tui_show_tokens()          // Display tokenization
tui_show_pipeline()        // Display parsed AST
tui_show_result()          // Display execution result
```

#### tui_widgets.c - UI Components
Reusable pieces: boxes, progress bars, spinners, etc.

#### tui_theme.c - Colors
Catppuccin color palette, neon accents for aesthetics.

#### tui_logo.c - Splash Screen
The animated logo with gradient effect.

#### tui_anim.c - Animations
Easing functions, fade-in effects, typewriter animations.

#### tui_icons.c - Icons
Nerd Font icons with ASCII fallbacks for non-patched fonts.

---

## 8. Key Functions Reference

### Most Important Functions

| Function | File | Purpose |
|----------|------|---------|
| `main()` | main.c | Program entry, REPL loop |
| `lexer_tokenize()` | lexer.c | String → Tokens |
| `parser_parse()` | parser.c | Tokens → Pipeline |
| `executor_run_capture()` | executor.c | Pipeline → Execution |
| `builtin_execute()` | builtins.c | Run built-in commands |
| `tui_init()` | tui_core.c | Setup terminal |
| `tui_draw_frame()` | tui_render.c | Draw entire UI |
| `tui_read_line()` | tui_input.c | Read user input |

### System Calls Used (from OS)

| Function | Purpose |
|----------|---------|
| `fork()` | Create new process (copy of current) |
| `execvp()` | Replace process with new program |
| `pipe()` | Create pipe for inter-process communication |
| `dup2()` | Redirect file descriptors (stdin/stdout) |
| `waitpid()` | Wait for child process to finish |
| `open()` | Open file (for redirects) |
| `chdir()` | Change directory (for cd builtin) |
| `getcwd()` | Get current directory (for pwd) |

---

## Quick Reference Card

```
┌────────────────────────────────────────────────────────────┐
│                    C QUICK REFERENCE                       │
├────────────────────────────────────────────────────────────┤
│ Types:     int, char, float, double, void                  │
│ Pointers:  int* p = &x;    // p points to x                │
│            *p = 10;        // set value at p               │
│ Strings:   char* s = "hi"; // or char s[] = "hi";          │
│ Structs:   struct Name { int x; };                         │
│ Memory:    malloc(size), free(ptr)                         │
│ Printf:    %d=int, %s=string, %f=float, %p=pointer         │
├────────────────────────────────────────────────────────────┤
│                  COMPILATION                               │
├────────────────────────────────────────────────────────────┤
│ make           Build project                               │
│ make clean     Delete compiled files                       │
│ ./shelli       Run the shell                               │
├────────────────────────────────────────────────────────────┤
│                  SHELL PIPELINE                            │
├────────────────────────────────────────────────────────────┤
│ Input → Lexer → Parser → Executor → Result                 │
│ "ls"  → [WORD] → Pipeline → fork/exec → output             │
└────────────────────────────────────────────────────────────┘
```

---

*This guide should give you enough understanding to explain the codebase. The next document covers the OS concepts in depth.*
