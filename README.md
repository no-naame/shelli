<h1 align="center">
  <br>
  <code>shelli</code>
  <br>
</h1>

<h3 align="center">See how shells actually work. Step by step. Beautifully.</h3>

<p align="center">
  <img src="https://img.shields.io/badge/written_in-C99-blue?style=flat-square" alt="C99">
  <img src="https://img.shields.io/badge/theme-Catppuccin_Mocha-b4befe?style=flat-square" alt="Catppuccin Mocha">
  <img src="https://img.shields.io/badge/platform-macOS_%7C_Linux-lightgrey?style=flat-square" alt="Platform">
  <img src="https://img.shields.io/badge/lines-11%2C400%2B-green?style=flat-square" alt="Lines of Code">
</p>

<p align="center">
  <strong>shelli</strong> is an educational shell that shows you <em>exactly</em> what happens inside your computer when you run a command. Type <code>ls | grep foo</code> and watch the tokens appear, the AST build, variables expand, processes fork, and the result arrive &mdash; all in a stunning terminal UI.
</p>

---

## The 5-Stage Pipeline

Every command you type flows through five visualized stages:

```
  ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
  ┃                                                            ┃
  ┃   ◉ Tokenize ━━━━ ◉ AST ━━━━ ◉ Expand ━━━━ ◉ Execute ━━━━ ◉ Result   ┃
  ┃                                                            ┃
  ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
```

| Stage | What you see |
|-------|-------------|
| **1. Tokenize** | The lexer scans your input character-by-character and breaks it into tokens: `WORD`, `PIPE`, `REDIRECT`, `STRING`, `KEYWORD`, ... |
| **2. AST** | The parser consumes the token stream and builds an Abstract Syntax Tree with tree-drawing connectors (`├──`, `└──`) showing pipelines, commands, arguments, and redirections. |
| **3. Expand** | Variable expansion (`$HOME` -> `/Users/you`), tilde expansion (`~`), command substitution (`$(pwd)`), arithmetic (`$((1+2))`), and glob matching (`*.c`). Each expansion shown as `original --> expanded`. |
| **4. Execute** | Watch the actual system calls: `pipe()`, `fork()`, `dup2()`, `execvp()`, `waitpid()` -- with colored prefixes showing process creation, I/O wiring, and program execution. |
| **5. Result** | Command output, exit code with human-readable description (0 = success, 127 = command not found, 130 = SIGINT, ...), and an educational "What happened?" summary. |

## Features

### Shell

- **Pipes** &mdash; `ls | grep foo | wc -l`
- **Input/output redirection** &mdash; `sort < in.txt > out.txt`
- **Append** &mdash; `echo more >> log.txt`
- **Heredoc** &mdash; `cat << EOF`
- **Conditional execution** &mdash; `make && echo ok || echo fail`
- **Background jobs** &mdash; `sleep 10 &`, `jobs`, `fg`, `bg`
- **Semicolon chaining** &mdash; `echo a; echo b; echo c`
- **Quoting** &mdash; `"double"`, `'single'`, `escape\ spaces`
- **Variable expansion** &mdash; `$HOME`, `$?`, `$PATH`
- **Command substitution** &mdash; `echo $(whoami)`
- **Arithmetic expansion** &mdash; `echo $((2 + 3 * 4))`
- **Tilde expansion** &mdash; `cd ~`
- **Glob matching** &mdash; `ls *.c`
- **Control flow** &mdash; `if`/`then`/`else`/`fi`, `while`/`do`/`done`, `for`/`in`/`do`/`done`, `until`
- **Functions** &mdash; `greet() { echo "hello $1"; }` with `local` variables
- **Negation** &mdash; `! false && echo ok`
- **Subshells** &mdash; `(cd /tmp && ls)`
- **History** &mdash; persistent across sessions, `!!` and `!n` recall, `history` builtin
- **Startup file** &mdash; `~/.shellirc` sourced on launch

### 16 Built-in Commands

