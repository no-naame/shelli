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
          $(TUIDIR)/tui_core.c \
          $(TUIDIR)/tui_input.c \
          $(TUIDIR)/tui_render.c \
          $(TUIDIR)/tui_widgets.c \
          $(TUIDIR)/tui_theme.c \
          $(TUIDIR)/tui_logo.c

HEADERS = $(SRCDIR)/lexer.h \
          $(SRCDIR)/parser.h \
          $(SRCDIR)/executor.h \
          $(SRCDIR)/builtins.h \
          $(TUIDIR)/tui.h

# Object files
OBJDIR = build
OBJECTS = $(OBJDIR)/main.o \
          $(OBJDIR)/lexer.o \
          $(OBJDIR)/parser.o \
          $(OBJDIR)/executor.o \
          $(OBJDIR)/builtins.o \
          $(OBJDIR)/tui_core.o \
          $(OBJDIR)/tui_input.o \
          $(OBJDIR)/tui_render.o \
          $(OBJDIR)/tui_widgets.o \
          $(OBJDIR)/tui_theme.o \
          $(OBJDIR)/tui_logo.o

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

$(OBJDIR)/lexer.o: $(SRCDIR)/lexer.c $(SRCDIR)/lexer.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/parser.o: $(SRCDIR)/parser.c $(SRCDIR)/parser.h $(SRCDIR)/lexer.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/executor.o: $(SRCDIR)/executor.c $(SRCDIR)/executor.h $(SRCDIR)/parser.h $(SRCDIR)/builtins.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/builtins.o: $(SRCDIR)/builtins.c $(SRCDIR)/builtins.h $(SRCDIR)/parser.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

# Compile TUI source files
$(OBJDIR)/tui_core.o: $(TUIDIR)/tui_core.c $(TUIDIR)/tui.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/tui_input.o: $(TUIDIR)/tui_input.c $(TUIDIR)/tui.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/tui_render.o: $(TUIDIR)/tui_render.c $(TUIDIR)/tui.h $(SRCDIR)/lexer.h $(SRCDIR)/parser.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/tui_widgets.o: $(TUIDIR)/tui_widgets.c $(TUIDIR)/tui.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/tui_theme.o: $(TUIDIR)/tui_theme.c $(TUIDIR)/tui.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/tui_logo.o: $(TUIDIR)/tui_logo.c $(TUIDIR)/tui.h | $(OBJDIR)
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

# Run in debug mode
run-debug: $(TARGET)
	./$(TARGET) --debug

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

.PHONY: all release debug clean run run-quick run-debug install uninstall loc format-check
