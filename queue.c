#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list_sort.h"
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
void q_free(struct list_head *head)
{
    if (!head)
        return;
    element_t *entry, *safe;
    list_for_each_entry_safe (entry, safe, head, list)
        q_release_element(entry);
    free(head);
}

/* Insert an element at head of queue */
bool q_insert_head(struct list_head *head, char *s)
{
    if (!head)
        return false;
    element_t *new = malloc(sizeof(element_t));
    if (!new)
        return false;
    new->value = strdup(s);
    if (!new->value) {
        free(new);
        return false;
    }
    list_add(&new->list, head);
    return true;
}

/* Insert an element at tail of queue */
bool q_insert_tail(struct list_head *head, char *s)
{
    if (!head)
        return false;
    element_t *new = malloc(sizeof(element_t));
    if (!new)
        return false;
    new->value = strdup(s);
    if (!new->value) {
        free(new);
        return false;
    }
    list_add_tail(&new->list, head);
    return true;
}

/* Remove an element from head of queue */
element_t *q_remove_head(struct list_head *head, char *sp, size_t bufsize)
{
    if (!head || list_empty(head))
        return NULL;
    element_t *first = list_first_entry(head, element_t, list);
    list_del(&first->list);
    if (sp && first->value) {
        strncpy(sp, first->value, bufsize - 1);
        sp[bufsize - 1] = '\0';
    }
    return first;
}

/* Remove an element from tail of queue */
element_t *q_remove_tail(struct list_head *head, char *sp, size_t bufsize)
{
    if (!head || list_empty(head))
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
    if (!head || list_empty(head))
        return false;
    struct list_head *slow, *fast;
    slow = fast = head->next;
    while (fast->next != head && fast->next->next != head) {
        fast = fast->next->next;
        slow = slow->next;
    }
    if (fast->next != head) {
        slow = slow->next;
    }
    element_t *element = list_entry(slow, element_t, list);
    list_del(&element->list);
    q_release_element(element);
    return true;
}