| Command | Description |
|---------|-------------|
| `cd [dir]` | Change directory (defaults to `$HOME`) |
| `pwd` | Print working directory |
| `echo [-n] [-e]` | Print arguments (`-n` no newline, `-e` escape sequences) |
| `export VAR=val` | Set environment variable |
| `unset VAR` | Remove variable |
| `type cmd` | Identify command type (builtin, function, executable) |
| `test` / `[` | Conditional expressions (`-f`, `-d`, `-e`, `-z`, `-n`, `=`, `!=`, `-eq`, `-lt`, ...) |
| `true` / `false` | Return exit code 0 / 1 |
| `history` | Show command history |
| `source` / `.` | Execute commands from a file |
| `local VAR=val` | Declare local variable inside a function |
| `jobs` | List background jobs |
| `fg [%n]` | Bring job to foreground |
| `bg [%n]` | Resume job in background |
| `exit [code]` | Exit the shell |
| `help` | Show help |

### Terminal UI

- **Double-buffered rendering** &mdash; diff-based flush, zero flicker
- **Catppuccin Mocha palette** &mdash; 13 carefully chosen colors from the [Catppuccin](https://github.com/catppuccin/catppuccin) project
- **Neon gradient logo** &mdash; 6-color ASCII art splash screen (pink -> purple -> lavender -> blue -> cyan -> teal)
- **Animated mascot** &mdash; mood-reactive block character with 5 expressions and 2-frame breathing animation
- **Smart suggestions** &mdash; history-aware command completion with fuzzy matching and frequency ranking
- **"Try These" presets** &mdash; curated example commands when input is empty
- **Syntax highlighting** &mdash; real-time coloring as you type (commands, keywords, strings, variables, operators)
- **Unicode box drawing** &mdash; rounded corners (`╭╮╰╯`), heavy frames (`┏┓┗┛`), tree connectors (`├──`, `└──`)
- **Full alt-screen** &mdash; takes over the terminal like vim; restores cleanly on exit
- **Braille spinners** &mdash; `⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏`

## Quick Start

```bash
git clone https://github.com/no-naame/shelli.git
cd shelli
make
./shelli
```

## Usage

```bash
./shelli                        # Launch with splash screen
./shelli --no-splash            # Skip splash, go straight to input
./shelli --anim-speed=fast      # Faster animations (none | fast | normal | slow)
./shelli --help                 # Show usage
```

### Keyboard Shortcuts

**Input page:**

| Key | Action |
|-----|--------|
| `Enter` | Run command / select preset |
| `Tab` | Accept suggestion / tab-complete |
| `Up` / `Down` | Browse history / navigate suggestions |
| `Ctrl+A` / `Ctrl+E` | Jump to start / end of line |
| `Ctrl+K` | Kill to end of line |
| `Ctrl+U` | Kill to start of line |
| `Ctrl+W` | Kill previous word |
| `Ctrl+L` | Force redraw |
| `Ctrl+C` | Clear line |
| `Ctrl+D` | Exit (on empty line) |
| `Esc` | Dismiss suggestions |

**Stage browser:**

| Key | Action |
|-----|--------|
| `Left` / `Right` | Previous / next stage |
| `Up` / `Down` | Scroll content |
| `Enter` | New command |
| `Q` | Quit |

## Install System-Wide

```bash
sudo make install                    # /usr/local/bin/shelli
make PREFIX=~/.local install         # Or custom prefix
make uninstall                       # Remove
```

## Architecture

```
src/
├── main.c              REPL loop, CLI argument parsing, .shellirc loading
├── lexer.c/h           Tokenizer (15 token types, quoted strings, heredoc markers)
├── parser.c/h          Recursive-descent parser producing a typed AST
├── ast.c/h             AST node types: command, pipeline, list, if, while, for, function, subshell, not
├── expand.c/h          Word expansion engine (tilde, variable, command subst, arithmetic, glob)
├── executor.c/h        fork/exec/pipe/redirect, capture mode, AST walker
├── builtins.c/h        16 built-in commands
├── test_builtin.c/h    POSIX test/[ implementation (-f, -d, -e, -z, -n, =, !=, -eq, ...)
├── variables.c/h       Shell variable storage with scoping (local/global/env)
├── functions.c/h       Function definition and lookup
├── jobs.c/h            Background job tracking (jobs, fg, bg)
├── util.c/h            Dynamic buffer, string helpers
│
└── tui/
    ├── tui.h               Public API: 580-line header defining all types and interfaces
    ├── tui_core.c           Raw mode, alt screen, signal handling, terminal queries
    ├── tui_input.c          Full-screen line editor, syntax highlighting, history, tab completion
    ├── tui_render.c         Double-buffered RenderBuf with diff-based flushing
    ├── tui_widgets.c        Rounded boxes, heavy boxes, spinners, progress bars, stage indicators
    ├── tui_theme.c          Catppuccin Mocha color palette, gradient helpers
    ├── tui_logo.c           6-line neon gradient ASCII art logo with glow effects
    ├── tui_anim.c           Easing functions, blocking animations (fade-in, typewriter)
    ├── tui_icons.c          Nerd Font icon registry with graceful ASCII fallbacks
    ├── tui_mascot.c         5-mood animated block mascot with double-line frame and breathing
    ├── tui_pages.c          Welcome page, stage browser, StageData population helpers
    ├── tui_suggest.c        Fuzzy suggestion engine with Levenshtein distance and frequency ranking
    ├── tui_stage_tokenize.c Token chip layout with color-coded type labels
    ├── tui_stage_ast.c      Tree-drawn AST with connectors and colored node types
    ├── tui_stage_expand.c   Before/after expansion pairs with arrow connectors
    ├── tui_stage_execute.c  System call trace with prefix icons (> fork, ~ I/O, * exec, . wait)
    └── tui_stage_result.c   Output display, exit code decoder, error presentation
```

**25 source files, 11 headers, 11,400+ lines of C99.**

## Color Palette

shelli uses [Catppuccin Mocha](https://github.com/catppuccin/catppuccin) &mdash; a warm, eye-friendly dark theme:

| Color | Hex | Usage |
|-------|-----|-------|
| Base | `#1e1e2e` | Background |
| Text | `#cdd6f4` | Primary text |
| Pink | `#f5c2e7` | Commands, keywords |
| Blue | `#89b4fa` | Titles, AST connectors |
| Green | `#a6e3a1` | Strings, success |
| Peach | `#fab387` | Execution stage |
| Red | `#f38ba8` | Errors |
| Yellow | `#f9e2af` | Variables, expansion stage |
| Teal | `#94e2d5` | Prompt, redirect tokens |
| Lavender | `#b4befe` | Mascot frame, accents |
| Mauve | `#cba6f7` | Control flow keywords |

## The Mascot

A mood-reactive companion built from block characters inside a double-line frame:

```
╔════════╗
║░░▀░▀░░║     NORMAL   eyes: ▀░▀   mouth: ▽   chest: ♥♥♥
║░░░▽░░░║     THINKING eyes: ▓░▓   mouth: ─   chest: ✦✦✦
║░░♥♥♥░░║     HAPPY    eyes: ▀░▀   mouth: ▿   chest: ♥♥♥
╚══╦══╦══╝     SAD      eyes: ▄░▄   mouth: ▵   chest: ░░░
┈┈┈╩┈┈╩┈┈     WORKING  eyes: ▒░▒   mouth: ═   chest: ⚡⚡⚡
```

It breathes (legs alternate between two frames) and changes mood based on the current stage.

## Requirements

- C99 compiler (gcc, clang, or cc)
- POSIX-compliant OS (macOS, Linux)
- Terminal with 256-color support
- UTF-8 locale for box-drawing and block characters

## License

MIT License &mdash; see [LICENSE](LICENSE) for details.

## Acknowledgments

- Theme: [Catppuccin](https://github.com/catppuccin/catppuccin)
- UI inspiration: [LazyVim](https://github.com/LazyVim/LazyVim) and [Charm](https://charm.sh/)
- Built to demystify what happens between pressing Enter and seeing output
