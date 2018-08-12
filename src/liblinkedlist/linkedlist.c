#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include "linkedlist.h"

#ifndef max
#define max(a,b) ((a) > (b) ? (a) : (b))
#endif

void linkedlist_init(LINKED_LIST *dcl) {
    dcl->head = NULL;
    dcl->tail = NULL;
    dcl->count = 0;
}

// O(1)
bool linkedlist_push_back(LINKED_LIST *dcl, LIST_VALTYPE entry) {
    LINKED_LIST_ENTRY *dce = (LINKED_LIST_ENTRY*)malloc(sizeof(LINKED_LIST_ENTRY));
    if (dce == NULL)
        return false;
    dce->value = entry;
    dce->next = NULL; // avoid loop

    if (dcl->head == NULL) {
        // add to empty list
        dce->prev = NULL;
        dcl->head = dce;
    }
    else {
        dce->prev = dcl->tail;
        dcl->tail->next = dce;
    }

    dcl->tail = dce;
    ++(dcl->count);
    return true;
}

// O(n)
void linkedlist_clear(LINKED_LIST *dcl) {
    LINKED_LIST_ENTRY *cur = dcl->head;
    while (cur != NULL) {
        LINKED_LIST_ENTRY *next = cur->next;
        free(cur);
        cur = next;
    }
    linkedlist_init(dcl);
}

// Should be O(1)
LIST_VALTYPE *linkedlist_throw(LINKED_LIST *dcl, LINKED_LIST_ENTRY **cur) {
    // ох, эта магия кода в час ночи. но я всё объясню.

    // 1. не морщим список, если нам сунули пустой указатель
    if (*cur == NULL)
        return NULL;

    // сохраняем указатели на предыдущее звено и текущее звено
    // пока не разыменовывая сами указатели
    LINKED_LIST_ENTRY *prev = (*cur)->prev;
    LINKED_LIST_ENTRY *next = (*cur)->next;

    // если предыдущее звено существует, то заставляем его указывать на следующее после нашего
    if (prev != NULL)
        prev->next = next;
        // если же нет, то мы пытаемся удалить первый элемент - нужно изменить указатель головы списка
    else
        dcl->head = next;

    // та же хня с последним элементом
    if (next != NULL)
        next->prev = prev;
    else
        dcl->tail = prev;

    // очень надеемся, что прежде чем запросить удаление звена, 
    // чувак освободил память по указателю внутри звена
    // иначе ему кранты от бога Валгринда!
    free(*cur);
    // эта функция аналогична обычному перемещению, только ещё и звенья удаляет
    // так что текущим звеном становится следующее для текущего
    *cur = next;
    // наконец декрементируем счётчик элементов
    --(dcl->count);

    // и если уж мы удалили не последний элемент, то нужно вернуть значение
    // ради которого все эти звенья и создавались
    if (*cur == NULL) {
        return NULL;
    }
    return &((*cur)->value);
}

// Single take, O(1)
// Reentrant version.
// Useful when not iterating over all, to avoid leaving non-NULL cur pointer.
LIST_VALTYPE *linkedlist_next_r(LINKED_LIST *dcl, LINKED_LIST_ENTRY **cur) {
    if (*cur == NULL) {
        *cur = dcl->head;
    }
    else {
        *cur = (*cur)->next;
    }

    if (*cur == NULL) {
        return NULL;
    }
    return &((*cur)->value);
}

// O(n), depending on index
LIST_VALTYPE *dcl_at_core(LINKED_LIST_ENTRY *dce, size_t index) {
    size_t pos = 0;
    while (true) {
        if (dce == NULL)
            return NULL;
        if (pos == index)
            break;
        dce = dce->next;
        pos++;
    }
    return &(dce->value);
}

LIST_VALTYPE *linkedlist_at(LINKED_LIST *dcl, size_t index) {
    if (index >= dcl->count)
        return NULL;
    return dcl_at_core(dcl->head, index);
}

// O(n log n), I hope
void dcl_quick_sort_core(LINKED_LIST_ENTRY *head, LINKED_LIST_ENTRY *tail, ssize_t len
                       , int (*cmpr)(LIST_VALTYPE *, LIST_VALTYPE *)) {
    LINKED_LIST_ENTRY *l_entry = head, *r_entry = tail;
    ssize_t l_idx = 0, r_idx = len - 1;
    // берём опорный элемент
    //ssize_t m_idx = rand() % len; // get rid of dcl->count
    ssize_t m_idx = len / 2 + rand() % max(1, len / 4); // TODO: unstable?
    LIST_VALTYPE m = *dcl_at_core(l_entry, m_idx); // FIXED: get copy instead of link

    do {
        // TODO: check for NULL
        while (l_entry != NULL && cmpr(&l_entry->value, &m) < 0) {
            l_entry = l_entry->next;
            l_idx++;
        }
        while (r_entry != NULL && cmpr(&r_entry->value, &m) > 0) {
            r_entry = r_entry->prev;
            r_idx--;
        }

        if (l_idx <= r_idx) {
            // don't copy self
            // FIXME: rebind links maybe?
            if (l_idx != r_idx) {
                LIST_VALTYPE tmp = l_entry->value;
                l_entry->value = r_entry->value;
                r_entry->value = tmp;
            }

            // taking ->next after swap!
            l_entry = l_entry->next;
            l_idx++;
            r_entry = r_entry->prev;
            r_idx--;
        }
    } while (l_idx <= r_idx);

    if (r_idx > 0) dcl_quick_sort_core(head, r_entry, r_idx + 1, cmpr);
    if (l_idx < len - 1) dcl_quick_sort_core(l_entry, tail, len - l_idx, cmpr);
}

// In-place quicksort of double linked list
void linkedlist_quick_sort(LINKED_LIST *dcl, int (*comparator)(LIST_VALTYPE *, LIST_VALTYPE *)) {
    dcl_quick_sort_core(dcl->head, dcl->tail, dcl->count, comparator);
}
