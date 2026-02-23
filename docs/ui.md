# Shelli — TUI Experience Design Guide
 
> A comprehensive visual and interaction design for Shelli's terminal UI,
> derived from Mole's battle-tested TUI patterns and adapted for a shell
> pipeline visualizer written in C99.
 
---

 
## Table of Contents
 
1. [Design Philosophy](#1-design-philosophy)
2. [Color Theme — Catppuccin Mocha](#2-color-theme--catppuccin-mocha)
3. [Typography & Box-Drawing](#3-typography--box-drawing)
4. [Splash Screen & Mascot Animation](#4-splash-screen--mascot-animation)
5. [Main Layout — The Pipeline View](#5-main-layout--the-pipeline-view)
6. [Panel 1 — Input (Line Editor)](#6-panel-1--input-line-editor)
7. [Panel 2 — Tokenizer](#7-panel-2--tokenizer)
8. [Panel 3 — Parser / AST](#8-panel-3--parser--ast)
9. [Panel 4 — Executor](#9-panel-4--executor)
10. [Panel 5 — Result / Output](#10-panel-5--result--output)
11. [Status Bar & Key Hints](#11-status-bar--key-hints)
12. [Debug / Step-Through Mode](#12-debug--step-through-mode)
13. [Animation System](#13-animation-system)
14. [Widget Catalog](#14-widget-catalog)
15. [Interaction & Keybindings](#15-interaction--keybindings)
16. [Responsive Layout](#16-responsive-layout)
17. [Implementation Architecture (C99)](#17-implementation-architecture-c99)
18. [Appendix — ASCII Mockups](#18-appendix--ascii-mockups)
 
---
 
## 1. Design Philosophy
 
Learned directly from Mole's TUI:
 
| Principle | Mole Pattern | Shelli Adaptation |
|---|---|---|
| **Alive, not static** | Mole mascot bounces; spinner speed tracks CPU | Pipeline flow animates token-by-token; cursor blinks at shell cadence |
| **Semantic color** | Green = OK, Yellow = warn, Red = danger | Green = valid token, Yellow = expansion, Red = syntax error, Blue = keyword |
| **Progressive disclosure** | Overview → Detail drill-down | Command line → Tokens → AST → Execution → Output |
| **Density with clarity** | Two-column cards, sparklines, compact bars | Five-panel pipeline, each panel dense but single-purpose |
| **Keyboard-first** | Vim keys (h/j/k/l), number shortcuts | Same vim keys + Tab cycles panels + number shortcuts for debug breakpoints |
| **No chrome waste** | Every pixel is data or navigation | Panels use single-line borders; no decorative padding |
 
---
 
## 2. Color Theme — Catppuccin Mocha
 
Hex values from Mole's `theme.go`, mapped to Shelli's semantic roles:
 
### Base Palette
 
```
Background (Base)     #1e1e2e    ── Terminal / panel backgrounds
Surface (Surface0)    #313244    ── Active panel highlight, selection bg
Overlay (Surface1)    #45475a    ── Borders, separators, inactive panels
Subtle text (Overlay0)#6c7086    ── Line numbers, hints, disabled text
Body text (Text)      #cdd6f4    ── Primary content
Subtext (Subtext1)    #bac2de    ── Secondary labels
```
 
### Semantic Token Colors
 
```
┌─────────────────────────────────────────────────────────┐
│  ROLE              HEX        USAGE                     │
├─────────────────────────────────────────────────────────┤
│  Keyword           #cba6f7    if, then, else, fi,       │
│  (Mauve)                      while, for, case          │
│                                                         │
│  String literal    #a6e3a1    "hello", 'world'          │
│  (Green)                                                │
│                                                         │
│  Variable          #f9e2af    $HOME, ${PATH},           │
│  (Yellow)                     expansion results         │
│                                                         │
│  Operator/Pipe     #89b4fa    |, &&, ||, ;, >>, <       │
│  (Blue)                                                 │
│                                                         │
│  Command name      #f5c2e7    ls, grep, echo            │
│  (Pink)                                                 │
│                                                         │
│  Argument          #cdd6f4    -la, --color, filenames   │
│  (Text)                                                 │
│                                                         │
│  Error             #f38ba8    Syntax errors, bad fds    │
│  (Red)                                                  │
│                                                         │
│  Number/FD         #fab387    2>&1, file descriptors    │
│  (Peach)                                                │
│                                                         │
│  Comment           #6c7086    # this is a comment       │
│  (Overlay0)                                             │
│                                                         │
│  Glob/Pattern      #94e2d5    *.txt, [a-z], ?           │
│  (Teal)                                                 │
│                                                         │
│  Active cursor     #89dceb    Blinking block cursor     │
│  (Sky)                                                  │
│                                                         │
│  Success           #a6e3a1    Exit code 0, pipe OK      │
│  (Green)                                                │
│                                                         │
│  Warning           #f9e2af    Non-zero exit, signals    │
│  (Yellow)                                               │
│                                                         │
│  Danger            #f38ba8    SIGSEGV, exec fail        │
│  (Red)                                                  │
└─────────────────────────────────────────────────────────┘
```
 
### Color Thresholds (from Mole's pattern)
 
Exit codes use graduated color like Mole's disk usage bars:
- `0`        → Green (#a6e3a1)
- `1`        → Yellow (#f9e2af)
- `2`        → Peach (#fab387)
- `126-127`  → Red (#f38ba8)
- `128+`     → Red bold (signal death)
 
---
 
## 3. Typography & Box-Drawing
 
### Box Characters
 
```
Panel borders (active):   ┌─────────┐
                          │ content  │
                          └─────────┘
 
Panel borders (inactive): ┌╌╌╌╌╌╌╌╌╌┐      ← Mole's dashed separator
                          ╎ content  ╎         style for inactive panels
                          └╌╌╌╌╌╌╌╌╌┘
 
Pipeline flow arrows:     ──▶  ═══▶  ···▶   ← connects panels left-to-right
 
AST tree branches:        ├── child
                          │   ├── grandchild
                          │   └── last grandchild
                          └── last child
 
Token chips:              ╭───────╮
                          │ TOKEN │
                          ╰───────╯
```
 
### Special Characters (from Mole's icon vocabulary)
 
```
◉  Active pipeline stage        (Mole uses for categories)
◯  Inactive pipeline stage
▶  Current execution point      (Mole's cyan cursor)
█▓▒░  Progress/throughput bars  (Mole's gradient fills)
▁▂▃▄▅▆▇█  Sparklines           (Mole uses for I/O graphs)
⚡ Fork/exec event
⇅  Pipe data flow               (Mole uses for network)
❊  Glob expansion               (Mole uses for processes)
⟳  Command substitution
↪  Redirect
✓  Success
✗  Failure
◆  Breakpoint (debug mode)
```
 
---
 
## 4. Splash Screen & Mascot Animation
 
Inspired by Mole's walking/bouncing mole mascot with CPU-tied animation speed:
 
### Shelli Mascot — The Shell (a hermit crab / nautilus shell)
 
```
    Frame 1 (idle)          Frame 2 (thinking)       Frame 3 (executing)
 
      ___                     ___                      ___
     /   \                   /   \                    /   \ ⚡
    | o_o |                 | ◉_◉ |                  | >_< |
    |  >  |                 |  =  |                  |  D  |
     \___/                   \___/                    \___/
    /|   |\                 /| ~ |\                  /|~~~|\
   ~ ~ ~ ~ ~              ~ ~ ~ ~ ~               ⚡~ ~ ~ ~⚡
```
 
### Splash Sequence
 
```
┌──────────────────────────────────────────────────────────────────┐
│                                                                  │
│                           ___                                    │
│                          /   \                                   │
│                         | o_o |                                  │
│                         |  >  |                                  │
│                          \___/                                   │
│                         /|   |\                                  │
│                        ~ ~ ~ ~ ~                                 │
│                                                                  │
│                     s h e l l i                                  │
│               ─────────────────────                              │
│              a shell that shows its work                         │
│                                                                  │
│                        v0.1.0                                    │
│                                                                  │
│              Press any key to continue...                        │
│              ~~~~~~~~~~~~~~~~~~~~~~~~~~~~                        │
│                    (blinks at 1Hz)                                │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```
 
Animation behavior (Mole pattern):
- Mascot eyes blink every 3–5s (randomized, like Mole's bounce)
- "Press any key" text blinks at 1Hz using ANSI `\033[5m` or manual toggle
- On keypress: mascot "walks" right off screen, pipeline panels slide in from left
- Transition takes 200ms (8 frames at 40ms each)
 
---
 
## 5. Main Layout — The Pipeline View
 
The core screen. Five panels arranged horizontally, connected by flow arrows.
Inspired by Mole's two-column card layout but adapted for a left-to-right pipeline.
 
### Full-Width Layout (≥120 cols)
 
```
┌─ INPUT ──────┐──▶┌─ TOKENS ─────┐──▶┌─ AST ─────────┐──▶┌─ EXEC ────────┐──▶┌─ RESULT ──────┐
│              │   │              │   │               │   │               │   │               │
│ $ ls -la |   │   │ ╭────╮╭──╮  │   │ PIPELINE      │   │ ⚡ fork()      │   │ total 48      │
│   grep foo   │   │ │ ls ││-l│  │   │ ├─CMD: ls -la │   │ ├─exec: ls    │   │ drwxr-xr-x  .│
│              │   │ ╰────╯╰──╯  │   │ │  ├─arg: -la │   │ ├─pipe[0]→[1]│   │ -rw-r--r-- fo│
│              │   │ ╭──╮╭────╮  │   │ └─CMD: grep   │   │ ⚡ fork()      │   │               │
│              │   │ │| ││grep│  │   │    └─arg: foo │   │ └─exec: grep  │   │ exit: 0 ✓     │
│ ▌            │   │ ╰──╯╰────╯  │   │               │   │               │   │ 0.003s         │
│              │   │ ╭─────╮     │   │               │   │               │   │               │
│              │   │ │ foo │     │   │               │   │               │   │               │
│              │   │ ╰─────╯     │   │               │   │               │   │               │
├──────────────┤   ├──────────────┤   ├───────────────┤   ├───────────────┤   ├───────────────┤
│ INS  Ln1 C14│   │ 6 tokens     │   │ 2 nodes       │   │ 2 procs       │   │ 2 lines  0.0s│
└──────────────┘   └──────────────┘   └───────────────┘   └───────────────┘   └───────────────┘
 
 [1]Input  [2]Tokens  [3]AST  [4]Exec  [5]Result    F5:Run  F8:Step  Tab:Next  ?:Help  q:Quit
```
 
### Panel Highlight Rules (from Mole's active-card pattern)
 
- **Active panel**: Solid border (`─│`) in Sky (#89dceb), title bolded
- **Inactive panels**: Dashed border (`╌╎`) in Overlay0 (#6c7086)
- **Error panel**: Border turns Red (#f38ba8), title gets `✗` prefix
- **Success panel**: Brief green flash on border (200ms), then returns to normal
- The bottom status row of each panel uses Surface0 (#313244) background — same as Mole's info rows
 
---
 
## 6. Panel 1 — Input (Line Editor)
 
The command input panel. This is where the user types shell commands.
 
### Features
 
```
┌─ INPUT ──────────────────────┐
│                              │
│  $ if [ -f "test.sh" ]; then │   ← Syntax-highlighted as you type
│      echo "found"            │   ← Multi-line with continuation
│    else                      │
│      echo "missing"          │
│    fi                        │
│  ▌                           │   ← Block cursor (Sky #89dceb)
│                              │
│  history: ↑↓  complete: Tab  │   ← Subtle hint text (Overlay0)
├──────────────────────────────┤
│  INS  Ln7  Col3   UTF-8     │   ← Status: mode, line, col, encoding
└──────────────────────────────┘
```
 
### Real-Time Syntax Highlighting
 
As the user types, the lexer runs continuously and applies colors:
 
```
  $ if [ -f "test.sh" ]; then echo "found"; fi
    ^^                   ^^   ^^^^            ^^
    Mauve(keyword)       Blue  Pink(cmd)      Mauve
         ^^ ^^^^^^^^^^^^
         Text  Green(string)
```
 
### Line Editor Behavior
 
| Feature | Implementation |
|---|---|
| Cursor | Blinking block, Sky (#89dceb), 530ms blink rate |
| Selection | Surface1 (#45475a) background on selected text |
| Matching brackets | Bold + underline matching `[`, `]`, `(`, `)`, `{`, `}` |
| Auto-indent | After `then`, `do`, `{`, indent next line by 4 spaces |
| History | ↑/↓ cycles history; Ctrl-R opens fuzzy-search overlay |
| Tab completion | Popup list below cursor, Mole-style highlight with `▶` marker |
| Paste bracket | Detect bracketed paste, don't execute on paste |
 
### Tab Completion Popup
 
```
  $ gr▌
       ┌────────────────────┐
       │ ▶ grep             │   ← Cyan ▶ marker (Mole's cursor style)
       │   groupadd         │
       │   groups           │
       │   grub-install     │
       └────────────────────┘
        4 matches (Tab/↓ to cycle)
```
 
---
 
## 7. Panel 2 — Tokenizer
 
Displays the lexer output as colored "chips" — each token is a rounded pill.
 
### Token Chip Design
 
```
┌─ TOKENS ─────────────────────────────────────┐
│                                               │
│  ╭──────╮ ╭────╮ ╭─────────╮ ╭──╮ ╭──────╮  │
│  │  if  │ │ [  │ │   -f    │ │ ] │ │ then │  │
│  │ KWD  │ │ OP │ │  WORD   │ │OP│ │ KWD  │  │
│  ╰──────╯ ╰────╯ ╰─────────╯ ╰──╯ ╰──────╯  │
│                                               │
│  ╭──────╮ ╭───────────╮ ╭──╮                  │
│  │ echo │ │ "found"   │ │ ;│                  │
│  │ WORD │ │  STRING   │ │OP│                  │
│  ╰──────╯ ╰───────────╯ ╰──╯                 │
│                                               │
│  ╭──────╮ ╭──────╮ ╭───────────╮ ╭──╮        │
│  │ else │ │ echo │ │ "missing" │ │ ;│        │
│  │ KWD  │ │ WORD │ │  STRING   │ │OP│        │
│  ╰──────╯ ╰──────╯ ╰───────────╯ ╰──╯        │
│                                               │
│  ╭──────╮                                     │
│  │  fi  │                                     │
│  │ KWD  │                                     │
│  ╰──────╯                                     │
├───────────────────────────────────────────────┤
│  12 tokens   0 errors                         │
└───────────────────────────────────────────────┘
```
 
### Token Type → Color Mapping
 
```
TOK_WORD        → Text (#cdd6f4)       plain arguments
TOK_PIPE        → Blue (#89b4fa)       |
TOK_AND         → Blue (#89b4fa)       &&
TOK_OR          → Blue (#89b4fa)       ||
TOK_SEMI        → Blue (#89b4fa)       ;
TOK_LPAREN      → Peach (#fab387)      (
TOK_RPAREN      → Peach (#fab387)      )
TOK_LBRACE      → Peach (#fab387)      {
TOK_RBRACE      → Peach (#fab387)      }
TOK_LESS        → Teal (#94e2d5)       <   (redirect)
TOK_GREATER     → Teal (#94e2d5)       >   (redirect)
TOK_DGREAT      → Teal (#94e2d5)       >>  (append)
TOK_DLESS       → Teal (#94e2d5)       <<  (heredoc)
TOK_AMP         → Blue (#89b4fa)       &   (background)
TOK_IF          → Mauve (#cba6f7)      if
TOK_THEN        → Mauve (#cba6f7)      then
TOK_ELSE        → Mauve (#cba6f7)      else
TOK_FI          → Mauve (#cba6f7)      fi
TOK_WHILE       → Mauve (#cba6f7)      while
TOK_FOR         → Mauve (#cba6f7)      for
TOK_DO          → Mauve (#cba6f7)      do
TOK_DONE        → Mauve (#cba6f7)      done
TOK_CASE        → Mauve (#cba6f7)      case
TOK_ESAC        → Mauve (#cba6f7)      esac
TOK_STRING      → Green (#a6e3a1)      "..." or '...'
TOK_VARIABLE    → Yellow (#f9e2af)     $VAR, ${VAR}
TOK_GLOB        → Teal (#94e2d5)       *.txt, [a-z]
TOK_COMMENT     → Overlay0 (#6c7086)   # comment
TOK_NEWLINE     → (not rendered)       line breaks
TOK_EOF         → (not rendered)       end of input
TOK_ERROR       → Red (#f38ba8)        unmatched quotes, bad escapes
```
 
### Chip Interaction
 
- Hovering a chip (j/k to move between tokens) highlights the corresponding source text in Panel 1
- Error tokens pulse red (Mole's danger animation pattern)
- Token count and error count in the status row update live
 
---
 
## 8. Panel 3 — Parser / AST
 
Displays the Abstract Syntax Tree as an indented tree with Mole-style tree-drawing characters.
 
### AST Tree View
 
```
┌─ AST ─────────────────────────────────────────┐
│                                                │
│  ◉ IF_COMMAND                                  │
│  ├── condition                                 │
│  │   └── SIMPLE_CMD                            │
│  │       ├── cmd: [                            │
│  │       ├── arg: -f                           │
│  │       ├── arg: "test.sh"                    │
│  │       └── arg: ]                            │
│  ├── then_branch                               │
│  │   └── SIMPLE_CMD                            │
│  │       ├── cmd: echo                         │
│  │       └── arg: "found"                      │
│  └── else_branch                               │
│      └── SIMPLE_CMD                            │
│          ├── cmd: echo                          │
│          └── arg: "missing"                     │
│                                                │
├────────────────────────────────────────────────┤
│  3 commands   1 if-block   depth: 2            │
└────────────────────────────────────────────────┘
```
 
### Node Type → Icon Mapping
 
```
NODE_COMMAND     ◉  (Pink)     Simple command
NODE_PIPELINE    ⇅  (Blue)     Piped commands
NODE_LIST        ◫  (Text)     Sequential commands (;, &&, ||)
NODE_IF          ◆  (Mauve)    If/then/else
NODE_WHILE       ⟳  (Mauve)    While loop
NODE_FOR         ⟳  (Mauve)    For loop
NODE_CASE        ◫  (Mauve)    Case statement
NODE_SUBSHELL    ◯  (Peach)    ( ... )
NODE_BRACE_GROUP ◯  (Peach)    { ... }
NODE_FUNCTION    ❊  (Teal)     Function definition
NODE_REDIRECT    ↪  (Teal)     I/O redirection
NODE_ASSIGNMENT  ▥  (Yellow)   VAR=value
```
 
### Tree Navigation
 
- j/k moves cursor between visible nodes
- Enter or l expands/collapses a node's children
- h collapses current node / moves to parent
- Selecting a node highlights its token range in Panel 2 and source text in Panel 1
 
### Pipeline Visualization (special case)
 
When the AST root is a pipeline, render a horizontal flow diagram instead of a tree:
 
```
┌─ AST ─────────────────────────────────────────┐
│                                                │
│  ⇅ PIPELINE                                   │
│                                                │
│  ╭─────────╮       ╭──────────╮                │
│  │  ls -la │──|──▶│ grep foo │                │
│  ╰─────────╯  pipe ╰──────────╯                │
│   stdin→       fd1→fd0  →stdout                │
│                                                │
├────────────────────────────────────────────────┤
│  2 stages   1 pipe                             │
└────────────────────────────────────────────────┘
```
 
---
 
## 9. Panel 4 — Executor
 
Shows the runtime execution: fork/exec calls, pipe setup, redirects, signal handling.
 
### Execution Trace View
 
```
┌─ EXEC ────────────────────────────────────────┐
│                                                │
│  ⚡ fork()  → pid 42371                        │
│  │  ├── pipe(fd[3,4])                          │
│  │  ├── dup2(fd[4], STDOUT)                    │
│  │  ├── close(fd[3])                           │
│  │  └── execvp("ls", ["ls","-la"])             │
│  │                                             │
│  ⚡ fork()  → pid 42372                        │
│  │  ├── dup2(fd[3], STDIN)                     │
│  │  ├── close(fd[4])                           │
│  │  └── execvp("grep", ["grep","foo"])         │
│  │                                             │
│  ├── waitpid(42371) → exit 0 ✓                 │
│  └── waitpid(42372) → exit 0 ✓                 │
│                                                │
│  ░░░░░░░░░░░░░░░░░░░░░░░░ 0.003s              │
│  ████████████████████████                      │
├────────────────────────────────────────────────┤
│  2 procs  1 pipe  exit: 0  0.003s              │
└────────────────────────────────────────────────┘
```
 
### Execution Animation
 
When a command runs, events appear line-by-line with a 50ms stagger (like Mole's progressive scan):
 
1. `⚡ fork()` appears with a brief flash (Yellow → normal in 150ms)
2. Child operations indent below
3. `execvp()` line appears in Pink (command color)
4. `waitpid()` result appears: Green `✓` for 0, Yellow for non-zero, Red `✗` for signal
5. Progress bar fills left-to-right during execution (Mole's gradient bar: `████████░░░░`)
 
### Throughput Sparkline
 
For long-running pipes, show a live throughput sparkline (Mole's I/O graph pattern):
 
```
  pipe throughput: ▁▂▃▅▇█▇▅▃▂▁  12.4 KB/s
```
 
### Signal Display
 
```
  waitpid(42371) → signal SIGSEGV (11) ✗     ← Red, bold
  ├── core dumped: yes
  └── backtrace available: bt command
```
 
---
 
## 10. Panel 5 — Result / Output
 
Shows the actual command output, plus exit status and timing.
 
### Output View
 
```
┌─ RESULT ──────────────────────────────────────┐
│                                                │
│  total 48                                      │
│  drwxr-xr-x  12 user staff  384 Feb 22 10:30 .│
│  -rw-r--r--   1 user staff  142 Feb 22 10:28 f│
│  -rw-r--r--   1 user staff 8192 Feb 22 09:15 t│
│  drwxr-xr-x   3 user staff   96 Feb 21 14:22 s│
│                                                │
│  ──────────────────────────────────            │
│  exit: 0 ✓   time: 0.003s   lines: 5          │
│                                                │
├────────────────────────────────────────────────┤
│  5 lines   324 bytes   0.003s                  │
└────────────────────────────────────────────────┘
```
 
### Exit Code Display (Mole's graduated-color pattern)
 
```
  exit: 0 ✓          → Green, bold         (success)
  exit: 1 ⚠          → Yellow              (general error)
  exit: 2 ⚠          → Peach               (misuse)
  exit: 126 ✗        → Red                 (cannot execute)
  exit: 127 ✗        → Red                 (not found)
  exit: 130           → Yellow              (SIGINT / Ctrl-C)
  exit: 139 ✗        → Red, bold           (SIGSEGV)
```
 
### Stderr Rendering
 
Stderr output is shown inline but with a left-border marker:
 
```
  ▐ grep: /nonexistent: No such file or directory    ← Red left-bar + Red text
```
 
### Scrollable Output
 
For large output, the panel becomes scrollable (Mole's viewport pattern):
- j/k or ↑/↓ scrolls line-by-line
- PgUp/PgDn scrolls by viewport height
- g/G jumps to top/bottom
- Scrollbar on right edge: `│` track with `█` thumb
 
---
 
## 11. Status Bar & Key Hints
 
Borrowed directly from Mole's bottom key-hint bar pattern.
 
### Bottom Bar (always visible, 2 lines)
 
```
 shelli v0.1.0 │ interactive │ last: 0.003s │ history: 142 │ pid: 42370
 [1]Input [2]Tok [3]AST [4]Exec [5]Out   F5:Run  F8:Step  Tab:▶  Ctrl-D:Quit  ?:Help
```
 
- Line 1: Status info in Subtext1 (#bac2de) on Surface0 (#313244) background
- Line 2: Key hints in Overlay0 (#6c7086), active keys in Text (#cdd6f4)
- Separator `│` in Overlay0
- Current mode indicator: `interactive` (Green) or `debug` (Yellow) or `stepping` (Peach)
 
### Contextual Hints
 
Key hints change based on the active panel (Mole does this for different views):
 
```
Panel 1 (Input):   Enter:Run  ↑↓:History  Tab:Complete  Ctrl-R:Search  Esc:Cancel
Panel 2 (Tokens):  j/k:Navigate  Enter:Inspect  /:Filter  y:Copy token
Panel 3 (AST):     j/k:Navigate  h/l:Collapse/Expand  Enter:Detail  t:Toggle tree/flow
Panel 4 (Exec):    j/k:Scroll  b:Set breakpoint  c:Continue  s:Step-into
Panel 5 (Result):  j/k:Scroll  y:Copy output  /:Search  w:Wrap toggle
```
 
---
 
## 12. Debug / Step-Through Mode
 
The signature Shelli feature. Press F8 to enter debug mode.
 
### Debug Mode Layout
 
The same 5 panels, but with a debug overlay:
 
```
┌─ INPUT ──────┐──▶┌─ TOKENS ─────┐──▶┌─ AST ─────────┐──▶┌─ EXEC ────────┐──▶┌─ RESULT ──────┐
│              │   │              │   │               │   │               │   │               │
│ $ ls | grep f│   │ ╭────╮ ◆    │   │ ◆ PIPELINE    │   │ (waiting...)  │   │ (waiting...)  │
│              │   │ │ ls │      │   │ ├─CMD: ls     │   │               │   │               │
│              │   │ ╰────╯      │   │ └─CMD: grep   │   │               │   │               │
│              │   │ ╭──╮        │   │               │   │               │   │               │
│              │   │ │| │ ◀──────│───│── current     │   │               │   │               │
│              │   │ ╰──╯        │   │               │   │               │   │               │
│              │   │ ╭──────╮    │   │               │   │               │   │               │
│              │   │ │ grep │    │   │               │   │               │   │               │
│              │   │ ╰──────╯    │   │               │   │               │   │               │
│              │   │ ╭─────╮     │   │               │   │               │   │               │
│              │   │ │  f  │     │   │               │   │               │   │               │
│              │   │ ╰─────╯     │   │               │   │               │   │               │
└──────────────┘   └──────────────┘   └───────────────┘   └───────────────┘   └───────────────┘
 
 DEBUG ◆ Step 2/4: Tokenize ▶ PIPE          n:Next  p:Prev  c:Continue  q:Quit debug  F5:Run all
```
 
### Debug Steps
 
The pipeline stages become discrete steps:
 
```
Step 1:  LEXER   ── Tokenize the input character by character
Step 2:  PARSER  ── Build AST from token stream
Step 3:  EXPAND  ── Perform word expansions ($VAR, ~, globs, etc.)
Step 4:  EXECUTE ── Fork, setup pipes/redirects, exec
Step 5:  WAIT    ── Collect exit statuses
Step 6:  RESULT  ── Display output
```
 
### Step-Through Animation
 
When stepping through the lexer:
 
```
  Input:  l  s     |     g  r  e  p     f
          ^
          ╰── cursor advances one character at a time
              each character highlights briefly in Sky (#89dceb)
              when a token boundary is found, a new chip appears in Panel 2
```
 
When stepping through the parser:
 
```
  Token stream:  [ls]  [|]  [grep]  [f]
                  ^^^
                  ╰── parser consumes tokens one at a time
                      AST grows visually in Panel 3 as nodes are added
```
 
When stepping through expansion:
 
```
  Before:  $HOME/*.txt
  After:   /Users/name/notes.txt /Users/name/todo.txt
 
  Expansion shown as animated transformation:
  $HOME      ──▶  /Users/name        (variable expansion, Yellow flash)
  *.txt      ──▶  notes.txt todo.txt (glob expansion, Teal flash)
```
 
### Breakpoint Indicators
 
- `◆` diamond in Mauve appears next to tokens/nodes with breakpoints
- Set with `b` key on any token or AST node
- Breakpoint list shown in debug status bar:
  ```
  ◆ Breakpoints: token#3(PIPE) ast#1(PIPELINE.cmd[1])
  ```
 
---
 
## 13. Animation System
 
All animations follow Mole's patterns: purposeful, tied to real state, never gratuitous.
 
### Animation Catalog
 
| Animation | Trigger | Duration | Style |
|---|---|---|---|
| Token appear | Lexer produces token | 100ms | Fade in (dim → bright) |
| AST node grow | Parser creates node | 150ms | Slide down + fade in |
| Pipeline flow | Data passes through pipe | Continuous | `···▶` dots travel left-to-right |
| Fork flash | fork() called | 200ms | Yellow flash on `⚡` |
| Exec highlight | execvp() called | 300ms | Pink pulse on command name |
| Exit badge | Process exits | 200ms | Badge appears with color |
| Error shake | Syntax error detected | 300ms | Panel border flickers Red 3× |
| Progress bar | Command executing | Continuous | `████░░░░` fills left-to-right |
| Cursor blink | Always in input panel | 530ms | Block cursor toggles visible/invisible |
| Sparkline | Pipe throughput | 200ms update | `▁▂▃▅▇` bars update live |
| Panel transition | Tab between panels | 120ms | Border color transitions smoothly |
| Debug step | Step-through advance | 80ms | Highlight slides to next element |
 
### Animation Frame Timing
 
Following Mole's cooperative spinner pattern:
- Animations run at 40ms tick (25fps) — smooth without being CPU-heavy
- Long-running animations (progress bar, sparkline) update at 200ms intervals
- Cursor blink at 530ms (standard terminal rate)
- All animations can be disabled with `--no-animate` flag or `SHELLI_NO_ANIMATE=1`
 
---
 
## 14. Widget Catalog
 
Reusable widgets derived from Mole's component library:
 
### 14.1 Gradient Progress Bar
 
```
  Executing...  ████████████░░░░░░░░░░░░  48%  0.012s
                ▔▔▔▔▔▔▔▔▔▔▔▔
                Green→Yellow→Red as time increases (Mole's threshold pattern)
 
  Thresholds:
    < 1s    → Green fill   (#a6e3a1)
    1–5s    → Yellow fill  (#f9e2af)
    > 5s    → Red fill     (#f38ba8)
```
 
### 14.2 Token Chip
 
```
  ╭─────────╮
  │  TOKEN  │   ← Background: token-type color at 20% opacity
  │  TYPE   │   ← Label: Overlay0, small caps
  ╰─────────╯
 
  Variants:
  - Normal:    single border
  - Selected:  double border + Sky background
  - Error:     Red border + pulsing
  - Breakpoint: ◆ prefix
```
 
### 14.3 Tree View
 
```
  ├── Node label              ← Prefix icons from node-type mapping
  │   ├── Child               ← Collapsible with h/l keys
  │   │   └── Leaf            ← Leaf nodes have no expand indicator
  │   └── Child (collapsed) ▶ ← ▶ indicates hidden children
  └── Last node
```
 
### 14.4 Sparkline Graph
 
```
  ▁▂▃▄▅▆▇█▇▆▅▄▃▂▁   12.4 KB/s peak
  ╰─── 30 samples, 200ms intervals ───╯
 
  Color: matches pipeline stage color
  Height: 8 levels (▁▂▃▄▅▆▇█) mapped to 0%–100% of peak
```
 
### 14.5 Scrollable Viewport
 
```
  ┌── content ────────────── ▲ ─┐    ← Scroll indicator
  │  visible line 1          █  │    ← Thumb position
  │  visible line 2          █  │
  │  visible line 3          ░  │    ← Track
  │  visible line 4          ░  │
  └──────────────────────── ▼ ──┘
 
  Controls: j/k, PgUp/PgDn, g/G, mouse scroll (if terminal supports)
  From Mole's defaultViewport = 12 lines pattern
```
 
### 14.6 Popup / Overlay
 
```
  ┌─ EXPANSION DETAIL ─────────────────┐
  │                                     │
  │  Input:    $HOME/*.txt              │
  │                                     │
  │  Step 1:   Variable expansion       │
  │            $HOME → /Users/name      │
  │                                     │
  │  Step 2:   Glob expansion           │
  │            *.txt → notes.txt        │
  │                      todo.txt       │
  │                                     │
  │  Result:   /Users/name/notes.txt    │
  │            /Users/name/todo.txt     │
  │                                     │
  │            Esc:Close  Tab:Next      │
  └─────────────────────────────────────┘
 
  - Centered on screen
  - Background: Base (#1e1e2e) with Surface0 border
  - Dims panels behind it (Mole's overlay pattern)
```
 
### 14.7 Notification Toast
 
```
  ╭──────────────────────────────────╮
  │ ✓ Command completed in 0.003s   │   ← Green border, fades after 2s
  ╰──────────────────────────────────╯
 
  ╭──────────────────────────────────╮
  │ ✗ Syntax error at position 14   │   ← Red border, persists until dismissed
  ╰──────────────────────────────────╯
 
  Position: top-right corner, stacks downward
  Animation: slides in from right (Mole's toast pattern)
```
 
---
 
## 15. Interaction & Keybindings
 
Full vim-style navigation inherited from Mole:
 
### Global Keys
 
```
Tab          → Cycle to next panel (forward)
Shift-Tab    → Cycle to previous panel (backward)
1-5          → Jump to panel by number
?            → Toggle help overlay
q            → Quit (with confirmation if command is running)
Ctrl-C       → Send SIGINT to running command / cancel current action
Ctrl-D       → Quit immediately
Ctrl-L       → Redraw screen
F5           → Run command (from any panel)
F8           → Toggle debug/step-through mode
```
 
### Panel Navigation (when inside a panel)
 
```
j / ↓        → Move down (next token, next AST node, scroll output)
k / ↑        → Move up
h / ←        → Collapse AST node / move to parent / scroll left
l / →        → Expand AST node / enter detail / scroll right
g            → Jump to first item
G            → Jump to last item
/            → Search within panel
n            → Next search match
N            → Previous search match
y            → Yank (copy) current item to clipboard
Enter        → Inspect / drill-down / confirm
Esc          → Back / close popup / cancel search
```
 
### Input Panel Keys
 
```
Enter        → Execute command
↑ / ↓        → History navigation
Tab          → Tab completion
Ctrl-A       → Move to start of line
Ctrl-E       → Move to end of line
Ctrl-W       → Delete word backward
Ctrl-U       → Delete to start of line
Ctrl-K       → Delete to end of line
Ctrl-R       → Reverse history search
Alt-B        → Move word backward
Alt-F        → Move word forward
```
 
### Debug Mode Keys
 
```
n / F10      → Step to next stage
p            → Step to previous stage (rewind)
s / F11      → Step into (character-level in lexer, token-level in parser)
c / F5       → Continue to next breakpoint or end
b            → Toggle breakpoint on current item
B            → List all breakpoints
x            → Clear all breakpoints
i            → Inspect current value (opens popup)
```
 
---
 
## 16. Responsive Layout
 
Following Mole's SIGWINCH handling and adaptive layout:
 
### Width Breakpoints
 
```
≥ 160 cols:  Full 5-panel horizontal layout with flow arrows
             Each panel ≥ 30 cols
 
≥ 120 cols:  Compact 5-panel horizontal (default target)
             Each panel ≥ 22 cols, shorter labels
 
≥ 80 cols:   3-panel view (Input+Tokens | AST | Exec+Result)
             Merged panels use tabbed sub-views
 
< 80 cols:   Stacked single-panel view
             Tab cycles through all 5 panels full-width
             Active panel gets all vertical space
```
 
### Height Breakpoints
 
```
≥ 40 rows:   Full layout with all chrome
≥ 24 rows:   Compact: hide per-panel status rows, keep bottom bar
< 24 rows:   Minimal: single panel, no decorations, just content + 1 status line
```
 
### Resize Behavior
 
- On SIGWINCH: recalculate layout immediately (no delay)
- Smooth reflow: content repositions without flicker
- Panel proportions: Input gets 20%, Tokens 20%, AST 25%, Exec 20%, Result 15%
- Minimum viable: 80×24 (standard terminal)
- CJK-aware text truncation (from Mole's `runewidth` approach)
 
---
 
## 17. Implementation Architecture (C99)
 
Shelli is written in C99. Here's how to implement the TUI layer:
 
### Terminal I/O Layer
 
```c
// Raw mode setup (like Mole's alt-screen pattern)
struct termios orig_termios;
 
void tui_init(void) {
    // Save original terminal state
    tcgetattr(STDIN_FILENO, &orig_termios);
 
    // Enter raw mode
    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;  // 100ms timeout for non-blocking reads
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
 
    // Enter alt screen buffer (Mole pattern)
    write(STDOUT_FILENO, "\033[?1049h", 8);   // Alt screen
    write(STDOUT_FILENO, "\033[?25l", 6);      // Hide cursor initially
    write(STDOUT_FILENO, "\033[2J", 4);        // Clear screen
}
 
void tui_cleanup(void) {
    write(STDOUT_FILENO, "\033[?25h", 6);      // Show cursor
    write(STDOUT_FILENO, "\033[?1049l", 8);    // Exit alt screen
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}
```
 
### Color System
 
```c
// Catppuccin Mocha palette as 24-bit ANSI escapes
#define COL_BASE      "\033[38;2;30;30;46m"
#define COL_SURFACE0  "\033[38;2;49;50;68m"
#define COL_TEXT      "\033[38;2;205;214;244m"
#define COL_MAUVE     "\033[38;2;203;166;247m"
#define COL_GREEN     "\033[38;2;166;227;161m"
#define COL_YELLOW    "\033[38;2;249;226;175m"
#define COL_BLUE      "\033[38;2;137;180;250m"
#define COL_PINK      "\033[38;2;245;194;231m"
#define COL_RED       "\033[38;2;243;139;168m"
#define COL_PEACH     "\033[38;2;250;179;135m"
#define COL_TEAL      "\033[38;2;148;226;213m"
#define COL_SKY       "\033[38;2;137;220;235m"
#define COL_OVERLAY0  "\033[38;2;108;112;134m"
#define COL_RESET     "\033[0m"
 
// Background variants (replace 38 with 48)
#define BG_BASE       "\033[48;2;30;30;46m"
#define BG_SURFACE0   "\033[48;2;49;50;68m"
// ... etc.
```
 
### Render Loop (Mole's MVU-inspired pattern in C)
 
```c
typedef struct {
    int active_panel;       // 0-4
    int debug_mode;         // 0 or 1
    int debug_step;         // current step in debug
    int width, height;      // terminal dimensions
    // ... per-panel scroll positions, cursor positions, etc.
} TuiState;
 
// Model-View-Update cycle (adapted from Mole's Bubbletea pattern)
void tui_run(ShellContext *ctx) {
    TuiState state = {0};
    tui_get_size(&state.width, &state.height);
 
    // Install SIGWINCH handler
    signal(SIGWINCH, handle_resize);
 
    while (1) {
        // VIEW: render current state to buffer
        char *buf = render_frame(&state, ctx);
        write(STDOUT_FILENO, buf, strlen(buf));
        free(buf);
 
        // UPDATE: read input and update state
        int key = read_key();
        if (key == 'q') break;
        update_state(&state, ctx, key);
    }
}
```
 
### Double Buffering
 
```c
// Avoid flicker: render to buffer, then flush once (Mole's
// lipgloss-style approach adapted for C)
typedef struct {
    char *data;
    size_t len;
    size_t cap;
} RenderBuf;
 
void buf_write(RenderBuf *b, const char *s) {
    size_t slen = strlen(s);
    if (b->len + slen >= b->cap) {
        b->cap = (b->cap + slen) * 2;
        b->data = realloc(b->data, b->cap);
    }
    memcpy(b->data + b->len, s, slen);
    b->len += slen;
}
 
void buf_flush(RenderBuf *b) {
    write(STDOUT_FILENO, b->data, b->len);
    b->len = 0;
}
```
 
---
 
## 18. Appendix — ASCII Mockups
 
### A. Welcome → Pipeline Transition
 
```
  FRAME 1 (Splash):                    FRAME 8 (Pipeline ready):
 
  ┌────────────────────────┐           ┌──┐┌──┐┌──┐┌──┐┌──┐
  │                        │           │IN││TK││AS││EX││RE│
  │        ___             │    ──▶    │  ││  ││  ││  ││  │
  │       | o_o |          │           │  ││  ││  ││  ││  │
  │       shelli           │           │  ││  ││  ││  ││  │
  │                        │           └──┘└──┘└──┘└──┘└──┘
  └────────────────────────┘           └── key hints ──────┘
```
 
### B. Error State
 
```
┌─ INPUT ──────┐──▶┌─ TOKENS ─────┐──✗  (pipeline stops here)
│              │   │              │
│ $ echo "unt │   │ ╭──────╮     │   ┌─ ERROR ─────────────────────┐
│             ▌│   │ │ echo │     │   │                             │
│              │   │ ╰──────╯     │   │  ✗ Syntax Error             │
│              │   │ ╭──────────╮ │   │                             │
│              │   │ │ "unt...  │ │   │  Unterminated string at     │
│              │   │ │ ✗ ERROR  │ │   │  line 1, column 7          │
│              │   │ ╰──────────╯ │   │                             │
│              │   │              │   │  Expected: closing "        │
│              │   │              │   │  Got: end of input          │
│              │   │              │   │                             │
└──────────────┘   └──────────────┘   └─────────────────────────────┘
```
 
### C. Word Expansion Detail (Debug Mode Popup)
 
```
                    ┌─ EXPANSION ───────────────────────────┐
                    │                                       │
                    │  Input:   ~/$DIR/*.{c,h}              │
                    │                                       │
                    │  Step 1 — Tilde expansion:            │
                    │    ~ → /home/user                     │
                    │                                       │
                    │  Step 2 — Variable expansion:         │
                    │    $DIR → src                         │
                    │                                       │
                    │  Step 3 — Brace expansion:            │
                    │    *.{c,h} → *.c *.h                  │
                    │                                       │
                    │  Step 4 — Glob expansion:             │
                    │    /home/user/src/*.c →               │
                    │      main.c  lexer.c  parser.c        │
                    │    /home/user/src/*.h →               │
                    │      main.h  lexer.h  parser.h        │
                    │                                       │
                    │  Final: 6 words                       │
                    │                                       │
                    │          Esc:Close   ↑↓:Scroll        │
                    └───────────────────────────────────────┘
```
 
### D. Help Overlay
 
```
┌─ SHELLI HELP ─────────────────────────────────────────────────────────┐
│                                                                       │
│  NAVIGATION                    EXECUTION                              │
│  ──────────                    ─────────                              │
│  Tab / Shift-Tab  Cycle panel  F5          Run command                │
│  1-5              Jump panel   F8          Toggle debug mode          │
│  j/k              Move in list Ctrl-C      Send SIGINT                │
│  h/l              Collapse/Exp Ctrl-D      Quit                       │
│  g/G              Top/Bottom                                          │
│  /                Search       DEBUG                                  │
│  Esc              Back/Cancel  ─────                                  │
│                                n/F10       Next step                  │
│  INPUT PANEL                   s/F11       Step into                  │
│  ───────────                   c           Continue                   │
│  Enter    Execute command      b           Toggle breakpoint          │
│  ↑↓       History              B           List breakpoints           │
│  Tab      Complete             x           Clear breakpoints          │
│  Ctrl-R   Search history       i           Inspect value              │
│                                                                       │
│                        Press ? or Esc to close                        │
└───────────────────────────────────────────────────────────────────────┘
```
 
---
 
## Summary of Mole Patterns Applied
 
| Mole Pattern | Where in Shelli |
|---|---|
| ASCII mascot with idle animation | Shell mascot on splash screen with blinking eyes |
| Catppuccin Mocha hex palette | All colors, token highlighting, exit code badges |
| Gradient progress bars (█▓▒░) | Execution timer bar with time-based color thresholds |
| Sparkline graphs (▁▂▃▄▅▆▇█) | Pipe throughput visualization |
| Card layout with dashed borders | Active/inactive panel border styles |
| Bottom key-hint bar (gray) | Contextual key hints that change per active panel |
| Cyan ▶ cursor for selection | Tab completion popup, token/node navigation |
| Category icons (◉ ◫ ▥ ⇅ ◪ ❊) | AST node type icons |
| Spinner with cooperative stop | Animation system with 40ms tick, cancellable |
| Alt-screen buffer | Full-screen TUI with clean restore on exit |
| Vim keybindings (h/j/k/l) | All panel navigation, tree traversal, scrolling |
| Progressive scan with stagger | Execution events appear line-by-line with 50ms delay |
| Color thresholds (green→yellow→red) | Exit codes, execution time, error severity |
| SIGWINCH resize handling | Responsive layout with 4 width breakpoints |
| Double-buffered rendering | Write to buffer, flush once per frame |
| Viewport scrolling with scrollbar | Output panel, long token lists, deep AST trees |
 
---
 
*This document is the complete TUI specification for Shelli. Every visual element,
animation, color, keybinding, and layout rule is defined above. Use it as the
blueprint for implementing `tui.c`, `render.c`, `theme.c`, and `input.c`.*
