#include "charbuf.h"

#include <string.h>

#include "alloc.h"

CharBuffer *
char_buffer_new(const char *from)
{
    CharBuffer *b = (CharBuffer *)jik_alloc(sizeof(CharBuffer));
    b->size       = strlen(from);
    b->capacity   = b->size;
    b->data       = jik_alloc(b->size + 1);
    memcpy(b->data, from, b->size + 1);
    b->data[b->size] = '\0';
    return b;
}

size_t
char_buffer_size(CharBuffer *b)
{
    return b->size;
}

char *
char_buffer_data(CharBuffer *b)
{
    return b->data;
}

void
char_buffer_append(CharBuffer *b, const char *str)
{
    size_t n       = strlen(str);
    size_t req_cap = b->size + n + 1;
    if (req_cap > b->capacity) {
        size_t old_cap = b->capacity;
        while (req_cap > b->capacity) {
            b->capacity = 2 * b->capacity + 1;
        }
        b->data = jik_realloc(b->data, b->capacity, old_cap);
    }
    memcpy(b->data + b->size, str, n);
    b->size += n;
    b->data[b->size] = '\0';
}

void
char_buffer_push(CharBuffer *b, char ch)
{
    size_t req_cap = b->size + 2;
    if (req_cap > b->capacity) {
        size_t old_cap = b->capacity;
        while (req_cap > b->capacity) {
            b->capacity = 2 * b->capacity + 1;
        }
        b->data = jik_realloc(b->data, b->capacity, old_cap);
    }
    b->data[b->size++] = ch;
    b->data[b->size]   = '\0';
}
