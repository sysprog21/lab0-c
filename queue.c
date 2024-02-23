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
    if (head) {
        INIT_LIST_HEAD(head);
    }
    return head;
}

/* Free all storage used by queue */
void q_free(struct list_head *l)
{
    if (!l)
        return;
    element_t *entry, *safe;
    list_for_each_entry_safe (entry, safe, l, list) {
        if (entry->value)
            free(entry->value);
        free(entry);
    }
    free(l);
}

/* Insert an element at head of queue */
bool q_insert_head(struct list_head *head, char *s)
{
    if (!head || !s)
        return false;
    element_t *entry = malloc(sizeof(element_t));
    if (!entry)
        return false;
    int len = (int) (strlen(s) + 1);
    entry->value = malloc(len);
    if (!entry->value) {
        free(entry);
        return false;
    }
    strncpy(entry->value, s, len - 1);
    entry->value[len - 1] = '\0';
    list_add(&(entry->list), head);
    if (!entry->list.next || !entry->list.prev) {
        q_release_element(entry);
    }
    return true;
}

/* Insert an element at tail of queue */
bool q_insert_tail(struct list_head *head, char *s)
{
    if (!head)
        return false;
    return q_insert_head(head->prev, s);
}

/* Remove an element from head of queue */
element_t *q_remove_head(struct list_head *head, char *sp, size_t bufsize)
{
    if (!head || list_empty(head))
        return NULL;
    element_t *entry = list_entry(head->next, element_t, list);
    if (!entry)
        return NULL;
    list_del(&(entry->list));
    if (entry->value && sp) {
        size_t len = strlen(entry->value) > (bufsize - 1)
                         ? (bufsize - 1)
                         : strlen(entry->value);
        memcpy(sp, entry->value, len);
        *(sp + len) = '\0';
    }
    return entry;
}

/* Remove an element from tail of queue */
element_t *q_remove_tail(struct list_head *head, char *sp, size_t bufsize)
{
    if (!head || list_empty(head))
        return NULL;
    element_t *entry = list_entry(head->prev, element_t, list);
    if (!entry)
        return NULL;
    list_del(&(entry->list));
    if (entry->value && sp) {
        size_t len = strlen(entry->value) > (bufsize - 1)
                         ? (bufsize - 1)
                         : strlen(entry->value);
        memcpy(sp, entry->value, len);
        *(sp + len) = '\0';
    }
    return entry;
}

/* Return number of elements in queue */
int q_size(struct list_head *head)
{
    if (!head || list_empty(head))
        return 0;
    int count = 0;
    struct list_head *node;
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
    struct list_head *fast, *slow;
    slow = fast = head->next;
    for (; fast != head && fast->next != head; fast = fast->next->next) {
        slow = slow->next;
    }
    element_t *entry = list_entry(slow, element_t, list);
    list_del(&(entry->list));
    q_release_element(entry);
    return true;
}

/* Delete all nodes that have duplicate string */
bool q_delete_dup(struct list_head *head)
{
    // https://leetcode.com/problems/remove-duplicates-from-sorted-list-ii/
    if (!head)
        return false;
    q_sort(head, false);
    element_t *entry;
    struct list_head *safe, *node;
    char *sp = NULL;
    node = head->next;
    safe = node->next;
    for (; safe != head; node = safe, safe = node->next) {
        entry = list_entry(node, element_t, list);
        if (strcmp((entry->value),
                   (list_entry(safe, element_t, list)->value)) == 0) {
            list_del(node);
            if (sp)
                free(sp);
            sp = entry->value;
            free(entry);
        } else if (sp) {
            if (strcmp((entry->value), (sp)) == 0) {
                list_del(node);
                q_release_element(entry);
            }
        }
    }
    if ((sp) && node != head) {
        entry = list_entry(node, element_t, list);
        if (strcmp((entry->value), (sp)) == 0) {
            list_del(node);
            q_release_element(entry);
        }
        free(sp);
    }

    return true;
}

/* Swap every two adjacent nodes */
void q_swap(struct list_head *head)
{
    // https://leetcode.com/problems/swap-nodes-in-pairs/
    if (!head || list_is_singular(head))
        return;
    struct list_head *pair1, *pair2;
    pair1 = head->next;
    pair2 = pair1->next;
    for (; pair1 != head && pair2 != head;
         pair1 = pair1->next, pair2 = pair1->next) {
        list_move(pair2, pair1->prev);
    }
}

/* Reverse elements in queue */
void q_reverse(struct list_head *head)
{
    if (!head || list_empty(head))
        return;
    struct list_head *node, *safe;
    list_for_each_safe (node, safe, head)
        list_move(node, head);
}

