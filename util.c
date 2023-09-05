#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "util.h"

Buffer *init_buffer() {
    Buffer *buf = calloc(1, sizeof(Buffer));
    buf->buf = calloc(1, 1);
    buf->tail = buf->buf;
    buf->len = 1;
    return buf;
}

void append_printf(Buffer *buf, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int appendlen = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    int filled = buf->tail - buf->buf;
    if(filled + appendlen + 1 > buf->len) {
        int newlen = filled + appendlen + 1;
        if(newlen < buf->len * 2) {
            newlen = buf->len * 2;
        }
        buf->buf = realloc(buf->buf, newlen);
        buf->tail = buf->buf + filled;
        buf->len = newlen;
    }
    va_start(ap, fmt);
    vsnprintf(buf->tail, buf->len - filled, fmt, ap);
    va_end(ap);
    buf->tail += appendlen;
}

