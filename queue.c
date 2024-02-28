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
    struct list_head *head =
        (struct list_head *) malloc(sizeof(struct list_head));
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
    element_t *cur, *safe = NULL;
    list_for_each_entry_safe (cur, safe, l, list) {
        if (cur->value)
            free(cur->value);
        free(cur);
    }
    free(l);
}

/* Insert an element at head of queue */
bool q_insert_head(struct list_head *head, char *s)
{
    element_t *new_node = (element_t *) malloc(sizeof(element_t));
    if (!new_node)
        return false;
    new_node->value = strdup(s);
    if (!new_node->value) {
        free(new_node);
        return false;
    }

    list_add(&new_node->list, head);
    return true;
}

/* Insert an element at tail of queue */
bool q_insert_tail(struct list_head *head, char *s)
{
    element_t *new_node = (element_t *) malloc(sizeof(element_t));
    if (!new_node)
        return false;

    new_node->value = strdup(s);
    if (!new_node->value) {
        free(new_node);
        return false;
    }

    list_add_tail(&new_node->list, head);
    return true;
}

/* Remove an element from head of queue */
element_t *q_remove_head(struct list_head *head, char *sp, size_t bufsize)
{
    if (!head || list_empty(head) || !sp)
        return NULL;
    struct list_head *first_element = head->next;
    list_del(first_element);
    INIT_LIST_HEAD(first_element);
    element_t *entry = list_entry(first_element, __typeof__(*entry), list);
    strncpy(sp, entry->value, bufsize);
    sp[bufsize - 1] = '\0';
    return entry;
}

/* Remove an element from tail of queue */
element_t *q_remove_tail(struct list_head *head, char *sp, size_t bufsize)
{
    if (!head || list_empty(head) || !sp)
        return NULL;
    struct list_head *last_element = head->prev;
    list_del(last_element);
    INIT_LIST_HEAD(last_element);
    element_t *entry = list_entry(last_element, __typeof__(*entry), list);
    strncpy(sp, entry->value, bufsize);
    sp[bufsize - 1] = '\0';
    return entry;
}

/* Return number of elements in queue */
int q_size(struct list_head *head)
{
    if (!head)
        return 0;

    int len = 0;
    struct list_head *li;

    list_for_each (li, head)
        len++;
    return len;
}

/* Delete the middle node in queue */
bool q_delete_mid(struct list_head *head)
{
    // https://leetcode.com/problems/delete-the-middle-node-of-a-linked-list/
    if (!head)
        return false;

    struct list_head *slow = head;
    struct list_head *fast = head->next;
    while (fast != head && fast->next != head) {
        slow = slow->next;
        fast = fast->next->next;
    }
    struct list_head *temp = slow->next;
    list_del(temp);
    element_t *entry = list_entry(temp, __typeof__(*entry), list);
    free(entry->value);
    free(entry);
    return true;
}

/* Delete all nodes that have duplicate string */
bool q_delete_dup(struct list_head *head)
{
    // https://leetcode.com/problems/remove-duplicates-from-sorted-list-ii/
    if (!head)
        return false;
    element_t *cur_entry, *safe_entry;
    struct list_head *cmp, *temp;
    list_for_each_entry_safe (cur_entry, safe_entry, head, list) {
        bool flag = false;
        cmp = cur_entry->list.next;

        while (cmp != head) {
            element_t *cmp_entry =
                list_entry(cmp, __typeof__(*cmp_entry), list);
            temp = cmp->next;

            if (strcmp(cur_entry->value, cmp_entry->value) == 0) {
                flag = true;
                list_del(cmp);
                free(cmp_entry->value);
                free(cmp_entry);
            }
            cmp = temp;
        }
        if (flag) {
            safe_entry =
                list_entry(cur_entry->list.next, __typeof__(*cur_entry), list);
            list_del(&cur_entry->list);
            free(cur_entry->value);
            free(cur_entry);
        }
    }
    return true;
}

/* Swap every two adjacent nodes */
void q_swap(struct list_head *head)
{
    // https://leetcode.com/problems/swap-nodes-in-pairs/
    struct list_head *cur = head;
    struct list_head *first = cur->next;
    struct list_head *second = first->next;
    while (first != head && second != head) {
        first->next = second->next;
        second->next = first;
        second->prev = first->prev;
        first->prev = second;
        cur->next = second;

        cur = cur->next->next;
        first = cur->next;
        second = first->next;
    }
}

/* Reverse elements in queue */
void q_reverse(struct list_head *head)
{
    struct list_head *temp = NULL, *cur = head;

    do {
        temp = cur->prev;
        cur->prev = cur->next;
        cur->next = temp;
        cur = cur->prev;

    } while (cur != head);
}

/* Reverse the nodes of the list k at a time */
void q_reverseK(struct list_head *head, int k)
{
    // https://leetcode.com/problems/reverse-nodes-in-k-group/
    if (q_size(head) < k || list_empty(head))
        return;

    struct list_head *cur = head;
    struct list_head *first = NULL, *tail = NULL;
    int len = q_size(head);
    while (len >= k) {
        first = cur->next;
        tail = cur;
        for (int i = 0; i < k; i++)
            tail = tail->next;

        cur->next = tail->next;
        tail->next->prev = cur;

        first->prev = tail;
        tail->next = first;
        q_reverse(first);

        tail->prev = cur;
        first->next = cur->next;
        cur->next->prev = first;
        cur->next = tail;

        cur = first;
        len -= k;
    }
}

/* Sort elements of queue in ascending/descending order */
void q_sort(struct list_head *head, bool descend) {}

/* Remove every node which has a node with a strictly less value anywhere to
 * the right side of it */
int q_ascend(struct list_head *head)
{
    // https://leetcode.com/problems/remove-nodes-from-linked-list/
    return 0;
}

/* Remove every node which has a node with a strictly greater value anywhere to
 * the right side of it */
int q_descend(struct list_head *head)
{
    // https://leetcode.com/problems/remove-nodes-from-linked-list/
    return 0;
}

/* Merge all the queues into one sorted queue, which is in ascending/descending
 * order */
int q_merge(struct list_head *head, bool descend)
{
    // https://leetcode.com/problems/merge-k-sorted-lists/
    return 0;
}