/* Reverse the nodes of the list k at a time */
void q_reverseK(struct list_head *head, int k)
{
    // https://leetcode.com/problems/reverse-nodes-in-k-group/
    if (!head || list_empty(head))
        return;
    int count = 0;
    struct list_head group, *group_head = &group;
    INIT_LIST_HEAD(group_head);
    struct list_head *node = head->next, *safe = node->next;
    for (; safe != (head); node = safe, safe = safe->next) {
        if (count < k) {
            list_move_tail(node, group_head);
            count++;
        }
        if (count == k) {
            q_reverse(group_head);
            list_splice_tail_init(group_head, safe);
            count = 0;
        }
    }
    if (count < k) {
        list_move_tail(node, group_head);
        count++;
    }
    if (count == k) {
        q_reverse(group_head);
    }
    list_splice_tail_init(group_head, head);
}

struct list_head *merge_two_q(struct list_head *left,
                              struct list_head *right,
                              bool descend)
{
    struct list_head *merge_head = NULL, **ptnext = &merge_head,
                     **ptnode = NULL;
    while (left && right) {
        element_t *entry1 = list_entry(left, element_t, list),
                  *entry2 = list_entry(right, element_t, list);
        if (descend ? (strcmp(entry1->value, entry2->value) < 0)
                    : (strcmp(entry1->value, entry2->value) > 0)) {
            ptnode = &(right);
        } else {
            ptnode = &(left);
        }
        *ptnext = *ptnode;
        ptnext = &(*ptnext)->next;
        *ptnode = (*ptnode)->next;
    }
    if (left) {
        *ptnext = left;
    } else {
        *ptnext = right;
    }
    return merge_head;
}
struct list_head *q_divide_helf(struct list_head *head, bool descend)
{
    if (!head || !head->next)
        return head;
    struct list_head *slow, *fast;
    slow = fast = head;
    for (; fast && fast->next; fast = fast->next->next)
        slow = slow->next;
    slow->prev->next = NULL;
    struct list_head *left = q_divide_helf(head, descend);
    struct list_head *right = q_divide_helf(slow, descend);
    return merge_two_q(left, right, descend);
}

/* Sort elements of queue in ascending/descending order */
void q_sort(struct list_head *head, bool descend)
{
    if (!head || list_empty(head) || list_is_singular(head))
        return;
    head->prev->next = NULL;
    head->next = q_divide_helf(head->next, descend);
    struct list_head *node = head, *safe = node->next;
    for (; safe != NULL; node = safe, safe = node->next) {
        safe->prev = node;
    }
    node->next = head;
    head->prev = node;
}

/* Remove every node which has a node with a strictly less value anywhere to
 * the right side of it */
int q_ascend(struct list_head *head)
{
    // https://leetcode.com/problems/remove-nodes-from-linked-list/
    if (!head || list_empty(head))
        return 0;
    if (list_is_singular(head))
        return 1;
    struct list_head *node = head->next, *safe = node->next;
    for (; safe != head; node = safe, safe = node->next) {
        element_t *entry, *safe_entry;
        entry = list_entry(node, element_t, list);
        safe_entry = list_entry(safe, element_t, list);
        while (node != head &&
               strcmp((entry->value), (safe_entry->value)) > 0) {
            list_del(node);
            q_release_element(entry);
            node = safe->prev;
            if (node != head)
                entry = list_entry(node, element_t, list);
        }
    }
    return q_size(head);
}

/* Remove every node which has a node with a strictly greater value anywhere to
 * the right side of it */
int q_descend(struct list_head *head)
{
    // https://leetcode.com/problems/remove-nodes-from-linked-list/
    if (!head || list_empty(head))
        return 0;
    if (list_is_singular(head))
        return 1;
    struct list_head *node = head->next, *safe = node->next;
    for (; safe != head; node = safe, safe = node->next) {
        element_t *entry, *safe_entry;
        entry = list_entry(node, element_t, list);
        safe_entry = list_entry(safe, element_t, list);
        while (node != head &&
               strcmp((entry->value), (safe_entry->value)) < 0) {
            list_del(node);
            q_release_element(entry);
            node = safe->prev;
            if (node == head)
                break;
            entry = list_entry(node, element_t, list);
        }
    }
    return q_size(head);
}

/* Merge all the queues into one sorted queue, which is in ascending/descending
 * order */
int q_merge(struct list_head *head, bool descend)
{
    // https://leetcode.com/problems/merge-k-sorted-lists/
    if (!head || list_empty(head))
        return 0;
    if (list_is_singular(head))
        return q_size(list_entry(head->next, queue_contex_t, chain)->q);

    struct list_head *main_q, *ptr_q;
    main_q = list_entry(head->next, queue_contex_t, chain)->q;
    main_q->prev->next = NULL;
    ptr_q = head->next->next;
    int size = 0;
    while (ptr_q != head) {
        list_entry(ptr_q, queue_contex_t, chain)->q->prev->next = NULL;
        main_q->next = merge_two_q(
            main_q->next, list_entry(ptr_q, queue_contex_t, chain)->q->next,
            descend);
        INIT_LIST_HEAD(list_entry(ptr_q, queue_contex_t, chain)->q);
        ptr_q = ptr_q->next;
    }
    struct list_head *node = main_q, *safe = node->next;
    for (; safe != NULL; node = safe, safe = node->next) {
        safe->prev = node;
    }
    node->next = main_q;
    main_q->prev = node;
    size = q_size(main_q);
    return size;
}
