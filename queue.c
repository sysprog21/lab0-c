#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "queue.h"

/* Notice: sometimes, Cppcheck would find the potential NULL pointer bugs,
 * but some of them cannot occur. You can suppress them by adding the
 * following line.
 *   cppcheck-suppress nullPointer
 */


/* Create an empty queue */
struct list_head *q_new()
{
    struct list_head *head = malloc(sizeof(struct list_head));

    if (!head)
        return NULL;

    INIT_LIST_HEAD(head);
    return head;
}

/* Free all storage used by queue */
void q_free(struct list_head *l)
{
    if (!l)
        return;

    element_t *curr, *safe;

    list_for_each_entry_safe (curr, safe, l, list) {
        list_del(&curr->list);
        q_release_element(curr);
    }
    free(l);
}

static element_t *ele_alloc(const char *s)
{
    element_t *node = malloc(sizeof(element_t));
    if (!node)
        return NULL;

    size_t len = strlen(s) + 1;
    node->value = malloc(sizeof(char) * len);
    if (!node->value) {
        free(node);
        return NULL;
    }
    strncpy(node->value, s, len);
    return node;
}

/* Insert an element at head of queue */
bool q_insert_head(struct list_head *head, char *s)
{
    if (!head)
        return false;

    element_t *new = ele_alloc(s);
    if (!new)
        return false;

    list_add(&new->list, head);

    return true;
}

/* Insert an element at tail of queue */
bool q_insert_tail(struct list_head *head, char *s)
{
    if (!head)
        return false;

    element_t *new = ele_alloc(s);
    if (!new)
        return false;

    list_add_tail(&new->list, head);

    return true;
}

#define min(x, y) (x) < (y) ? (x) : (y)

/* Remove an element from head of queue */
element_t *q_remove_head(struct list_head *head, char *sp, size_t bufsize)
{
    if (!head || list_empty(head))
        return NULL;

    element_t *node;
    node = list_first_entry(head, element_t, list);
    list_del(&node->list);

    if (sp) {
        size_t len = strlen(node->value) + 1;
        len = min(bufsize, len);
        strncpy(sp, node->value, len);
        sp[len - 1] = '\0';
    }
    return node;
}

/* Remove an element from tail of queue */
element_t *q_remove_tail(struct list_head *head, char *sp, size_t bufsize)
{
    if (!head || list_empty(head))
        return NULL;

    element_t *node;
    node = list_last_entry(head, element_t, list);
    list_del(&node->list);

    if (sp) {
        size_t len = strnlen(node->value, bufsize - 1);
        strncpy(sp, node->value, len);
        sp[len] = '\0';
    }
    return node;
}

/* Return number of elements in queue */
int q_size(struct list_head *head)
{
    if (!head || list_empty(head))
        return 0;

    struct list_head *node;
    int count = 0;
    list_for_each (node, head)
        count++;

    return count;
}

/* Delete the middle node in queue */
bool q_delete_mid(struct list_head *head)
{
    // https://leetcode.com/problems/delete-the-middle-node-of-a-linked-list/
    if (!head || list_empty(head))
        return false;

    struct list_head *forward = head->next, *backward = head->prev;
    while (forward != backward) {
        forward = forward->next;
        if (forward == backward)
            break;

        backward = backward->prev;
    }
    element_t *to_del = list_entry(forward, element_t, list);
    list_del(forward);
    q_release_element(to_del);
    return true;
}

/* Delete all nodes that have duplicate string */
bool q_delete_dup(struct list_head *head)
{
    // https://leetcode.com/problems/remove-duplicates-from-sorted-list-ii/
    if (!head)
        return false;

    if (list_is_singular(head) || list_empty(head))
        return true;

    struct list_head **indirect = &head->next;
    while (*indirect != head && (*indirect)->next != head) {
        char *a = list_entry(*indirect, element_t, list)->value;
        char *b = list_entry((*indirect)->next, element_t, list)->value;
        if (strcmp(a, b) == 0) {
            struct list_head *tmp = (*indirect)->next;
            do {
                list_del(tmp);
                q_release_element(list_entry(tmp, element_t, list));
                tmp = (*indirect)->next;
            } while (tmp != head &&
                     strcmp(a, list_entry(tmp, element_t, list)->value) == 0);

            tmp = *indirect;
            list_del(tmp);
            q_release_element(list_entry(tmp, element_t, list));
        } else {
            indirect = &(*indirect)->next;
        }
    }
    return true;
}

/* Swap every two adjacent nodes */
void q_swap(struct list_head *head)
{
    // https://leetcode.com/problems/swap-nodes-in-pairs/
    if (!head || list_empty(head))
        return;

    struct list_head **indirect = &head->next;
    while (*indirect != head && (*indirect)->next != head) {
        // swap next pointer
        struct list_head **sec = &(*indirect)->next;
        *indirect = *sec;
        *sec = (*sec)->next;
        (*indirect)->next = (*indirect)->prev;

        // swap prev pointer
        indirect = &(*sec)->prev;
        sec = &(*indirect)->prev;
        *indirect = *sec;
        *sec = (*sec)->prev;
        (*indirect)->prev = (*sec)->next;
        indirect = &(*indirect)->next;
    }
}

/* Reverse elements in queue */
void q_reverse(struct list_head *head)
{
    if (!head || list_empty(head) || list_is_singular(head))
        return;

    struct list_head *curr = head;
    do {
        struct list_head *tmp = curr->next;
        curr->next = curr->prev;
        curr->prev = tmp;
        curr = curr->prev;
    } while (curr != head);
}

// return head, head->prev point to tail
static struct list_head *mergeTwoSortedList(struct list_head *l1,
                                            struct list_head *l2)
{
    struct list_head *head = NULL, **ptr = &head, **node;
    for (node = NULL; l1 && l2; *node = (*node)->next) {
        node = strcmp(list_entry(l1, element_t, list)->value,
                      list_entry(l2, element_t, list)->value) < 0
                   ? &l1
                   : &l2;
        *ptr = *node;
        ptr = &(*ptr)->next;
    }
    *ptr = (struct list_head *) ((uintptr_t) l1 | (uintptr_t) l2);

    // find tail
    while ((*ptr)->next)
        ptr = &(*ptr)->next;

    // head->prev point to head
    head->prev = *ptr;

    return head;
}

static struct list_head *list_split(struct list_head *head, int n)
{
    struct list_head **curr = &head;
    struct list_head *remain;
    for (int i = 0; i < n && *curr; i++) {
        curr = &(*curr)->next;
    }
    remain = *curr;
    *curr = NULL;
    return remain;
}

void list_mergesort(struct list_head *head, int n)
{
    head->prev->next = NULL;
    for (int i = 1; i < n; i *= 2) {
        struct list_head **l1 = &head->next;
        while (*l1) {
            struct list_head *l2 = list_split(*l1, i);
            struct list_head *remain = list_split(l2, i);

            *l1 = mergeTwoSortedList(*l1, l2);
            (*l1)->prev->next = remain;

            l1 = &(*l1)->prev->next;
        }
    }

    // rebuild prev pointer and circular list
    head->prev = head->next->prev;
    head->prev->next = head;
    head->next->prev = head;
    struct list_head *curr = head->next;
    while (curr != head) {
        curr->next->prev = curr;
        curr = curr->next;
    }
}

/* Sort elements of queue in ascending order */
void q_sort(struct list_head *head)
{
    if (!head || list_empty(head) || head->prev == head->next)
        return;
    int n = q_size(head);
    list_mergesort(head, n);
}
