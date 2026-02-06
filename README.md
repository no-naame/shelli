# shelli

A beautiful, educational shell that visualizes how shells work - step by step.

![shelli demo](https://img.shields.io/badge/TUI-Catppuccin-89b4fa?style=flat-square)
![C99](https://img.shields.io/badge/C-C99-blue?style=flat-square)
![Platform](https://img.shields.io/badge/Platform-macOS%20%7C%20Linux-lightgrey?style=flat-square)

## What is shelli?

shelli is an educational shell implementation with a professional-grade terminal UI inspired by [LazyVim](https://github.com/LazyVim/LazyVim) and [Charm](https://charm.sh/). It visualizes each stage of shell command processing:

```
╭──────────────────────────── shelli ─────────────────────────────╮
│                                                                  │
│  ╭─ INPUT ──────────────────────────────────────────────────╮   │
│  │ ❯ ls -la | grep foo                                      │   │
│  ╰──────────────────────────────────────────────────────────╯   │
│                                                                  │
│  ● INPUT ─── ✓ TOKENIZE ─── ✓ PARSE ─── ● EXECUTE ─── ○ RESULT  │
│                                                                  │
│  ╭─ TOKENIZE ──────────╮  ╭─ PARSE ─────────────────────╮       │
│  │  [WORD] "ls"        │  │  cmd[0]: ls -la             │       │
│  │  [WORD] "-la"       │  │     ↓ pipe                  │       │
│  │  [PIPE]             │  │  cmd[1]: grep foo           │       │
│  │  [WORD] "grep"      │  │                             │       │
│  │  [WORD] "foo"       │  │                             │       │
│  ╰─────────────────────╯  ╰─────────────────────────────╯       │
│                                                                  │
│  ╭─ EXECUTE ────────────────────────────────────────────────╮   │
│  │  ⠋ fork() → pid 1234 (ls)                                │   │
│  │  ⠙ fork() → pid 1235 (grep)                              │   │
│  │    pipe: 1234 stdout ──► 1235 stdin                      │   │
│  ╰──────────────────────────────────────────────────────────╯   │
│                                                                  │
╰──────────────────────────────────────────────────────────────────╯
```

## Features

- **Step-by-step visualization** - Watch tokens appear one by one, see the AST build, observe fork/exec calls
- **Animated UI** - Smooth 150ms animations with Braille spinners (⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏)
- **Catppuccin Mocha theme** - Beautiful, eye-friendly color palette
- **Full terminal takeover** - Uses alternate screen buffer like vim/nvim
- **Real shell functionality** - Pipes, redirects, builtins all work
- **Debug mode** - Step through each stage manually with `--debug`

## Installation

### Build from source

```bash
git clone https://github.com/no-naame/shelli.git
cd shelli
make
./shelli
```

### Install system-wide

```bash
make install          # Installs to /usr/local/bin
make PREFIX=~/.local install  # Or custom prefix
```

## Usage

```bash
./shelli              # Start with splash screen
./shelli --no-splash  # Skip the splash screen
./shelli --debug      # Step-by-step mode (press Enter between stages)
./shelli --help       # Show help
```

### Keyboard Shortcuts

| Key | Action |
|-----|--------|
| `↑` `↓` | Navigate command history |
| `←` `→` | Move cursor |
| `Ctrl+A` | Jump to start of line |
| `Ctrl+E` | Jump to end of line |
| `Ctrl+K` | Delete to end of line |
| `Ctrl+U` | Delete to start of line |
| `Ctrl+W` | Delete previous word |
| `Ctrl+L` | Redraw screen |
| `Ctrl+C` | Clear current line |
| `Ctrl+D` | Exit (on empty line) |

## Shell Features

shelli supports:

- **Pipes**: `ls | grep foo | wc -l`
- **Input redirection**: `sort < file.txt`
- **Output redirection**: `echo hello > file.txt`
- **Append redirection**: `echo more >> file.txt`
- **Quoting**: `echo "hello world"` or `echo 'hello world'`
- **Builtins**: `cd`, `pwd`, `exit`, `echo`, `export`, `unset`, `env`

## Architecture

```
src/
├── main.c           # REPL loop
├── lexer.c/h        # Tokenization
├── parser.c/h       # AST construction
├── executor.c/h     # fork/exec/pipe handling
├── builtins.c/h     # Built-in commands
└── tui/
    ├── tui.h        # Public API
    ├── tui_core.c   # Terminal control (raw mode, alt buffer)
    ├── tui_input.c  # Line editor with history
    ├── tui_render.c # Double-buffered rendering
    ├── tui_widgets.c# Boxes, spinners, progress bars
    ├── tui_theme.c  # Catppuccin color palette
    └── tui_logo.c   # ASCII art splash screen
```

## Color Palette

shelli uses the [Catppuccin Mocha](https://github.com/catppuccin/catppuccin) color palette:

| Color | Hex | Usage |
|-------|-----|-------|
| Blue | `#89b4fa` | Titles, primary accent |
| Pink | `#f5c2e7` | Token types, keywords |
| Green | `#a6e3a1` | Success, strings |
| Peach | `#fab387` | Commands, numbers |
| Red | `#f38ba8` | Errors |
| Lavender | `#b4befe` | Spinners, secondary |
| Teal | `#94e2d5` | Prompt, special |

## Requirements

- C99 compiler (gcc, clang)
- POSIX-compliant OS (macOS, Linux)
- Terminal with 256-color support
- UTF-8 support for box-drawing characters

## License

MIT License - See [LICENSE](LICENSE) for details.

## Acknowledgments

- UI inspired by [LazyVim](https://github.com/LazyVim/LazyVim) and [Charm](https://charm.sh/)
- Color palette from [Catppuccin](https://github.com/catppuccin/catppuccin)
- Built for educational purposes to demystify shell internals
