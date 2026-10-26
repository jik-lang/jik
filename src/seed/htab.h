#ifndef JIK_HTAB_H
#define JIK_HTAB_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"

// TODO: get rid of these macros, as we effectively duplicate them in any generic type
//  implement directly instead
#define JIK_HTAB_FATAL_ERROR_IF(cond, msg, ...)                                                    \
    do {                                                                                           \
        if (cond) {                                                                                \
            fprintf(stderr, "%s", (msg), ##__VA_ARGS__);                                           \
            exit(EXIT_FAILURE);                                                                    \
        }                                                                                          \
    } while (0)

#define JIK_HTAB_ALLOC_ERR_MSG "memory allocation failure"

#define JIK_HTAB_DECLARE(tab_name, tab_elem_type)                                                  \
    typedef struct tab_name tab_name;                                                              \
    typedef struct {                                                                               \
        tab_name *ht;                                                                              \
        size_t    iter_idx;                                                                        \
        size_t    iter_cnt;                                                                        \
    } tab_name##_iter;                                                                             \
    typedef struct tab_name##_item {                                                               \
        char         *key;                                                                         \
        tab_elem_type value;                                                                       \
    } tab_name##_item;                                                                             \
    uint32_t         tab_name##_hash_key(char *key);                                               \
    tab_name##_item *tab_name##_find_entry(tab_name##_item *items, size_t capacity, char *key);    \
    void             tab_name##_resize(tab_name *ht, size_t capacity);                             \
    tab_name        *tab_name##_new(void);                                                         \
    void             tab_name##_set(tab_name *ht, char *key, tab_elem_type val);                   \
    tab_elem_type   *tab_name##_get(tab_name *ht, char *key);                                      \
    size_t           tab_name##_size(tab_name *ht);                                                \
    tab_name##_iter  tab_name##_iter_new(tab_name *ht);                                            \
    bool             tab_name##_iter_next(tab_name##_iter *it, tab_name##_item *out);              \
    void             tab_name##_free(tab_name **ht);

#define JIK_FNV_OFFSET_BASIS   2166136261u
#define JIK_FNV_PRIME          16777619u
#define JIK_HTAB_INIT_CAPACITY 16
#define JIK_HTAB_LOAD_FACT     0.75

#define JIK_HTAB_DEFINE(tab_name, tab_elem_type)                                                   \
    struct tab_name {                                                                              \
        size_t           capacity;                                                                 \
        size_t           size;                                                                     \
        tab_name##_item *items;                                                                    \
    };                                                                                             \
                                                                                                   \
    uint32_t tab_name##_hash_key(char *key)                                                        \
    {                                                                                              \
        uint32_t hash = JIK_FNV_OFFSET_BASIS;                                                      \
        for (char *p = key; *p; p++) {                                                             \
            hash ^= (uint8_t)(unsigned char)(*p);                                                  \
            hash *= JIK_FNV_PRIME;                                                                 \
        }                                                                                          \
        return hash;                                                                               \
    }                                                                                              \
                                                                                                   \
    tab_name##_item *tab_name##_find_entry(tab_name##_item *items, size_t capacity, char *key)     \
    {                                                                                              \
        size_t           idx = (size_t)(tab_name##_hash_key(key) % capacity);                      \
        tab_name##_item *item;                                                                     \
        for (;;) {                                                                                 \
            item = &items[idx];                                                                    \
            if (item->key == NULL || strcmp(item->key, key) == 0)                                  \
                return item;                                                                       \
            idx = (idx + 1) % capacity;                                                            \
        }                                                                                          \
    }                                                                                              \
                                                                                                   \
    void tab_name##_resize(tab_name *ht, size_t capacity)                                          \
    {                                                                                              \
        tab_name##_item *new_items =                                                               \
            (tab_name##_item *)jik_alloc(capacity * sizeof(tab_name##_item));                      \
        memset(new_items, 0, capacity * sizeof(tab_name##_item));                                  \
        tab_name##_item *item;                                                                     \
        for (size_t i = 0; i < ht->capacity; i++) {                                                \
            item = &(ht->items[i]);                                                                \
            if (item->key == NULL)                                                                 \
                continue;                                                                          \
            tab_name##_item *dest = tab_name##_find_entry(new_items, capacity, item->key);         \
            dest->key             = item->key;                                                     \
            dest->value           = item->value;                                                   \
        }                                                                                          \
        ht->items    = new_items;                                                                  \
        ht->capacity = capacity;                                                                   \
        return;                                                                                    \
    }                                                                                              \
                                                                                                   \
    tab_name *tab_name##_new(void)                                                                 \
    {                                                                                              \
        tab_name *ht = (tab_name *)jik_alloc(sizeof(tab_name));                                    \
        ht->capacity = JIK_HTAB_INIT_CAPACITY;                                                     \
        ht->size     = 0;                                                                          \
        ht->items =                                                                                \
            (tab_name##_item *)jik_alloc(JIK_HTAB_INIT_CAPACITY * sizeof(tab_name##_item));        \
        memset(ht->items, 0, JIK_HTAB_INIT_CAPACITY * sizeof(tab_name##_item));                    \
        return ht;                                                                                 \
    }                                                                                              \
                                                                                                   \
    void tab_name##_set(tab_name *ht, char *key, tab_elem_type val)                                \
    {                                                                                              \
        if (ht->size + 1 > ht->capacity * JIK_HTAB_LOAD_FACT) {                                    \
            size_t new_cap = ht->capacity * 2;                                                     \
            tab_name##_resize(ht, new_cap);                                                        \
        }                                                                                          \
        tab_name##_item *item = tab_name##_find_entry(ht->items, ht->capacity, key);               \
        if (item->key == NULL)                                                                     \
            ht->size++;                                                                            \
        item->key   = key;                                                                         \
        item->value = val;                                                                         \
        return;                                                                                    \
    }                                                                                              \
                                                                                                   \
    tab_elem_type *tab_name##_get(tab_name *ht, char *key)                                         \
    {                                                                                              \
        tab_name##_item *item = tab_name##_find_entry(ht->items, ht->capacity, key);               \
        if (item->key == NULL)                                                                     \
            return NULL;                                                                           \
        return &item->value;                                                                       \
    }                                                                                              \
                                                                                                   \
    tab_name##_iter tab_name##_iter_new(tab_name *ht)                                              \
    {                                                                                              \
        return (tab_name##_iter){.ht = ht, .iter_idx = 0, .iter_cnt = 0};                          \
    }                                                                                              \
                                                                                                   \
    bool tab_name##_iter_next(tab_name##_iter *it, tab_name##_item *out)                           \
    {                                                                                              \
        if (it->iter_cnt == it->ht->size) {                                                        \
            return false;                                                                          \
        }                                                                                          \
        while (it->ht->items[it->iter_idx].key == NULL) {                                          \
            it->iter_idx++;                                                                        \
            JIK_HTAB_FATAL_ERROR_IF(it->iter_idx == it->ht->capacity,                              \
                                    "hash table iterator exhausted");                              \
        }                                                                                          \
        it->iter_cnt++;                                                                            \
        *out = it->ht->items[it->iter_idx++];                                                      \
        return true;                                                                               \
    }                                                                                              \
                                                                                                   \
    size_t tab_name##_size(tab_name *ht) { return ht->size; }                                      \
                                                                                                   \
    void tab_name##_free(tab_name **ht)                                                            \
    {                                                                                              \
        if (ht && *ht) {                                                                           \
            free((*ht)->items);                                                                    \
            free((*ht));                                                                           \
            (*ht) = NULL;                                                                          \
        }                                                                                          \
    }

#endif
