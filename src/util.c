/*
 * shelli - Educational Shell
 * util.c - Shared utilities: dynamic buffer
 */

#include <stdlib.h>
#include <string.h>
#include "util.h"

#define DYNBUF_INIT_CAP 64

void dynbuf_init(DynBuf *b) {
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

static void dynbuf_grow(DynBuf *b, int needed) {
    if (b->len + needed <= b->cap) return;

    int new_cap = b->cap == 0 ? DYNBUF_INIT_CAP : b->cap;
    while (new_cap < b->len + needed) {
        new_cap *= 2;
    }

    char *new_data = realloc(b->data, new_cap);
    if (!new_data) return; /* OOM - silently fail */
    b->data = new_data;
    b->cap = new_cap;
}

void dynbuf_push(DynBuf *b, char c) {
    dynbuf_grow(b, 1);
    if (b->len < b->cap) {
        b->data[b->len++] = c;
    }
}

void dynbuf_append(DynBuf *b, const char *s, int n) {
    if (n <= 0) return;
    dynbuf_grow(b, n);
    if (b->len + n <= b->cap) {
        memcpy(b->data + b->len, s, n);
        b->len += n;
    }
}

void dynbuf_append_str(DynBuf *b, const char *s) {
    if (!s) return;
    dynbuf_append(b, s, (int)strlen(s));
}

char *dynbuf_steal(DynBuf *b) {
    /* Ensure null termination */
    dynbuf_push(b, '\0');
    char *result = b->data;
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
    return result;
}

void dynbuf_free(DynBuf *b) {
    free(b->data);
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}
