#pragma once

// Note: this include is a beta feature for design- and compile-time
#include "../liblinux_util/mscfix.h"

#include <stdint.h>
#include <stdbool.h>
// ReSharper disable once CppUnusedIncludeDirective
#include <stddef.h>

typedef union {
	uint64_t lng;
	void *ptr;
} LIST_VALTYPE;

// Double-linked list with fake node, counter and end pointer
// Push: O(1)
// Pop: O(1)
// Take random: O(n)
// Iterate over all: O(n)
typedef struct __linked_list_entry {
	struct __linked_list_entry *prev;
	struct __linked_list_entry *next;
	LIST_VALTYPE value;
} LINKED_LIST_ENTRY;

typedef struct __linked_list {
	LINKED_LIST_ENTRY *head;
	LINKED_LIST_ENTRY *tail;
	size_t count;
} LINKED_LIST;

void linkedlist_init(LINKED_LIST *dcl);
bool linkedlist_push_back(LINKED_LIST *dcl, LIST_VALTYPE entry);
void linkedlist_clear(LINKED_LIST *dcl);
// Remove from `dcl' by `ptr' using ptr->prev and ptr->next
// This is an angry replacement to `linkedlist_next_r' call
LIST_VALTYPE *linkedlist_throw(LINKED_LIST *dcl, LINKED_LIST_ENTRY **cur);
LIST_VALTYPE *linkedlist_next_r(LINKED_LIST *dcl, LINKED_LIST_ENTRY **cur);
LIST_VALTYPE *linkedlist_at(LINKED_LIST *dcl, size_t index);

void linkedlist_quick_sort(LINKED_LIST *dcl, int(*comparator)(LIST_VALTYPE*, LIST_VALTYPE*));
