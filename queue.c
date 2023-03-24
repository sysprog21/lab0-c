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
    struct list_head *q = malloc(sizeof(struct list_head));
    if (list_empty(q) == 0)
        INIT_LIST_HEAD(q);
    return q;
}

/* Free all storage used by queue */
void q_free(struct list_head *l)
{
    if (l == NULL)
        return;
    element_t *n, *s;
    list_for_each_entry_safe (n, s, l, list)
        q_release_element(n);
    free(l);
}
/*
 * New an element for s,
 * It will allocate memory for s
 * Return null if allocation failed.
 */
element_t *part_ins(char *s)
{
    element_t *new_ele = malloc(sizeof(element_t));
    if (new_ele == NULL)
        return NULL;
    new_ele->value = strdup(s);
    if (new_ele->value == NULL) {
        free(new_ele);
        return NULL;
    }
    return new_ele;
}

bool q_insert_head(struct list_head *head, char *s)
{
    if (head == NULL)
        return false;
    element_t *new_ele = part_ins(s);
    if (new_ele == NULL)
        return false;
    list_add(&new_ele->list, head);
    return true;
}

bool q_insert_tail(struct list_head *head, char *s)
{
    if (head == NULL)
        return false;
    element_t *new_ele = part_ins(s);
    if (new_ele == NULL)
        return false;
    list_add_tail(&new_ele->list, head);
    return true;
}


/* Remove an element from head of queue */
element_t *q_remove_head(struct list_head *head, char *sp, size_t bufsize)
{
    if (list_empty(head) != 0)
        return NULL;
    // else if (list_empty(head) == 0)
    // puts("queue not empty");
    element_t *n = list_first_entry(head, element_t, list);
    list_del(head->next);
    if (sp != NULL) {
        strncpy(sp, n->value, bufsize - 1);
        sp[bufsize - 1] = '\0';
    }
    return n;
}

// 修改一次兩處： (1)head 要做檢查。 (2)可以使用 q_remove_head
element_t *q_remove_tail(struct list_head *head, char *sp, size_t bufsize)
{
    if (!head || !head->prev)
        return NULL;
    return q_remove_head(head->prev->prev, sp, bufsize);
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
    if (!head || list_empty(head)) {
        return false;
    }
    struct list_head *h = head->next;
    struct list_head *t = head->prev;
    while (true) {
        if (h == t || h->next == t) {
            list_del(h);
            q_release_element(list_entry(h, element_t, list));
            break;
        } else {
            puts("del tail case.");
            break;
        }
        h = h->next;
        t = t->prev;
    }
    return true;
}

/* Delete all nodes that have duplicate string */
bool q_delete_dup(struct list_head *head)
{
    // https://leetcode.com/problems/remove-duplicates-from-sorted-list-ii/
    return true;
}

/* Swap every two adjacent nodes */
void q_swap(struct list_head *head)
{
    // https://leetcode.com/problems/swap-nodes-in-pairs/
}

/* Reverse elements in queue */
void q_reverse(struct list_head *head) {}

/* Reverse the nodes of the list k at a time */
void q_reverseK(struct list_head *head, int k)
{
    // https://leetcode.com/problems/reverse-nodes-in-k-group/
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
