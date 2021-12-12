// Copyright (C) 2021 Lennard Walter
// License: MIT

#ifndef __LIST_H
#define __LIST_H

#include <stdlib.h>

// disable -Wunused-value warnings (compiler complains because of compound expressions)
#pragma GCC diagnostic ignored "-Wunused-value"

#define _LIST_NODE_TYPE(type) type##_node_t
#define _LIST_NODE_TYPE_P(type) _LIST_NODE_TYPE(type)*

#define LIST_DEF(eltype, listtype)                                                       \
    typedef struct _LIST_NODE_TYPE(listtype) {                                           \
        eltype value;                                                                    \
        struct _LIST_NODE_TYPE_P(listtype) next;                                         \
        struct _LIST_NODE_TYPE_P(listtype) prev;                                         \
    } _LIST_NODE_TYPE(listtype);                                                         \
                                                                                         \
    typedef struct {                                                                     \
        _LIST_NODE_TYPE_P(listtype) head;                                                \
        _LIST_NODE_TYPE_P(listtype) tail;                                                \
        size_t node_size;                                                                \
        size_t length;                                                                   \
    } listtype;

#define LIST_NEW(listtype)                                                               \
    ({                                                                                   \
        listtype* __list_tmp = malloc(sizeof(listtype));                                 \
        __list_tmp->head = NULL;                                                         \
        __list_tmp->tail = NULL;                                                         \
        __list_tmp->node_size = sizeof(_LIST_NODE_TYPE(listtype));                       \
        __list_tmp->length = 0;                                                          \
        __list_tmp;                                                                      \
    });

#define LIST_FREE(list)                                                                  \
    do {                                                                                 \
        typeof(list->head) node = list->head;                                            \
        while (node) {                                                                   \
            typeof(node->next) next = node->next;                                        \
            free(node);                                                                  \
            node = next;                                                                 \
        }                                                                                \
        free(list);                                                                      \
    } while (0)

#define LIST_APPEND(list, value_)                                                        \
    do {                                                                                 \
        typeof(list->head) node = malloc(list->node_size);                               \
        node->value = value_;                                                            \
        node->next = NULL;                                                               \
        node->prev = list->tail;                                                         \
        if (list->tail) {                                                                \
            list->tail->next = node;                                                     \
        } else {                                                                         \
            list->head = node;                                                           \
        }                                                                                \
        list->tail = node;                                                               \
        list->length++;                                                                  \
        node;                                                                            \
    } while (0)

#define LIST_PREPEND(list, value_)                                                       \
    do {                                                                                 \
        typeof(list->head) node = malloc(list->node_size);                               \
        node->value = value_;                                                            \
        node->next = list->head;                                                         \
        node->prev = NULL;                                                               \
        if (list->head) {                                                                \
            list->head->prev = node;                                                     \
        } else {                                                                         \
            list->tail = node;                                                           \
        }                                                                                \
        list->head = node;                                                               \
        list->length++;                                                                  \
        node;                                                                            \
    } while (0)

#define _LIST_GET_NODE(list, index)                                                      \
    ({                                                                                   \
        typeof(list->head) node;                                                         \
                                                                                         \
        if (index < 0 || index >= list->length) {                                        \
            node = NULL;                                                                 \
        } else {                                                                         \
            node = list->head;                                                           \
            for (int i = index; i; i--) {                                                \
                node = node->next;                                                       \
            }                                                                            \
        }                                                                                \
        node;                                                                            \
    })

#define LIST_GET(list, index)                                                            \
    ({                                                                                   \
        typeof(list->head) node = _LIST_GET_NODE(list, index);                           \
        node->value;                                                                     \
    })

#define LIST_INSERT(list, index, value_)                                                 \
    do {                                                                                 \
        typeof(list->head) node = malloc(list->node_size);                               \
        node->value = value_;                                                            \
        node->next = NULL;                                                               \
        node->prev = NULL;                                                               \
                                                                                         \
        if (index < 0 || index >= list->length) {                                        \
            LIST_APPEND(list, value_);                                                   \
        } else if (index == 0) {                                                         \
            LIST_PREPEND(list, value_);                                                  \
        } else {                                                                         \
            typeof(list->head) prev = _LIST_GET_NODE(list, index - 1);                   \
            node->next = prev->next;                                                     \
            prev->next->prev = node;                                                     \
            prev->next = node;                                                           \
            node->prev = prev;                                                           \
            list->length++;                                                              \
        }                                                                                \
    } while (0)

#define _LIST_REMOVE_NODE(list, node)                                                    \
    do {                                                                                 \
        if (node->prev) {                                                                \
            node->prev->next = node->next;                                               \
        } else {                                                                         \
            list->head = node->next;                                                     \
        }                                                                                \
                                                                                         \
        if (node->next) {                                                                \
            node->next->prev = node->prev;                                               \
        } else {                                                                         \
            list->tail = node->prev;                                                     \
        }                                                                                \
        free(node);                                                                      \
        list->length--;                                                                  \
    } while (0)

#define LIST_REMOVE(list, index)                                                         \
    do {                                                                                 \
        typeof(list->head) node = _LIST_GET_NODE(list, index);                           \
        if (node) {                                                                      \
            _LIST_REMOVE_NODE(list, node)                                                \
        }                                                                                \
    } while (0)

#define LIST_POP(list, index)                                                            \
    ({                                                                                   \
        typeof(list->head) node = _LIST_GET_NODE(list, index);                           \
        typeof(node->value) value = node->value;                                         \
        _LIST_REMOVE_NODE(list, node);                                                   \
        value;                                                                           \
    })

#define LIST_LENGTH(list) (list->length)

#define _LIST_FOREACH_IMPL(list, name, counter)                                          \
    for (typeof(list->head) __node##counter = list->head,                                \
                            __run_once_guard##counter = (void*)1;                        \
         __node##counter != NULL;                                                        \
         __node##counter = __node##counter->next, __run_once_guard##counter = (void*)1)  \
        for (typeof(list->head->value) name = __node##counter->value;                    \
             __run_once_guard##counter; __run_once_guard##counter = (void*)0)

#define _LIST_FOREACH_REVERSE_IMPL(list, name, counter)                                  \
    for (typeof(list->tail) __node##counter = list->tail,                                \
                            __run_once_guard##counter = (void*)1;                        \
         __node##counter != NULL;                                                        \
         __node##counter = __node##counter->prev, __run_once_guard##counter = (void*)1)  \
        for (typeof(list->tail->value) name = __node##counter->value;                    \
             __run_once_guard##counter; __run_once_guard##counter = (void*)0)

#define _LIST_FOREACH_IMPL_WRAPPER(list, name, counter)                                  \
    _LIST_FOREACH_IMPL(list, name, counter)

#define _LIST_FOREACH_REVERSE_IMPL_WRAPPER(list, name, counter)                          \
    _LIST_FOREACH_REVERSE_IMPL(list, name, counter)

#define LIST_FOREACH(list, name) _LIST_FOREACH_IMPL_WRAPPER(list, name, __COUNTER__)
#define LIST_FOREACH_REVERSE(list, name)                                                 \
    _LIST_FOREACH_REVERSE_IMPL_WRAPPER(list, name, __COUNTER__)

#endif /* __LIST_H__ */
