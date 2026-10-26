#ifndef JIK_CHARBUF_H
#define JIK_CHARBUF_H

#include <stddef.h>

typedef struct CharBuffer {
    char  *data;
    size_t size;
    size_t capacity;
} CharBuffer;

CharBuffer *
char_buffer_new(const char *from);
size_t
char_buffer_size(CharBuffer *b);
char *
char_buffer_data(CharBuffer *b);
void
char_buffer_append(CharBuffer *b, const char *str);
void
char_buffer_push(CharBuffer *b, char ch);

#endif
