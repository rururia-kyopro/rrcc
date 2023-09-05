#include <stddef.h>
typedef struct Buffer Buffer;

struct Buffer {
    char *buf;
    char *tail;
    size_t len;
};

Buffer *init_buffer();
void append_printf(Buffer *buf, char *fmt, ...);