/* Delete all nodes that have duplicate string */
bool q_delete_dup(struct list_head *head)
{
    // https://leetcode.com/problems/remove-duplicates-from-sorted-list-ii/
    if (!head)
        return false;
    bool flag = false;
    element_t *post, *cur;
    list_for_each_entry_safe (cur, post, head, list) {
        if (&post->list != head && !strcmp(cur->value, post->value)) {
            flag = true;
            list_del(&cur->list);
            q_release_element(cur);
        } else {
            if (flag) {
                list_del(&cur->list);
                q_release_element(cur);
                flag = false;
            }
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
    q_reverseK(head, 2);
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
    int count = k;
    struct list_head *node, *safe, *cut;
    cut = head;
    list_for_each_safe (node, safe, head) {
        if (count != 1) {
            count--;
        } else {
            LIST_HEAD(tmp);
            count = k;
            list_cut_position(&tmp, cut, node);
            q_reverse(&tmp);
            list_splice(&tmp, cut);
            cut = safe->prev;
        }
    }
}

void q_merge_two(struct list_head *left, struct list_head *right, bool descend)
{
    if (!left || !right)
        return;
    LIST_HEAD(tmp);
    while (!list_empty(left) && !list_empty(right)) {
        element_t *l = list_entry(left->next, element_t, list);
        element_t *r = list_entry(right->next, element_t, list);
        if (descend) {
            if (strcmp(l->value, r->value) >= 0) {
                list_move_tail(&l->list, &tmp);
            } else {
                list_move_tail(&r->list, &tmp);
            }
        } else {
            if (strcmp(l->value, r->value) <= 0) {
                list_move_tail(&l->list, &tmp);
            } else {
                list_move_tail(&r->list, &tmp);
            }
        }
    }
    if (list_empty(right)) {
        list_splice_tail_init(left, &tmp);
    } else {
        list_splice_tail_init(right, &tmp);
    }
    list_splice_init(&tmp, left);
}

void merge(struct list_head *l_head,
           struct list_head *r_head,
           struct list_head *head)
{
    while (!list_empty(l_head) && !list_empty(r_head)) {
        element_t *l_entry = list_entry(l_head->next, element_t, list);
        element_t *r_entry = list_entry(r_head->next, element_t, list);

        if (strcmp(l_entry->value, r_entry->value) <= 0)
            list_move_tail(l_head->next, head);
        else
            list_move_tail(r_head->next, head);
    }
    if (!list_empty(l_head))
        list_splice_tail_init(l_head, head);
    else
        list_splice_tail_init(r_head, head);
}

void merge_sort(struct list_head *head)
{
    if (!head || list_empty(head) || list_is_singular(head))
        return;

    // Find middle node
    struct list_head *slow = head, *fast = head;
    do {
        fast = fast->next->next;
        slow = slow->next;
    } while (fast != head && fast->next != head);

    LIST_HEAD(l_head);
    LIST_HEAD(r_head);

    // Split list into two parts - Left and Right
    list_splice_tail_init(head, &r_head);
    list_cut_position(&l_head, &r_head, slow);
    // Recursively split the left and right parts
    merge_sort(&l_head);
    merge_sort(&r_head);
    // Merge the left and right parts
    merge(&l_head, &r_head, head);
}

/* Compare two nodes in queue */
int cmp(const struct list_head *a, const struct list_head *b, bool descend)
{
    element_t *a_entry = list_entry(a, element_t, list);
    element_t *b_entry = list_entry(b, element_t, list);

    if (!descend)
        return strcmp(a_entry->value, b_entry->value) < 0 ? 0 : 1;
    else
        return strcmp(a_entry->value, b_entry->value) > 0 ? 0 : 1;
}

void q_sort2(struct list_head *head, bool descend)
{
    list_sort(head, cmp, descend);
}

/* Sort elements of queue in ascending/descending order */
void q_sort(struct list_head *head, bool descend)
{
    merge_sort(head);

    if (descend)
        q_reverse(head);
}
/* Sort elements of queue in ascending/descending order */
// void q_sort(struct list_head *head, bool descend)
// {
//     if (!head || list_empty(head) || list_is_singular(head))
//         return;
//     struct list_head *slow, *fast;
//     slow = fast = head->next;
//     while (fast->next != head && fast->next->next != head) {
//         fast = fast->next->next;
//         slow = slow->next;
//     }
//     LIST_HEAD(left);
//     list_cut_position(&left, head, slow);
//     q_sort(head, descend);
//     q_sort(&left, descend);
//     q_merge_two(head, &left, descend);
// }

/* Remove every node which has a node with a strictly less value anywhere to
 * the right side of it */
int q_ascend(struct list_head *head)
{
    // https://leetcode.com/problems/remove-nodes-from-linked-list/
    if (!head || list_empty(head) || list_is_singular(head))
        return 0;
    int count = 1;
    element_t *cur = list_entry(head->prev, element_t, list);
    while (cur->list.prev != head) {
        element_t *prev = list_entry(cur->list.prev, element_t, list);
        if (strcmp(prev->value, cur->value) > 0) {
            list_del(&prev->list);
            q_release_element(prev);
        } else {
            count++;
            cur = prev;
        }
    }
    return count;
}

/* Remove every node which has a node with a strictly greater value anywhere to
 * the right side of it */
int q_descend(struct list_head *head)
{
    // https://leetcode.com/problems/remove-nodes-from-linked-list/
    if (!head || list_empty(head) || list_is_singular(head))
        return 0;
    int count = 1;
    element_t *cur = list_entry(head->prev, element_t, list);
    while (cur->list.prev != head) {
        element_t *prev = list_entry(cur->list.prev, element_t, list);
        if (strcmp(prev->value, cur->value) < 0) {
            list_del(&prev->list);
            q_release_element(prev);
        } else {
            count++;
            cur = prev;
        }
    }
    return count;
}

/* Merge all the queues into one sorted queue, which is in ascending/descending
 * order */
int q_merge(struct list_head *head, bool descend)
{
    // https://leetcode.com/problems/merge-k-sorted-lists/
    if (!head || list_empty(head))
        return 0;
    if (list_is_singular(head))
        return q_size(list_first_entry(head, queue_contex_t, chain)->q);
    queue_contex_t *first = list_first_entry(head, queue_contex_t, chain);
    struct list_head *node, *safe;
    int count = 0;
    list_for_each_safe (node, safe, head) {
        if (safe != head) {
            queue_contex_t *second = list_entry(safe, queue_contex_t, chain);
            q_merge_two(first->q, second->q, descend);
            count = q_size(first->q);
        }
    }
    return count;
}

void q_shuffle(struct list_head *head)
{
    if (!head || list_empty(head) || list_is_singular(head))
        return;
    int len = q_size(head);
    struct list_head *new_head;
    struct list_head *new_tail = head->prev;
    while (len) {
        int index = rand() % len;
        new_head = head->next;
        while (index--) {
            new_head = new_head->next;
        }
        if (new_head == new_tail) {
            new_tail = new_tail->prev;
            len--;
            continue;
        }
        struct list_head *tmp = new_head->prev;
        list_move(new_head, new_tail);
        list_move(new_tail, tmp);
        new_tail = new_head->prev;
        len--;
    }
    return;
}