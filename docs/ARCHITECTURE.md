# Shelli Architecture: Complete Technical Reference

This document traces every step that happens inside shelli from the moment
you press Enter to the moment output appears on screen.  It covers the
lexer, parser, AST, word expansion, executor, builtins, variable scoping,
function calls, job control, heredocs, the TUI, and every command type
shelli understands.

---

## Table of Contents

1. [What Is Shelli?](#1-what-is-shelli)
2. [High-Level Flow](#2-high-level-flow)
3. [The REPL Loop (main.c)](#3-the-repl-loop)
4. [Stage 1 -- Lexical Analysis (lexer.c)](#4-stage-1----lexical-analysis)
5. [Stage 2 -- Parsing (parser.c)](#5-stage-2----parsing)
6. [The AST (ast.h / ast.c)](#6-the-ast)
7. [Stage 3 -- Word Expansion (expand.c)](#7-stage-3----word-expansion)
8. [Stage 4 -- Execution (executor.c)](#8-stage-4----execution)
9. [Built-in Commands (builtins.c)](#9-built-in-commands)
10. [The test / \[ Builtin (test_builtin.c)](#10-the-test---builtin)
11. [Variable Scoping (variables.c)](#11-variable-scoping)
12. [Shell Functions (functions.c)](#12-shell-functions)
13. [Job Control (jobs.c)](#13-job-control)
14. [Heredoc Handling](#14-heredoc-handling)
15. [Multi-Line Input](#15-multi-line-input)
16. [History and History Expansion](#16-history-and-history-expansion)
17. [The TUI System](#17-the-tui-system)
18. [Utilities (util.c)](#18-utilities)
19. [Startup Sequence](#19-startup-sequence)
20. [Complete Command Reference](#20-complete-command-reference)
21. [Supported Syntax At a Glance](#21-supported-syntax-at-a-glance)
22. [Limits and Constants](#22-limits-and-constants)
23. [End-to-End Walkthrough](#23-end-to-end-walkthrough)
24. [Known Limitations](#24-known-limitations)

---

## 1. What Is Shelli?

Shelli is an **educational Unix shell** written in C99.  Its purpose is to
make the invisible stages of shell execution visible.  A full-screen
terminal UI (inspired by LazyVim / Charm) divides the screen into panels
that show, in real time, what the shell is doing at each stage:

```
 Input        -->  what you typed
 Tokenize     -->  the token stream the lexer produced
 Parse        -->  the AST the parser built
 Execute      -->  fork/exec/pipe/redirect log messages
 Result       -->  exit code and captured output
```

In debug mode (`--debug`) the shell pauses after each stage so you can
study what happened.

---

## 2. High-Level Flow

Every command you type passes through four stages:

```
 input string
      |
      v
 +----------+     TokenList
 |  LEXER   | ---------------+
 +----------+                |
                              v
                        +-----------+     AstNode* tree
                        |  PARSER   | -----------------+
                        +-----------+                  |
                                                        v
                                    +-------------------------------------------+
                                    |              EXECUTOR                     |
                                    |  for each command node in the tree:       |
                                    |    1. expand words (tilde, $VAR, glob)    |
                                    |    2. set up redirects                    |
                                    |    3. fork + exec  (or run builtin)       |
                                    +-------------------------------------------+
                                                        |
                                                        v
                                                   exit code + output
```

Word expansion does **not** happen before parsing.  It happens inside the
executor, per-command, at execution time.  This is critical for loops: the
body is parsed once but expanded on every iteration with the current
values of variables.

---

## 3. The REPL Loop

**File:** `src/main.c`

The Read-Eval-Print Loop is the heart of the shell.  Here is what happens
on each iteration, step by step.

### 3.1 Read input

```c
char *line = tui_read_line();
```

The TUI reads a line of input using its own line editor (cursor movement,
history navigation, tab completion).  The line is returned as a malloc'd
string.

### 3.2 History expansion

If the line starts with `!`, the shell expands history references before
doing anything else:

| Syntax | Meaning                          |
|--------|----------------------------------|
| `!!`   | Repeat the last command          |
| `!n`   | Repeat command number *n*        |

The expanded command is printed so the user sees what will actually run.

### 3.3 Accumulate for multi-line

The input is appended to a `DynBuf` accumulation buffer.  If a previous
iteration returned `PARSE_INCOMPLETE`, the new line is joined with a
newline character and the entire accumulated string is re-processed.

### 3.4 Tokenize

```c
lexer_tokenize(input_str, &tokens, lex_error, sizeof(lex_error));
```

On error (unterminated quotes, etc.) the shell either shows the error or
enters multi-line mode to let the user finish the quote.

### 3.5 Parse

```c
ParseResult result = parser_parse_ast(&tokens, &ast, parse_err, sizeof(parse_err));
```

Three outcomes:

| Result             | Action                                          |
|--------------------|-------------------------------------------------|
| `PARSE_OK`         | AST is ready -- proceed to execution            |
| `PARSE_INCOMPLETE` | Accumulate more input (show `...>` prompt)      |
| `PARSE_ERROR`      | Show syntax error message, discard input        |

### 3.6 TUI visualization

After tokenization, the token list is displayed in the Tokenize panel.
After parsing, the AST is rendered as a tree in the Parse panel.
In debug mode, the shell pauses with `tui_wait_step()` after each stage.

### 3.7 Collect heredocs

Before executing, the shell walks the AST looking for commands with
`heredoc_delim` set.  For each one it prompts `heredoc> ` and reads lines
until the delimiter, piping them into a file descriptor stored on the
command.  (See [Heredoc Handling](#14-heredoc-handling).)

### 3.8 Execute

```c
last_exit = executor_run_ast_capture(ast, output_buf, sizeof(output_buf));
```

The executor walks the AST tree, expanding and running each command.
The exit code and any captured output are shown in the Result panel.

### 3.9 Cleanup

The AST is freed, the input string is freed, `$?` is updated, and the
loop starts over.

---

## 4. Stage 1 -- Lexical Analysis

**Files:** `src/lexer.c`, `src/lexer.h`

The lexer converts a raw input string into a flat array of tokens.  It is
a hand-written state machine with four states.

### 4.1 Token types

| Token           | Syntax    | Example            |
|-----------------|-----------|--------------------|
| `TOK_WORD`      | any word  | `echo`, `hello`    |
| `TOK_PIPE`      | `\|`      | `ls \| grep foo`   |
| `TOK_REDIR_IN`  | `<`       | `sort < input.txt` |
| `TOK_REDIR_OUT` | `>`       | `echo hi > out`    |
| `TOK_REDIR_APP` | `>>`      | `echo hi >> log`   |
| `TOK_HEREDOC`   | `<<`      | `cat <<EOF`        |
| `TOK_SEMI`      | `;`       | `echo a ; echo b`  |
| `TOK_AND`       | `&&`      | `true && echo ok`  |
| `TOK_OR`        | `\|\|`    | `false \|\| echo fallback` |
| `TOK_BG`        | `&`       | `sleep 10 &`       |
| `TOK_EOF`       | *(end)*   | *(always appended)* |

Each token has a `value` (malloc'd string, NULL for pure operators) and a
`quoted` flag (1 if any part of the word came from inside quotes).

### 4.2 Lexer states

| State          | Description                                         |
|----------------|-----------------------------------------------------|
| `STATE_START`  | Consuming whitespace, detecting operators           |
| `STATE_WORD`   | Accumulating an unquoted word character by character |
| `STATE_SQUOTE` | Inside `'single quotes'` -- everything is literal   |
| `STATE_DQUOTE` | Inside `"double quotes"` -- `$` and `\` are active  |

### 4.3 How quoting works

**Single quotes** (`'...'`):  Everything between the quotes is taken
literally.  No variable expansion, no escape sequences, no globbing.
The `quoted` flag on the resulting token is set to 1.

**Double quotes** (`"..."`):  Variable references (`$VAR`) and command
substitutions (`$(cmd)`) remain active.  Four escape sequences are
recognized inside double quotes:

| Escape | Produces       |
|--------|----------------|
| `\"`   | literal `"`    |
| `\\`   | literal `\`    |
| `\$`   | literal `$`    |
| `` \` `` | literal `` ` `` |

Any other `\` is kept as-is.  The `quoted` flag is set to 1.

**No quotes:**  Whitespace splits words.  `$`, `*`, `?`, `[` are
metacharacters.  Backslashes are not treated as escapes outside of quotes
(they become part of the word).

### 4.4 Operator disambiguation

The lexer disambiguates multi-character operators by looking one character
ahead:

- `&` followed by `&` --> `TOK_AND`.  Otherwise --> `TOK_BG`.
- `|` followed by `|` --> `TOK_OR`.   Otherwise --> `TOK_PIPE`.
- `<` followed by `<` --> `TOK_HEREDOC`.  Otherwise --> `TOK_REDIR_IN`.
- `>` followed by `>` --> `TOK_REDIR_APP`. Otherwise --> `TOK_REDIR_OUT`.

### 4.5 TokenList

Tokens are stored in a dynamically growing array:

```c
typedef struct {
    Token *tokens;
    int count;
    int capacity;   // starts at 16, doubles on overflow
} TokenList;
```

`tokenlist_free()` frees every token's value string and the array itself.

### 4.6 Error conditions

- Unterminated single quote: returns -1, error message set
- Unterminated double quote: returns -1, error message set
- Allocation failure: returns -1

---

## 5. Stage 2 -- Parsing

**Files:** `src/parser.c`, `src/parser.h`

The parser is a **recursive descent** parser that consumes the token array
and builds an AST.  Keywords like `if`, `while`, `for` are ordinary
`TOK_WORD` tokens -- the parser recognizes them by their string value.

### 5.1 Grammar

```
program        ::= list EOF

list           ::= and_or ( (';' | '&') and_or )* [';' | '&']

and_or         ::= pipeline ( ('&&' | '||') pipeline )*

pipeline       ::= ['!'] command ( '|' command )*

command        ::= if_clause
                 | while_clause
                 | until_clause
                 | for_clause
                 | brace_group
                 | function_def
                 | simple_command

simple_command ::= word+ (redirects interspersed)

if_clause      ::= 'if' list 'then' list
                    ('elif' list 'then' list)*
                    ['else' list]
                    'fi'

while_clause   ::= 'while' list 'do' list 'done'

until_clause   ::= 'until' list 'do' list 'done'

for_clause     ::= 'for' NAME ['in' word*] (';' | newline) 'do' list 'done'

brace_group    ::= '{' list '}'

function_def   ::= NAME'()' '{' list '}'
```

### 5.2 Parser state

```c
typedef struct {
    TokenList *tokens;
    int pos;              // cursor into token array
    char *error;          // error message buffer
    int error_size;
    ParseResult result;   // PARSE_OK / PARSE_ERROR / PARSE_INCOMPLETE
    int in_compound;      // nesting depth counter
} Parser;
```

### 5.3 Helper functions

| Function          | Purpose                                      |
|-------------------|----------------------------------------------|
| `peek(p)`         | Look at current token without consuming       |
| `advance(p)`      | Consume current token, return it              |
| `at_eof(p)`       | True if current token is `TOK_EOF`            |
| `at_word(p, str)` | True if current token is a specific word      |
| `expect_word(p, str)` | Consume word or set error                 |
| `at_list_end(p)`  | True if at `)`, `}`, or a keyword that ends a list |

### 5.4 PARSE_INCOMPLETE

When the parser reaches EOF while inside a compound command
(`in_compound > 0`), it returns `PARSE_INCOMPLETE` instead of
`PARSE_ERROR`.  This tells the REPL to read another line and retry
parsing with the accumulated input.

Examples that trigger PARSE_INCOMPLETE:
- `if true; then` -- waiting for body and `fi`
- `for x in a b c; do` -- waiting for body and `done`
- `{` -- waiting for body and `}`

### 5.5 Redirect parsing

Redirects are parsed inline within `parse_simple_command()`.  When the
parser sees a redirect operator (`<`, `>`, `>>`, `<<`), it consumes the
next word as the filename (or delimiter for heredoc) and attaches it to
the Command struct.

### 5.6 Function definition parsing

The parser detects function definitions by checking if the current word
ends with `()`.  For example, the input `greet() { echo hello; }` is
tokenized as `greet()` (one word), `{`, `echo`, `hello`, `;`, `}`.  The
parser:

1. Strips the `()` suffix to get the function name
2. Expects `{`
3. Parses the body as a list
4. Expects `}`
5. Returns a `NODE_FUNCTION_DEF` node

### 5.7 Command struct

The parser builds `Command` structs for simple commands:

```c
typedef struct Command {
    char **argv;          // NULL-terminated argument array
    int argc;             // argument count
    int *arg_quoted;      // parallel array: 1 if arg was quoted
    Redirect redir_in;    // input redirection  (< file)
    Redirect redir_out;   // output redirection (> file, >> file)
    char *heredoc_delim;  // delimiter for <<DELIM  (NULL if none)
    int heredoc_fd;       // pipe fd filled in before execution (-1 initially)
} Command;
```

The `arg_quoted` array is parallel to `argv`.  If `arg_quoted[i]` is 1,
glob expansion is suppressed for that argument during the expansion stage.
This is how `echo "*.c"` prints the literal string `*.c` instead of
expanding it.

---

## 6. The AST

**Files:** `src/ast.h`, `src/ast.c`

The AST is a tree of `AstNode` structs.  Each node has a type and a
tagged union for its data.

### 6.1 Node types

| Type                | Represents                          | Children / Data          |
|---------------------|-------------------------------------|--------------------------|
| `NODE_COMMAND`      | Simple command (`ls -la`)           | `Command *cmd`           |
| `NODE_PIPELINE`     | Pipeline (`a \| b \| c`)           | Array of child nodes, negated flag |
| `NODE_LIST`         | Semicolon/&&/\|\| list             | Array of `ListEntry` (node + separator) |
| `NODE_IF`           | `if/elif/else/fi`                   | condition, then_body, else_body |
| `NODE_WHILE`        | `while/do/done`                     | condition, body          |
| `NODE_UNTIL`        | `until/do/done`                     | condition, body          |
| `NODE_FOR`          | `for VAR in WORDS; do/done`         | var_name, words[], body  |
| `NODE_FUNCTION_DEF` | `name() { body }`                   | name, body               |
| `NODE_NOT`          | `! pipeline`                        | child                    |
| `NODE_SUBSHELL`     | `( list )`                          | body                     |

### 6.2 List separators

Each entry in a `NODE_LIST` carries a separator that describes the
relationship to the *next* entry:

| Separator        | Syntax | Semantics                            |
|------------------|--------|--------------------------------------|
| `LIST_SEP_SEMI`  | `;`    | Always execute next                  |
| `LIST_SEP_AND`   | `&&`   | Execute next only if this succeeded  |
| `LIST_SEP_OR`    | `\|\|` | Execute next only if this failed     |
| `LIST_SEP_BG`    | `&`    | Run this in background               |
| `LIST_SEP_NONE`  | *(end)* | No next entry                       |

### 6.3 Deep cloning

`ast_clone()` produces a completely independent deep copy of an AST
subtree.  This is used when defining functions: the function body is
cloned so that the original AST can be freed without affecting the stored
function.

### 6.4 How elif works

There is no separate `NODE_ELIF`.  An `elif` clause is represented as a
nested `NODE_IF` stored in the `else_body` of the parent `NODE_IF`.
This means `if/elif/elif/else/fi` becomes a chain of nested if-nodes:

```
NODE_IF
  condition: <first condition>
  then_body: <first body>
  else_body: NODE_IF           <-- the elif
               condition: <second condition>
               then_body: <second body>
               else_body: <else body or NULL>
```

---

## 7. Stage 3 -- Word Expansion

**Files:** `src/expand.c`, `src/expand.h`

Expansion is **not** a separate pass over the token stream.  It happens
inside the executor, per-command, right before fork/exec.  The executor
calls `expand_command(cmd)` which modifies the Command's argv in place.

### 7.1 Expansion order

Expansions happen in this order for each word, matching POSIX:

```
 1. Tilde expansion       ~      -->  /home/user
 2. Variable expansion    $VAR   -->  value
 3. Arithmetic expansion  $((x)) -->  number
 4. Command substitution  $(cmd) -->  output
 5. Glob expansion        *.c    -->  file1.c file2.c
```

Steps 2-4 all happen inside `expand_variables()`.  Step 5 only happens
if the word is **not quoted** (`arg_quoted[i] == 0`).

### 7.2 Tilde expansion

Only the leading `~` is expanded, and only if it is followed by `/` or
is the entire word.  `~user` syntax is not supported.

```
 ~           -->  /home/zebra
 ~/Documents -->  /home/zebra/Documents
 ~other      -->  ~other  (unchanged)
```

### 7.3 Variable expansion

| Syntax         | Expands to                              |
|----------------|-----------------------------------------|
| `$VAR`         | Value of variable VAR                   |
| `${VAR}`       | Same, with explicit boundaries          |
| `$?`           | Exit code of last command               |
| `$$`           | PID of the shell process                |
| `$#`           | Number of positional parameters         |
| `$1` .. `$9`   | Positional parameters (function args)   |
| `$@`           | All positional parameters               |
| `$*`           | All positional parameters (joined)      |

Variable lookup goes through a callback (`shell_var_get`) that checks:
1. Local scope (function variables)
2. Global shell variables
3. Environment (`getenv`)

### 7.4 Arithmetic expansion

`$(( expression ))` evaluates an integer arithmetic expression.

**Supported operators** (by precedence, highest first):

| Precedence | Operators              |
|------------|------------------------|
| Highest    | `()` grouping          |
| Unary      | `-`, `+`, `~`, `!`    |
| Power      | `**`                   |
| Multiply   | `*`, `/`, `%`          |
| Lowest     | `+`, `-`               |

Variables can be referenced inside arithmetic with or without `$`:
```
 x=5
 echo $(( x + 3 ))      -->  8
 echo $(( $x * 2 ))     -->  10
```

Division by zero sets an error flag and produces 0.

### 7.5 Command substitution

`$(command)` runs the command and substitutes its stdout output.

The expansion code extracts the command string, passes it to a callback
function registered by `main.c`.  The callback tokenizes, parses, and
executes the command, capturing stdout.  Trailing newlines are stripped
from the output (matching POSIX behavior).

Nested parentheses inside the command are tracked by a depth counter so
`$(echo $(date))` works correctly.

### 7.6 Glob expansion

If a word contains `*`, `?`, or `[` and is **not quoted**, it is passed
to the POSIX `glob()` function.

- `*.c` matches all `.c` files in the current directory
- `file?.txt` matches `file1.txt`, `fileA.txt`, etc.
- `[abc].txt` matches `a.txt`, `b.txt`, `c.txt`
- `GLOB_NOCHECK` flag: if nothing matches, the pattern is kept as-is

Glob expansion can **grow the argv array**.  If `*.c` matches 5 files,
the single argv entry is replaced by 5 entries, and argc is updated.

### 7.7 Redirect filename expansion

Redirect filenames (`< file`, `> file`, `>> file`) are also expanded
through `expand_word()` (tilde + variable expansion, but no glob).

### 7.8 Why expansion is per-command

Consider this loop:

```sh
x=1
while test $x -le 3; do
    echo $x
    x=$(( x + 1 ))
done
```

If expansion happened before parsing, `$x` would be expanded once to `1`
and the loop would print `1` forever.  Because expansion happens at
execution time, `$x` is re-expanded on every iteration with the current
value.

---

## 8. Stage 4 -- Execution

**Files:** `src/executor.c`, `src/executor.h`

The executor is a **tree-walking interpreter**.  It receives an AST and
recursively executes each node.

### 8.1 Main dispatch

```c
static int execute_node(AstNode *node) {
    switch (node->type) {
        case NODE_COMMAND:      return execute_simple_command(...);
        case NODE_PIPELINE:     return execute_pipeline_node(...);
        case NODE_LIST:         return execute_list_node(...);
        case NODE_IF:           return execute_if_node(...);
        case NODE_WHILE:        return execute_while_node(...);
        case NODE_UNTIL:        return execute_until_node(...);
        case NODE_FOR:          return execute_for_node(...);
        case NODE_NOT:          return execute_not_node(...);
        case NODE_SUBSHELL:     return execute_subshell_node(...);
        case NODE_FUNCTION_DEF: func_define(name, body); return 0;
    }
}
```

### 8.2 Simple command execution

`execute_simple_command()` is the workhorse.  Here is the full decision
tree:

```
 1. Is it a bare VAR=value assignment?
    --> Yes: set variable, return 0
    --> No: continue

 2. Clone the Command (so expansion doesn't mutate the AST)

 3. Run expand_command(cmd) on the clone

 4. Is it an in-parent builtin (cd, export, unset, local)?
    --> Yes: run builtin_execute() in the current process, return

 5. Is it break or continue?
    --> Yes: set shell_break_count or shell_continue_count, return

 6. Is it return?
    --> Yes: set shell_return_flag and shell_return_value, return

 7. Is it source / . ?
    --> Yes: call source_file(), return

 8. Is it a defined function?
    --> Yes: push scope, set positional params, execute body, pop scope

 9. Fork:
    Child:
      - Restore default signal handlers
      - Set up capture pipe if needed
      - Set up redirects (heredoc, <, >, >>)
      - If builtin: run builtin_execute(), _exit()
      - If external: execvp(), print error if fails, _exit(127)
    Parent:
      - Close write end of capture pipe
      - Read captured output if needed
      - waitpid() for child
      - Return exit status
```

### 8.3 Command cloning

Before expansion, the executor clones the Command:

```c
static Command *clone_cmd_for_exec(Command *src);
```

This is necessary because `expand_command()` modifies argv in place
(replacing `$VAR` with its value, expanding globs into multiple args).
Without cloning, a command inside a loop would have its AST permanently
mutated on the first iteration.

### 8.4 Pipeline execution

For a pipeline `cmd1 | cmd2 | cmd3`:

1. Create N-1 pipes (2 pipes for 3 commands)
2. Fork N children
3. Each child:
   - If not first: `dup2(prev_pipe[0], STDIN_FILENO)`
   - If not last: `dup2(curr_pipe[1], STDOUT_FILENO)`
   - Close all pipe fds
   - Expand and execute the command
4. Parent closes all pipe fds
5. Parent waits for all children
6. Return exit code of the **last** command

If the pipeline has `!` prefix (`! cmd1 | cmd2`), the exit code is
inverted: 0 becomes 1, non-zero becomes 0.

### 8.5 List execution

A list is a sequence of pipelines connected by `;`, `&&`, `||`, or `&`:

```
echo a ; echo b && echo c || echo d & echo e
```

The executor iterates through entries and checks the separator of the
**previous** entry to decide whether to run the current one:

| Previous separator | Current entry runs if...      |
|--------------------|-------------------------------|
| `;`                | Always                        |
| `&&`               | Previous exit code was 0      |
| `\|\|`             | Previous exit code was not 0  |
| `&`                | Always (previous backgrounded)|

### 8.6 If execution

```c
static int execute_if_node(AstNode *node) {
    int cond = execute_node(node->data.if_clause.condition);
    if (cond == 0)
        return execute_node(node->data.if_clause.then_body);
    else if (node->data.if_clause.else_body)
        return execute_node(node->data.if_clause.else_body);
    else
        return cond;
}
```

The condition is itself an AST (a list), so it can be any command.  The
exit code of the condition determines the branch.  For `elif`, the
else_body is another `NODE_IF`.

### 8.7 While / Until execution

```
while: loop while condition exit code == 0
until: loop while condition exit code != 0
```

Both support `break` and `continue` with optional count:
- `break 2` breaks out of 2 enclosing loops
- `continue 2` continues the 2nd enclosing loop

The count is decremented on each loop boundary.

### 8.8 For execution

```sh
for f in *.c; do echo $f; done
```

1. For each word in the word list:
   - Expand the word at runtime (`expand_word()`)
   - Set the loop variable (`var_set()` and `setenv()`)
   - Execute the body
   - Check for break/continue

### 8.9 Subshell execution

```sh
( cd /tmp && ls )
```

The body runs in a forked child process.  Changes to variables, working
directory, etc. do not affect the parent shell.

### 8.10 Redirect setup

`setup_redirects()` runs in the child process after fork:

| Redirect | System calls                                         |
|----------|------------------------------------------------------|
| `<<`     | `dup2(heredoc_fd, STDIN_FILENO)`                     |
| `<`      | `open(file, O_RDONLY)`, `dup2(fd, STDIN_FILENO)`     |
| `>`      | `open(file, O_WRONLY\|O_CREAT\|O_TRUNC, 0644)`, `dup2(fd, STDOUT_FILENO)` |
| `>>`     | `open(file, O_WRONLY\|O_CREAT\|O_APPEND, 0644)`, `dup2(fd, STDOUT_FILENO)` |

### 8.11 Exit status

```c
static int exit_status(int status) {
    if (WIFEXITED(status))   return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 1;
}
```

A process killed by signal N has exit code 128 + N (e.g., SIGINT = 2,
so killed-by-SIGINT = 130).

### 8.12 Capture mode

For command substitution (`$(cmd)`) and the REPL output panel, the
executor can capture stdout:

- **Simple commands**: a pipe is created before fork.  The child's stdout
  goes to the pipe.  The parent reads the pipe into a buffer.
- **Compound commands** (if, for, while, lists): the entire AST execution
  is forked.  The child's stdout is redirected to a pipe, and the parent
  reads the output.

Trailing newlines are stripped from captured output (matching POSIX
behavior for command substitution).

### 8.13 Signal handling in execution

- The shell sets `fg_child_pid` to the foreground child's PID
- SIGINT handler in `main.c` forwards the signal: `kill(fg_child_pid, SIGINT)`
- Children restore `SIG_DFL` for SIGINT and SIGQUIT
- SIGQUIT is ignored by the shell itself

---

## 9. Built-in Commands

**File:** `src/builtins.c`

Builtins are commands that run inside the shell process (or are
recognized specially by the executor).  Here is every builtin:

### Navigation & Environment

| Command             | Description                                          |
|---------------------|------------------------------------------------------|
| `cd [dir]`          | Change directory.  Default: `$HOME`                  |
| `pwd`               | Print working directory                              |
| `export VAR=val`    | Set and export environment variable                  |
| `export VAR`        | Export existing shell variable to environment         |
| `export`            | Print all environment variables                      |
| `unset VAR`         | Remove variable from shell and environment           |

### Shell Control

| Command             | Description                                          |
|---------------------|------------------------------------------------------|
| `exit [n]`          | Exit shell with code *n* (default 0)                 |
| `help`              | Print comprehensive help text                        |
| `history`           | Print command history                                |
| `source file`       | Execute commands from file in current shell          |
| `. file`            | Same as `source`                                     |

### Testing & Output

| Command             | Description                                          |
|---------------------|------------------------------------------------------|
| `echo [-neE] args`  | Print arguments (see below for flags)                |
| `test EXPR`         | Evaluate conditional expression (exit 0=true, 1=false) |
| `[ EXPR ]`          | Same as `test`, requires closing `]`                 |
| `true`              | Always returns 0                                     |
| `false`             | Always returns 1                                     |
| `type cmd`          | Show whether cmd is a builtin, function, or external |

### Function & Loop Control

| Command             | Description                                          |
|---------------------|------------------------------------------------------|
| `local VAR=val`     | Set variable local to current function scope         |
| `return [n]`        | Return from function with exit code *n*              |
| `break [n]`         | Break out of *n* enclosing loops (default 1)         |
| `continue [n]`      | Skip to next iteration of *n*th enclosing loop       |

### Job Control

| Command             | Description                                          |
|---------------------|------------------------------------------------------|
| `jobs`              | List background jobs                                 |
| `fg [%n]`           | Bring job *n* to foreground                          |
| `bg [%n]`           | Continue stopped job *n* in background               |

### Echo flags

| Flag  | Effect                                |
|-------|---------------------------------------|
| `-n`  | Do not print trailing newline         |
| `-e`  | Interpret escape sequences            |
| `-E`  | Do not interpret escapes (default)    |
| `-ne` | Combine `-n` and `-e`                 |

Escape sequences with `-e`:

| Escape  | Character         |
|---------|-------------------|
| `\n`    | Newline           |
| `\t`    | Tab               |
| `\r`    | Carriage return   |
| `\\`    | Backslash         |
| `\a`    | Bell              |
| `\b`    | Backspace         |
| `\f`    | Form feed         |
| `\v`    | Vertical tab      |
| `\0nnn` | Octal byte value  |

### In-parent vs forked builtins

Some builtins **must** run in the parent process because they modify
shell state:

- `cd` -- changes working directory
- `export` -- sets environment variables
- `unset` -- removes variables
- `local` -- sets local variables
- `break`, `continue` -- affect loop state
- `return` -- affects function state
- `source` -- executes in current environment

All other builtins (`echo`, `pwd`, `test`, `type`, `history`, etc.) can
safely run in a forked child.  The executor forks them when they appear
in a pipeline or when output capture is needed.

---

## 10. The test / [ Builtin

**File:** `src/test_builtin.c`

The `test` command (also invoked as `[`) evaluates conditional
expressions.  It uses a recursive descent evaluator over the argv array.

### File tests

| Test       | True if...                          |
|------------|-------------------------------------|
| `-e file`  | File exists                         |
| `-f file`  | File exists and is a regular file   |
| `-d file`  | File exists and is a directory      |
| `-r file`  | File is readable                    |
| `-w file`  | File is writable                    |
| `-x file`  | File is executable                  |
| `-s file`  | File exists and is non-empty        |
| `-L file`  | File is a symbolic link             |

### String tests

| Test              | True if...                      |
|-------------------|---------------------------------|
| `-n string`       | String is non-empty             |
| `-z string`       | String is empty                 |
| `s1 = s2`         | Strings are equal               |
| `s1 == s2`        | Strings are equal (alternate)   |
| `s1 != s2`        | Strings are not equal           |

### Numeric comparisons

| Test           | True if...                     |
|----------------|--------------------------------|
| `n1 -eq n2`   | n1 equals n2                   |
| `n1 -ne n2`   | n1 not equal to n2             |
| `n1 -lt n2`   | n1 less than n2                |
| `n1 -le n2`   | n1 less than or equal to n2    |
| `n1 -gt n2`   | n1 greater than n2             |
| `n1 -ge n2`   | n1 greater than or equal to n2 |

### Logical operators

| Operator       | Meaning                         |
|----------------|---------------------------------|
| `! expr`       | NOT                             |
| `expr -a expr` | AND                             |
| `expr -o expr` | OR                              |
| `( expr )`     | Grouping                        |

### Return values

| Code | Meaning              |
|------|----------------------|
| 0    | Expression is true   |
| 1    | Expression is false  |
| 2    | Syntax error         |

---

## 11. Variable Scoping

**File:** `src/variables.c`

Shelli has a three-tier variable lookup model:

```
 Lookup order:
   1. Local scope (innermost function scope)
   2. Global shell variables
   3. Environment (getenv)
```

### Scope stack

```c
VarScope global_scope;                  // always exists
VarScope *scope_stack[MAX_SCOPES];      // function scopes (max depth 32)
int scope_depth;                        // current nesting level
```

Each `VarScope` holds up to 256 name-value pairs.

### When scopes are used

| Operation             | Effect                                  |
|-----------------------|-----------------------------------------|
| `var_set(n, v)`       | Set in current local scope (if any), else global |
| `var_get(n)`          | Search local (top to bottom) -> global -> env |
| `var_set_local(n, v)` | Set in topmost local scope only         |
| `var_push_scope()`    | Called on function entry                |
| `var_pop_scope()`     | Called on function exit (frees locals)  |
| `var_export(n)`       | Copy to environment via `setenv()`      |
| `var_unset(n)`        | Remove from all scopes and `unsetenv()` |

### Positional parameters

When a function is called, its arguments become `$1`, `$2`, etc.:

```c
var_set_positional(argc - 1, argv + 1);
```

- `$#` returns the count of positional parameters
- `$@` and `$*` return all parameters joined by spaces
- `$1` through `$9` return individual parameters

### VAR=value assignment

A bare `VAR=value` (with no command name) is detected by the executor and
handled as a variable assignment without forking:

```sh
x=hello          # sets shell variable x
echo $x          # prints "hello"
```

---

## 12. Shell Functions

**File:** `src/functions.c`

### Defining a function

```sh
greet() {
    echo "Hello, $1!"
}
```

The parser produces a `NODE_FUNCTION_DEF` node.  The executor calls
`func_define(name, body)`, which stores a deep clone of the body AST in
a function table (up to 128 functions).

### Calling a function

When the executor encounters a command name that is neither a builtin nor
found in PATH, it checks the function table:

```c
AstNode *func_body = func_lookup(cmd->argv[0]);
if (func_body) {
    var_push_scope();                              // new local scope
    var_set_positional(cmd->argc - 1, cmd->argv + 1);  // $1, $2, ...
    int ret = execute_node(func_body);             // execute body
    if (shell_return_flag) {                       // handle return
        ret = shell_return_value;
        shell_return_flag = 0;
    }
    var_pop_scope();                               // clean up locals
    return ret;
}
```

### Command lookup order

When you type a command name, the shell checks in this order:

1. **Variable assignment** (`VAR=value` with no command)
2. **In-parent builtins** (`cd`, `export`, `unset`, `local`)
3. **Loop control** (`break`, `continue`)
4. **`return`**
5. **`source` / `.`**
6. **Function table** (`func_lookup()`)
7. **Other builtins** (`echo`, `test`, `pwd`, etc.)
8. **External command** (`execvp()` searches `$PATH`)

---

## 13. Job Control

**File:** `src/jobs.c`

Shelli has basic job control infrastructure.

### Job table

```c
static Job job_table[MAX_JOBS];  // MAX_JOBS = 64
```

Each job has:
- `id` -- 1-based job number
- `pgid` -- process group ID
- `state` -- `JOB_RUNNING`, `JOB_STOPPED`, or `JOB_DONE`
- `command` -- command string for display
- `exit_status`

### Background job checking

At the top of each REPL iteration, `jobs_check()` is called.  It uses
non-blocking `waitpid(-1, &status, WNOHANG | WUNTRACED)` to detect
completed or stopped background processes.

### fg and bg

- `fg [%n]` -- brings job *n* to the foreground.  If the job was stopped,
  sends `SIGCONT`.  Waits for the job synchronously.
- `bg [%n]` -- sends `SIGCONT` to a stopped background job.

---

## 14. Heredoc Handling

### What is a heredoc?

A heredoc (`<<DELIMITER`) feeds multi-line input to a command's stdin:

```sh
cat <<EOF
Hello
World
EOF
```

### How shelli implements it

1. **Lexer**: Tokenizes `<<` as `TOK_HEREDOC`.  The next word becomes the
   delimiter.

2. **Parser**: Stores the delimiter in `cmd->heredoc_delim` and sets
   `cmd->heredoc_fd = -1`.

3. **Main loop** (`collect_heredocs_ast()`): Before execution, walks the
   entire AST looking for commands with `heredoc_delim` set.  For each:
   - Suspends raw mode (so fgets works normally)
   - Creates a pipe
   - Prompts `heredoc> ` and reads lines until the delimiter is found
   - Writes each line to the pipe's write end
   - Stores the pipe's read end in `cmd->heredoc_fd`
   - Resumes raw mode

4. **Executor** (`setup_redirects()`): `dup2(heredoc_fd, STDIN_FILENO)`
   redirects the command's stdin to read from the pipe.

---

## 15. Multi-Line Input

Shelli supports multi-line compound commands interactively.  You can type:

```
shelli> if true; then
  ...>    echo "yes"
  ...>  fi
```

### How it works

1. User types `if true; then` and presses Enter.
2. The lexer tokenizes it successfully.
3. The parser starts parsing and recognizes `if ... then` but hits EOF
   before finding `fi`.  Since `in_compound > 0`, it returns
   `PARSE_INCOMPLETE`.
4. The REPL stores the input in `accumulated` DynBuf and sets
   `multiline = 1`.
5. On the next iteration, the TUI shows a continuation prompt.
6. The user types `echo "yes"`.  This is appended to accumulated with a
   newline separator.
7. The full accumulated input is re-tokenized and re-parsed.  Still
   `PARSE_INCOMPLETE` (no `fi` yet).
8. User types `fi`.  Accumulated is now `if true; then\necho "yes"\nfi`.
9. Parser returns `PARSE_OK` with a complete `NODE_IF` AST.
10. Execution proceeds normally.

Unterminated quotes also trigger multi-line mode through the lexer (it
returns an error for unterminated quotes, and the REPL enters multi-line
accumulation).

---

## 16. History and History Expansion

### Persistent history

History is loaded from disk on startup (`tui_history_load()`) and saved
on exit (`tui_history_save()`).  The `history` builtin prints all stored
commands.

### Navigation

Up/Down arrow keys navigate through history in the line editor.  The
current input is saved when you start navigating and restored if you
return to the bottom.

### History expansion

| Syntax | Expands to                |
|--------|---------------------------|
| `!!`   | Previous command          |
| `!n`   | Command number *n*        |

History expansion happens before tokenization.  The expanded command is
printed so the user can see what will be executed.

---

## 17. The TUI System

**Files:** `src/tui/tui_core.c`, `src/tui/tui_input.c`,
`src/tui/tui_render.c`, `src/tui/tui_widgets.c`, `src/tui/tui_theme.c`,
`src/tui/tui_logo.c`, `src/tui/tui_anim.c`, `src/tui/tui_icons.c`

### Terminal setup

- **Alternate screen buffer**: preserves the user's terminal content
- **Raw mode**: disables line buffering, echo, and signal generation so
  the TUI has full control of input/output
- **SIGWINCH handler**: detects terminal resize and redraws

### Panel layout

The screen is divided into panels:

```
+------------------------------------------------------+
|  INPUT     | what you typed                          |
+------------------------------------------------------+
|  TOKENIZE  | token stream from lexer                 |
+------------------------------------------------------+
|  PARSE     | AST tree from parser                    |
+------------------------------------------------------+
|  EXECUTE   | fork/exec/redirect log messages         |
+------------------------------------------------------+
|  RESULT    | exit code and captured output            |
+------------------------------------------------------+
```

### Color theme

Shelli uses a **Catppuccin Mocha** inspired 256-color palette with neon
accents:

- Dark backgrounds (colors 234, 236)
- Light text (colors 249, 254)
- Accents: Blue (111), Pink (218), Green (114), Peach (216), Red (204)
- Neon highlights: Pink (213), Cyan (123), Purple (141), Green (84)

### Box drawing

Panels use Unicode box-drawing characters:
- Light borders for inner panels: `-- | +` (ASCII fallback)
- Rounded corners: `...`
- Heavy borders for the outer frame

### AST rendering

`tui_show_ast()` recursively renders the AST as a tree in the Parse
panel.  Each node type is shown with its relevant data:

```
List (;)
  Pipeline
    Command: echo hello
  Pipeline
    Command: ls -la
```

### Line editor features

The TUI includes a full line editor with:

| Key        | Action                                     |
|------------|--------------------------------------------|
| Left/Right | Move cursor                                |
| Home       | Move to start of line                      |
| End        | Move to end of line                        |
| Backspace  | Delete character before cursor              |
| Delete     | Delete character at cursor                 |
| Ctrl+A     | Move to start (same as Home)               |
| Ctrl+E     | Move to end (same as End)                  |
| Ctrl+K     | Kill from cursor to end of line            |
| Ctrl+U     | Kill from start to cursor                  |
| Ctrl+W     | Kill previous word                         |
| Ctrl+L     | Clear screen and redraw                    |
| Ctrl+C     | Cancel current line                        |
| Ctrl+D     | EOF (exit shell)                           |
| Up/Down    | History navigation                         |
| Tab        | Tab completion                             |

### Tab completion

Tab completion works for:

1. **Builtin commands**: All 18+ builtins
2. **Shell keywords**: `if`, `then`, `elif`, `else`, `fi`, `while`,
   `until`, `do`, `done`, `for`, `in`, `case`, `esac`
3. **External commands**: Searches `$PATH` directories
4. **Filenames**: In current directory (for non-command positions)

When multiple matches exist, the longest common prefix is inserted.

### Debug mode

With `--debug`, the shell pauses after each stage:

```
 1. "Input received"        -- after reading input
 2. "Tokenization complete" -- after lexer runs
 3. "Parsing complete"      -- after parser builds AST
 4. "Execution complete"    -- after command finishes
```

Press any key to advance to the next stage.

### Animations

The TUI supports several animation types:
- **Fade in**: Characters appear with increasing opacity
- **Typewriter**: Characters appear one at a time
- **Easing functions**: Cubic, quadratic, elastic, linear

### Splash screen

On startup (unless `--no-splash`), a splash screen is shown with the
shelli logo using gradient text and animations.

---

## 18. Utilities

**Files:** `src/util.c`, `src/util.h`

### DynBuf -- Dynamic Buffer

A growable byte buffer used throughout the codebase:

```c
typedef struct {
    char *data;
    int len;    // current length
    int cap;    // allocated capacity
} DynBuf;
```

| Function                     | Description                              |
|------------------------------|------------------------------------------|
| `dynbuf_init(b)`             | Initialize to empty (NULL data)          |
| `dynbuf_push(b, c)`         | Append one byte                          |
| `dynbuf_append(b, s, n)`    | Append *n* bytes from *s*                |
| `dynbuf_append_str(b, s)`   | Append null-terminated string            |
| `dynbuf_steal(b)`           | Return data as malloc'd string, reset buf|
| `dynbuf_free(b)`            | Free data, reset to empty                |

Growth strategy: initial capacity 64, doubles on each resize.

DynBuf is used for:
- Accumulating word characters in the lexer
- Building expanded strings in the expander
- Accumulating multi-line input in the REPL

---

## 19. Startup Sequence

Here is exactly what happens when you run `./shelli`:

```
 1.  Parse command-line arguments (--debug, --no-splash, --help)
 2.  tui_init()         -- enter alternate screen, enable raw mode
 3.  tui_history_load() -- load ~/.shelli_history
 4.  Set up SIGINT handler (forwards to foreground child)
 5.  Ignore SIGQUIT
 6.  var_init()          -- initialize variable system
 7.  func_init()         -- initialize function table
 8.  jobs_init()         -- initialize job table
 9.  tui_set_debug()     -- enable/disable debug mode
10.  Register callbacks:
     - executor_set_logger(exec_logger)          -- TUI logging
     - expand_set_substitution_fn(cmd_subst_callback)  -- $() support
     - expand_set_var_get_fn(var_get)             -- variable lookup
11.  Load ~/.shellirc    -- source rc file if it exists
12.  tui_splash()        -- show splash screen (unless --no-splash)
13.  tui_draw_frame()    -- draw initial TUI frame
14.  Enter REPL loop
```

And on exit:

```
 1.  dynbuf_free(&accumulated)  -- free multi-line buffer
 2.  tui_history_save()         -- save history to disk
 3.  var_cleanup()              -- free all variables and scopes
 4.  func_cleanup()             -- free all function definitions
 5.  jobs_cleanup()             -- free job table
 6.  tui_cleanup()              -- exit alternate screen, restore terminal
 7.  Return last exit code
```

---

## 20. Complete Command Reference

### Simple commands

```sh
command arg1 arg2 ...
```

Runs an external program found via `$PATH`, a builtin, or a function.

### Pipelines

```sh
cmd1 | cmd2 | cmd3
```

Connect stdout of each command to stdin of the next.  Exit code is from
the last command.

### Negated pipelines

```sh
! command
! cmd1 | cmd2
```

Inverts the exit code of the pipeline (0 becomes 1, non-zero becomes 0).

### Sequential execution

```sh
cmd1 ; cmd2 ; cmd3
```

Run commands one after another regardless of exit codes.

### Conditional AND

```sh
cmd1 && cmd2
```

Run `cmd2` only if `cmd1` succeeds (exit code 0).

### Conditional OR

```sh
cmd1 || cmd2
```

Run `cmd2` only if `cmd1` fails (exit code non-zero).

### Background

```sh
command &
```

Run command in the background.

### Input redirection

```sh
command < input_file
```

Read stdin from a file.

### Output redirection

```sh
command > output_file
command >> output_file
```

Write stdout to a file (truncate or append).

### Heredoc

```sh
command <<DELIMITER
line 1
line 2
DELIMITER
```

Feed multi-line text to command's stdin.

### If / elif / else

```sh
if condition; then
    body
elif condition2; then
    body2
else
    body3
fi
```

### While loop

```sh
while condition; do
    body
done
```

### Until loop

```sh
until condition; do
    body
done
```

### For loop

```sh
for var in word1 word2 word3; do
    echo $var
done
```

### Brace group

```sh
{ cmd1; cmd2; cmd3; }
```

Group commands (executed in current shell, not a subshell).

### Subshell

```sh
( cmd1; cmd2; cmd3 )
```

Group commands in a forked subprocess.  Variable changes do not affect the
parent.

### Function definition

```sh
name() {
    body
}
```

### Function call

```sh
name arg1 arg2
```

Arguments become `$1`, `$2`, etc. inside the function.

### Variable assignment

```sh
VAR=value
```

Sets a shell variable (no export).

### Variable expansion

```sh
$VAR
${VAR}
$?       # last exit code
$$       # shell PID
$#       # number of positional params
$1 - $9  # positional parameters
$@ $*    # all positional parameters
```

### Arithmetic expansion

```sh
$(( expression ))
```

Supports `+`, `-`, `*`, `/`, `%`, `**`, `~`, `!`, parentheses, and
variable references.

### Command substitution

```sh
$(command)
```

Replaced by the stdout of `command` (trailing newlines stripped).

### Tilde expansion

```sh
~            # expands to $HOME
~/path       # expands to $HOME/path
```

### Glob expansion

```sh
*.c          # all .c files
file?.txt    # single character wildcard
[abc].txt    # character class
```

Suppressed inside quotes.

### Quoting

```sh
'literal string'          # no expansion, no escapes
"string with $VAR"        # variable expansion active
"string with $(cmd)"      # command substitution active
"escaped \" quote"        # escape sequences: \" \\ \$ \`
```

---

## 21. Supported Syntax At a Glance

```
 COMMANDS          ls -la, echo hello, /usr/bin/env
 PIPELINES         cmd1 | cmd2 | cmd3
 LISTS             cmd1 ; cmd2 && cmd3 || cmd4
 BACKGROUND        cmd &
 NEGATION          ! cmd
 REDIRECTS         < file, > file, >> file, <<DELIM
 IF/ELIF/ELSE/FI   if cond; then body; elif cond; then body; else body; fi
 WHILE/DO/DONE     while cond; do body; done
 UNTIL/DO/DONE     until cond; do body; done
 FOR/IN/DO/DONE    for var in words; do body; done
 BRACE GROUP       { cmd1; cmd2; }
 SUBSHELL          ( cmd1; cmd2 )
 FUNCTIONS         name() { body; }
 ASSIGNMENT         VAR=value
 TILDE              ~ ~/path
 VARIABLES          $VAR ${VAR} $? $$ $# $1-$9 $@ $*
 ARITHMETIC         $(( expr ))
 CMD SUBSTITUTION   $(command)
 SINGLE QUOTES      'no expansion'
 DOUBLE QUOTES      "with $expansion"
 GLOB               * ? [abc]
 HEREDOC            <<DELIMITER ... DELIMITER
 HISTORY            !! !n
 BUILTINS           cd pwd exit help export unset history
                    echo test [ ] type true false
                    local return source . break continue
                    jobs fg bg
```

---

## 22. Limits and Constants

| Constant          | Value | Where used                          |
|-------------------|-------|-------------------------------------|
| `MAX_ARGS`        | 256   | Maximum arguments per command       |
| `MAX_VARS`        | 256   | Maximum variables per scope         |
| `MAX_SCOPES`      | 32    | Maximum function nesting depth      |
| `MAX_FUNCTIONS`   | 128   | Maximum defined functions           |
| `MAX_JOBS`        | 64    | Maximum concurrent jobs             |
| `MAX_POSITIONAL`  | 64    | Maximum positional parameters       |
| `LINE_BUFFER_SIZE`| 4096  | Maximum input line length           |
| `HISTORY_SIZE`    | 100   | Maximum history entries             |
| `MAX_PANEL_LINES` | 32    | Maximum lines per TUI panel         |
| `MAX_LINE_LEN`    | 512   | Maximum line length in TUI panel    |

---

## 23. End-to-End Walkthrough

Let's trace what happens for a realistic command:

```sh
for f in *.c; do wc -l $f; done | sort -n
```

### Step 1: Lexer

Input string is tokenized into:

```
TOK_WORD("for")  TOK_WORD("f")  TOK_WORD("in")  TOK_WORD("*.c")
TOK_SEMI  TOK_WORD("do")  TOK_WORD("wc")  TOK_WORD("-l")
TOK_WORD("$f")  TOK_SEMI  TOK_WORD("done")  TOK_PIPE
TOK_WORD("sort")  TOK_WORD("-n")  TOK_EOF
```

`"*.c"` is a `TOK_WORD` with `quoted=0`.  `"$f"` is a `TOK_WORD` with
`quoted=0`.

### Step 2: Parser

The parser builds this AST:

```
NODE_PIPELINE (2 commands)
  [0] NODE_FOR
        var_name: "f"
        words: ["*.c"]
        body: NODE_LIST (1 entry)
          [0] NODE_PIPELINE (1 command)
                [0] NODE_COMMAND: argv=["wc", "-l", "$f"]
  [1] NODE_COMMAND: argv=["sort", "-n"]
```

Note: `"*.c"` and `"$f"` are stored as raw strings.  No expansion yet.

### Step 3: Pipeline execution

The executor sees a 2-command pipeline.  It creates a pipe and forks two
children.

**Child 1** (the for loop):
- stdout goes to the pipe
- The for loop expands `*.c` via `expand_word("*.c", 0)`:
  - No tilde, no variables
  - `glob("*.c")` matches `main.c`, `lexer.c`, `parser.c`, etc.
  - Returns the first match (glob expansion happens per-word in for loops)
- Actually, for-loop words are expanded one at a time.  `*.c` as a single
  word would expand to the first match only.  The glob expansion in
  `expand_word` does NOT do multi-word splitting -- that's only in
  `expand_command`.

  Wait -- `expand_word` doesn't do glob expansion.  The for-loop words
  would need special handling for globs.  In the current implementation,
  `expand_word("*.c", 0)` only does tilde and variable expansion, so
  the literal `*.c` would be set as the variable value.

  (This is a known limitation: for-loop word lists don't glob-expand.)

- For each word, sets `f=word`, runs `wc -l $f`:
  - Clone command, expand: `$f` becomes the filename
  - Fork, exec `wc -l filename`
  - Output goes to the pipeline pipe

**Child 2** (`sort -n`):
- stdin reads from the pipe
- Expand argv (no expansion needed)
- `execvp("sort", ["sort", "-n"])`
- Reads wc output lines, sorts numerically, prints to terminal

**Parent**: closes pipes, waits for both children, returns last exit code.

### Step 4: Result

The sorted line-count listing appears in the Result panel with the exit
code of `sort`.

---

## 24. Known Limitations

These features are **not** implemented in shelli (by design, as an
educational shell):

| Feature                        | Status          |
|--------------------------------|-----------------|
| `${VAR:-default}`              | Not implemented |
| `${VAR##pattern}`              | Not implemented |
| Arrays / associative arrays    | Not implemented |
| Aliases                        | Not implemented |
| `<(process substitution)`      | Not implemented |
| Coprocesses (`\|&`, `coproc`)  | Not implemented |
| Regex matching (`=~`)          | Not implemented |
| `select` loops                 | Not implemented |
| `case/esac`                    | Not implemented |
| Traps (`trap`)                 | Not implemented |
| `set` / `shopt`                | Not implemented |
| `read` builtin                 | Not implemented |
| `printf` builtin               | Not implemented |
| `eval` builtin                 | Not implemented |
| `exec` builtin                 | Not implemented |
| `wait` builtin                 | Not implemented |
| Brace expansion (`{a,b,c}`)   | Not implemented |
| `~user` expansion              | Not implemented |
| Glob: `**` recursive           | Not implemented |
| Glob: extglob `@()` `+()`      | Not implemented |
| Word splitting after expansion  | Simplified      |
| Signal trapping                 | Minimal         |
| POSIX full compliance           | Not a goal      |

Shelli focuses on the core mechanisms that make a shell work: lexing,
parsing, tree building, expansion, fork/exec, pipes, and redirects.
These are the concepts that transfer to understanding bash, zsh, and
other production shells.
