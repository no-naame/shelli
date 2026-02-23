# shelli - Educational Shell
# Makefile

CC = cc
CFLAGS = -Wall -Wextra -pedantic -std=c99 -D_DEFAULT_SOURCE -D_DARWIN_C_SOURCE
LDFLAGS =

# Debug build flags
DEBUG_CFLAGS = -g -O0 -DDEBUG
# Release build flags
RELEASE_CFLAGS = -O2

# Source directories
SRCDIR = src
TUIDIR = $(SRCDIR)/tui

# Source files
SOURCES = $(SRCDIR)/main.c \
          $(SRCDIR)/lexer.c \
          $(SRCDIR)/parser.c \
          $(SRCDIR)/executor.c \
          $(SRCDIR)/builtins.c \
          $(SRCDIR)/expand.c \
          $(SRCDIR)/util.c \
          $(SRCDIR)/ast.c \
          $(SRCDIR)/test_builtin.c \
          $(SRCDIR)/variables.c \
          $(SRCDIR)/functions.c \
          $(SRCDIR)/jobs.c \
          $(TUIDIR)/tui_core.c \
          $(TUIDIR)/tui_input.c \
          $(TUIDIR)/tui_render.c \
          $(TUIDIR)/tui_widgets.c \
          $(TUIDIR)/tui_theme.c \
          $(TUIDIR)/tui_logo.c \
          $(TUIDIR)/tui_anim.c \
          $(TUIDIR)/tui_icons.c \
          $(TUIDIR)/tui_stage_tokenize.c \
          $(TUIDIR)/tui_stage_ast.c \
          $(TUIDIR)/tui_stage_expand.c \
          $(TUIDIR)/tui_stage_execute.c \
          $(TUIDIR)/tui_stage_result.c \
          $(TUIDIR)/tui_mascot.c \
          $(TUIDIR)/tui_pages.c \
          $(TUIDIR)/tui_suggest.c

HEADERS = $(SRCDIR)/lexer.h \
          $(SRCDIR)/parser.h \
          $(SRCDIR)/executor.h \
          $(SRCDIR)/builtins.h \
          $(SRCDIR)/expand.h \
          $(SRCDIR)/util.h \
          $(SRCDIR)/ast.h \
          $(SRCDIR)/test_builtin.h \
          $(SRCDIR)/variables.h \
          $(SRCDIR)/functions.h \
          $(SRCDIR)/jobs.h \
          $(TUIDIR)/tui.h

# Object files
OBJDIR = build
OBJECTS = $(OBJDIR)/main.o \
          $(OBJDIR)/lexer.o \
          $(OBJDIR)/parser.o \
          $(OBJDIR)/executor.o \
          $(OBJDIR)/builtins.o \
          $(OBJDIR)/expand.o \
          $(OBJDIR)/util.o \
          $(OBJDIR)/ast.o \
          $(OBJDIR)/test_builtin.o \
          $(OBJDIR)/variables.o \
          $(OBJDIR)/functions.o \
          $(OBJDIR)/jobs.o \
          $(OBJDIR)/tui_core.o \
          $(OBJDIR)/tui_input.o \
          $(OBJDIR)/tui_render.o \
          $(OBJDIR)/tui_widgets.o \
          $(OBJDIR)/tui_theme.o \
          $(OBJDIR)/tui_logo.o \
          $(OBJDIR)/tui_anim.o \
          $(OBJDIR)/tui_icons.o \
          $(OBJDIR)/tui_stage_tokenize.o \
          $(OBJDIR)/tui_stage_ast.o \
          $(OBJDIR)/tui_stage_expand.o \
          $(OBJDIR)/tui_stage_execute.o \
          $(OBJDIR)/tui_stage_result.o \
          $(OBJDIR)/tui_mascot.o \
          $(OBJDIR)/tui_pages.o \
          $(OBJDIR)/tui_suggest.o

# Output binary
TARGET = shelli

# Default target
all: release

# Release build
release: CFLAGS += $(RELEASE_CFLAGS)
release: $(TARGET)

# Debug build
debug: CFLAGS += $(DEBUG_CFLAGS)
debug: $(TARGET)

# Link
$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^

