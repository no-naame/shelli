/*
 * shelli - Educational Shell
 * util.h - Shared utilities: dynamic buffer
 */

#ifndef UTIL_H
#define UTIL_H

/* Dynamic buffer for building strings without fixed-size limits */
typedef struct {
    char *data;
    int len;
    int cap;
} DynBuf;

/* Initialize a dynamic buffer */
void dynbuf_init(DynBuf *b);

/* Push a single character */
void dynbuf_push(DynBuf *b, char c);

/* Append n bytes from s */
void dynbuf_append(DynBuf *b, const char *s, int n);

/* Append a null-terminated string */
void dynbuf_append_str(DynBuf *b, const char *s);

/* Return the data and reset the buffer (caller frees returned pointer) */
char *dynbuf_steal(DynBuf *b);

/* Free buffer memory */
void dynbuf_free(DynBuf *b);

#endif /* UTIL_H */
