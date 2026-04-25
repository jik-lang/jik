#ifndef JIK_VEC_H
#define JIK_VEC_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "alloc.h"

// TODO: get rid of these macros, as we effectively duplicate them in any generic type
//  implement directly instead
#define JIK_VEC_FATAL_ERROR_IF(cond, msg, ...)                                                     \
    do {                                                                                           \
        if (cond) {                                                                                \
            fprintf(stderr, "%s", (msg), ##__VA_ARGS__);                                           \
            exit(EXIT_FAILURE);                                                                    \
        }                                                                                          \
    } while (0)

#define JIK_VEC_ALLOC_ERR_MSG "memory allocation failure"

#define JIK_VEC_DECLARE(vec_name, vec_type)                                                        \
    typedef struct vec_name vec_name;                                                              \
    typedef struct {                                                                               \
        vec_name *vec;                                                                             \
        size_t    iter_idx;                                                                        \
    } vec_name##_iter;                                                                             \
    vec_name       *vec_name##_new(size_t size);                                                   \
    vec_name       *vec_name##_new_empty(void);                                                    \
    vec_type        vec_name##_get(vec_name *a, size_t idx);                                       \
    vec_type       *vec_name##_get_ptr(vec_name *a, size_t idx);                                   \
    void            vec_name##_set(vec_name *a, size_t idx, vec_type item);                        \
    void            vec_name##_push(vec_name *a, vec_type item);                                   \
    vec_type        vec_name##_pop(vec_name *a);                                                   \
    vec_name##_iter vec_name##_iter_new(vec_name *a);                                              \
    bool            vec_name##_iter_next(vec_name##_iter *it, vec_type *out);                      \
    void            vec_name##_extend(vec_name *a, vec_name *b);                                   \
    size_t          vec_name##_size(vec_name *a);                                                  \
    void            vec_name##_free(vec_name **a);

#define JIK_INIT_VEC_CAPACITY 8

#define JIK_VEC_DEFINE(vec_name, vec_type)                                                           \
    typedef struct vec_name vec_name;                                                                \
    struct vec_name {                                                                                \
        vec_type *data;                                                                              \
        size_t    size;                                                                              \
        size_t    capacity;                                                                          \
    };                                                                                               \
                                                                                                     \
    vec_name *vec_name##_new(size_t size)                                                            \
    {                                                                                                \
        struct vec_name *a = (struct vec_name *)jik_alloc(sizeof(vec_name));                         \
        a->data            = (vec_type *)jik_alloc(size * sizeof(vec_type));                         \
        a->size            = size;                                                                   \
        a->capacity        = size;                                                                   \
        return a;                                                                                    \
    }                                                                                                \
                                                                                                     \
    vec_name *vec_name##_new_empty(void)                                                             \
    {                                                                                                \
        struct vec_name *a = (struct vec_name *)jik_alloc(sizeof(vec_name));                         \
        a->data            = (vec_type *)jik_alloc(JIK_INIT_VEC_CAPACITY * sizeof(vec_type));        \
        a->size            = 0;                                                                      \
        a->capacity        = JIK_INIT_VEC_CAPACITY;                                                  \
        return a;                                                                                    \
    }                                                                                                \
                                                                                                     \
    vec_type vec_name##_get(vec_name *a, size_t idx)                                                 \
    {                                                                                                \
        JIK_VEC_FATAL_ERROR_IF(idx >= a->size, "get: vector index out of bounds");                   \
        return a->data[idx];                                                                         \
    }                                                                                                \
                                                                                                     \
    vec_type *vec_name##_get_ptr(vec_name *a, size_t idx)                                            \
    {                                                                                                \
        JIK_VEC_FATAL_ERROR_IF(idx >= a->size, "get_ptr: vector index out of bounds");               \
        return &a->data[idx];                                                                        \
    }                                                                                                \
                                                                                                     \
    void vec_name##_set(vec_name *a, size_t idx, vec_type item)                                      \
    {                                                                                                \
        JIK_VEC_FATAL_ERROR_IF(idx >= a->size, "set: vector index out of bounds");                   \
        a->data[idx] = item;                                                                         \
    }                                                                                                \
                                                                                                     \
    void vec_name##_push(vec_name *a, vec_type item)                                                 \
    {                                                                                                \
        if (a->size == a->capacity) {                                                                \
            size_t old_cap = a->capacity;                                                            \
            a->capacity    = 2 * a->capacity + 1;                                                    \
            void *p        = jik_realloc(                                                            \
                (char *)a->data, a->capacity * sizeof(vec_type), old_cap * sizeof(vec_type)); \
            a->data = p;                                                                             \
        }                                                                                            \
        a->data[a->size] = item;                                                                     \
        a->size++;                                                                                   \
    }                                                                                                \
                                                                                                     \
    vec_type vec_name##_pop(vec_name *a)                                                             \
    {                                                                                                \
        JIK_VEC_FATAL_ERROR_IF(a->size == 0, "cannot pop from empty vector");                        \
        a->size--;                                                                                   \
        return a->data[a->size];                                                                     \
    }                                                                                                \
                                                                                                     \
    vec_name##_iter vec_name##_iter_new(vec_name *a)                                                 \
    {                                                                                                \
        return (vec_name##_iter){.vec = a, .iter_idx = 0};                                           \
    }                                                                                                \
                                                                                                     \
    bool vec_name##_iter_next(vec_name##_iter *it, vec_type *out)                                    \
    {                                                                                                \
        if (it->iter_idx == it->vec->size) {                                                         \
            return false;                                                                            \
        }                                                                                            \
        *out = it->vec->data[it->iter_idx++];                                                        \
        return true;                                                                                 \
    }                                                                                                \
                                                                                                     \
    void vec_name##_extend(vec_name *a, vec_name *b)                                                 \
    {                                                                                                \
        size_t cap_req = a->size + b->size;                                                          \
        void  *p =                                                                                   \
            jik_realloc((char *)a->data, cap_req * sizeof(vec_type), a->size * sizeof(vec_type));    \
        a->data = p;                                                                                 \
        for (size_t i = 0; i < b->size; i++)                                                         \
            a->data[a->size + i] = b->data[i];                                                       \
        a->size = cap_req;                                                                           \
    }                                                                                                \
                                                                                                     \
    size_t vec_name##_size(vec_name *a) { return a->size; }                                          \
                                                                                                     \
    void vec_name##_free(vec_name **a)                                                               \
    {                                                                                                \
        if (a && *a) {                                                                               \
            free((*a)->data);                                                                        \
            free((*a));                                                                              \
            (*a) = NULL;                                                                             \
        }                                                                                            \
    }

#endif