# Compile main source files
$(OBJDIR)/main.o: $(SRCDIR)/main.c $(HEADERS) | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/lexer.o: $(SRCDIR)/lexer.c $(SRCDIR)/lexer.h $(SRCDIR)/util.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/parser.o: $(SRCDIR)/parser.c $(SRCDIR)/parser.h $(SRCDIR)/ast.h $(SRCDIR)/lexer.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/executor.o: $(SRCDIR)/executor.c $(SRCDIR)/executor.h $(SRCDIR)/ast.h $(SRCDIR)/parser.h $(SRCDIR)/builtins.h $(SRCDIR)/expand.h $(SRCDIR)/variables.h $(SRCDIR)/functions.h $(SRCDIR)/jobs.h $(SRCDIR)/lexer.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/builtins.o: $(SRCDIR)/builtins.c $(SRCDIR)/builtins.h $(SRCDIR)/parser.h $(SRCDIR)/test_builtin.h $(SRCDIR)/variables.h $(SRCDIR)/functions.h $(SRCDIR)/jobs.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/expand.o: $(SRCDIR)/expand.c $(SRCDIR)/expand.h $(SRCDIR)/parser.h $(SRCDIR)/util.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/util.o: $(SRCDIR)/util.c $(SRCDIR)/util.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/ast.o: $(SRCDIR)/ast.c $(SRCDIR)/ast.h $(SRCDIR)/parser.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/test_builtin.o: $(SRCDIR)/test_builtin.c $(SRCDIR)/test_builtin.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/variables.o: $(SRCDIR)/variables.c $(SRCDIR)/variables.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/functions.o: $(SRCDIR)/functions.c $(SRCDIR)/functions.h $(SRCDIR)/ast.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/jobs.o: $(SRCDIR)/jobs.c $(SRCDIR)/jobs.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

# Compile TUI source files
$(OBJDIR)/tui_core.o: $(TUIDIR)/tui_core.c $(TUIDIR)/tui.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/tui_input.o: $(TUIDIR)/tui_input.c $(TUIDIR)/tui.h $(SRCDIR)/builtins.h $(SRCDIR)/lexer.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/tui_render.o: $(TUIDIR)/tui_render.c $(TUIDIR)/tui.h $(SRCDIR)/lexer.h $(SRCDIR)/ast.h $(SRCDIR)/parser.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/tui_widgets.o: $(TUIDIR)/tui_widgets.c $(TUIDIR)/tui.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/tui_theme.o: $(TUIDIR)/tui_theme.c $(TUIDIR)/tui.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/tui_logo.o: $(TUIDIR)/tui_logo.c $(TUIDIR)/tui.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/tui_anim.o: $(TUIDIR)/tui_anim.c $(TUIDIR)/tui.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/tui_icons.o: $(TUIDIR)/tui_icons.c $(TUIDIR)/tui.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/tui_stage_tokenize.o: $(TUIDIR)/tui_stage_tokenize.c $(TUIDIR)/tui.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/tui_stage_ast.o: $(TUIDIR)/tui_stage_ast.c $(TUIDIR)/tui.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/tui_stage_expand.o: $(TUIDIR)/tui_stage_expand.c $(TUIDIR)/tui.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/tui_stage_execute.o: $(TUIDIR)/tui_stage_execute.c $(TUIDIR)/tui.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/tui_stage_result.o: $(TUIDIR)/tui_stage_result.c $(TUIDIR)/tui.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/tui_mascot.o: $(TUIDIR)/tui_mascot.c $(TUIDIR)/tui.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/tui_pages.o: $(TUIDIR)/tui_pages.c $(TUIDIR)/tui.h $(SRCDIR)/lexer.h $(SRCDIR)/ast.h $(SRCDIR)/parser.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/tui_suggest.o: $(TUIDIR)/tui_suggest.c $(TUIDIR)/tui.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

# Create build directory
$(OBJDIR):
	mkdir -p $(OBJDIR)

# Clean
clean:
	rm -rf $(OBJDIR) $(TARGET)

# Run
run: $(TARGET)
	./$(TARGET)

# Run without splash
run-quick: $(TARGET)
	./$(TARGET) --no-splash

# Run with slow animations (step-through feel)
run-slow: $(TARGET)
	./$(TARGET) --anim-speed=slow

# Install (to /usr/local/bin by default)
PREFIX ?= /usr/local
install: $(TARGET)
	install -d $(PREFIX)/bin
	install -m 755 $(TARGET) $(PREFIX)/bin/

# Uninstall
uninstall:
	rm -f $(PREFIX)/bin/$(TARGET)

# Count lines of code
loc:
	@echo "Lines of code:"
	@wc -l $(SOURCES) $(HEADERS) | tail -1

# Format check (if clang-format available)
format-check:
	@command -v clang-format >/dev/null 2>&1 && \
		clang-format --dry-run --Werror $(SOURCES) $(HEADERS) || \
		echo "clang-format not found, skipping format check"

.PHONY: all release debug clean run run-quick run-slow install uninstall loc format-check
