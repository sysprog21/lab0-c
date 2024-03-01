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
    if (!head)
        return false;
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
    if (!head)
        return false;
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
    if (!head || list_empty(head))
        return NULL;

    element_t *first_entry =
        list_first_entry(head, __typeof__(*first_entry), list);
    list_del(&first_entry->list);

    if (sp) {
        strncpy(sp, first_entry->value, bufsize);
        sp[bufsize - 1] = '\0';
    }
    return first_entry;
}

/* Remove an element from tail of queue */
element_t *q_remove_tail(struct list_head *head, char *sp, size_t bufsize)
{
    if (!head || list_empty(head))
        return NULL;

    element_t *last_entry =
        list_last_entry(head, __typeof__(*last_entry), list);
    list_del(&last_entry->list);

    if (sp) {
        strncpy(sp, last_entry->value, bufsize);
        sp[bufsize - 1] = '\0';
    }
    return last_entry;
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
    if (!head || q_size(head) <= 1)
        return;
    struct list_head *cur = head;
    struct list_head *first = cur->next;
    struct list_head *second = first->next;
    while (first != head && second != head) {
        first->next = second->next;
        second->prev = first->prev;
        second->next = first;
        first->prev = second;
        cur->next = second;
        first->next->prev = first;

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

void q_bubble_sort(struct list_head *head)
{
    element_t *cur_entry, *next_entry;
    struct list_head *cur;

    int len = q_size(head);

    for (int i = len; i > 0; i--) {
        cur = head->next;
        for (int j = 0; j < i - 1; j++) {
            cur_entry = list_entry(cur, __typeof__(*cur_entry), list);
            next_entry = list_entry(cur->next, __typeof__(*next_entry), list);
            if (strcmp(cur_entry->value, next_entry->value) > 0) {
                cur_entry->list.next = next_entry->list.next;
                next_entry->list.prev = cur_entry->list.prev;
                next_entry->list.next = &cur_entry->list;
                cur_entry->list.prev = &next_entry->list;
                next_entry->list.prev->next = &next_entry->list;
                cur_entry->list.next->prev = &cur_entry->list;
            } else
                cur = cur->next;
        }
    }
}

/*void printList(struct list_head *head){
    element_t *cur;
    list_for_each_entry(cur,head->prev,list){
        printf("%s => ",cur->value);
    }
    printf("%s",cur->value);
    printf("\n");
}*/

struct list_head *merge(struct list_head *l1, struct list_head *l2)
{
    struct list_head l1_head, l2_head, result;
    INIT_LIST_HEAD(&result);
    INIT_LIST_HEAD(&l1_head);
    INIT_LIST_HEAD(&l2_head);

    list_add_tail(&l1_head, l1);
    list_add_tail(&l2_head, l2);

    int l1_len = q_size(&l1_head);
    int l2_len = q_size(&l2_head);

    struct list_head *cur;

    while (l1_len != 0 && l2_len != 0) {
        element_t *l1_entry = list_entry(l1, __typeof__(*l1_entry), list);
        element_t *l2_entry = list_entry(l2, __typeof__(*l2_entry), list);

        if (strcmp(l1_entry->value, l2_entry->value) <= 0) {
            cur = l1;
            list_del(l1);
            l1 = l1->next;
            l1_len--;
            list_add_tail(cur, &result);
        } else {
            cur = l2;
            list_del(l2);
            l2 = l2->next;
            l2_len--;
            list_add_tail(cur, &result);
        }
    }

    if (l2_len > 0)
        list_splice_tail(&l2_head, &result);


    if (l1_len > 0)
        list_splice_tail(&l1_head, &result);


    cur = result.next;
    list_del(&result);
    return cur;
}

struct list_head *mergeSortList(struct list_head *head)
{
    if (!head || head->next == head)
        return head;

    struct list_head *fast = head->next;
    struct list_head *slow = head;

    while (fast != head && fast->next != head) {
        slow = slow->next;
        fast = fast->next->next;
    }

    // split two list
    fast = slow->next;

    fast->prev = head->prev;
    fast->prev->next = fast;

    slow->next = head;
    head->prev = slow;

    struct list_head *l1 = mergeSortList(head);
    struct list_head *l2 = mergeSortList(fast);

    return merge(l1, l2);
}


/* Sort elements of queue in ascending/descending order */
void q_sort(struct list_head *head, bool descend)
{
    if (!head || q_size(head) <= 1)
        return;

    // Bubble sort
    // q_bubble_sort(head);

    struct list_head *first = head->next;
    list_del_init(head);

    struct list_head *result = mergeSortList(first);
    list_add_tail(head, result);
    if (descend)
        q_reverse(head);
}

/* Remove every node which has a node with a strictly less value anywhere to
 * the right side of it */
int q_ascend(struct list_head *head)
{
    // https://leetcode.com/problems/remove-nodes-from-linked-list/
    if (!head || q_size(head) <= 1)
        return q_size(head);

    element_t *cur, *cur_safe, *cmp_entry;
    struct list_head *cmp_node;
    list_for_each_entry_safe (cur, cur_safe, head, list) {
        bool del_flag = false;

        cmp_node = cur->list.next;
        while (cmp_node != head) {
            cmp_entry = list_entry(cmp_node, __typeof__(*cmp_entry), list);
            if (strcmp(cur->value, cmp_entry->value) > 0) {
                del_flag = true;
                break;
            }
            cmp_node = cmp_node->next;
        }
        if (del_flag) {
            list_del(&cur->list);
            free(cur->value);
            free(cur);
        }
    }
    return q_size(head);
}

/* Remove every node which has a node with a strictly greater value anywhere to
 * the right side of it */
int q_descend(struct list_head *head)
{
    // https://leetcode.com/problems/remove-nodes-from-linked-list/
    if (!head || q_size(head) <= 1)
        return q_size(head);

    element_t *cur, *cur_safe, *cmp_entry;
    struct list_head *cmp_node;
    list_for_each_entry_safe (cur, cur_safe, head, list) {
        bool del_flag = false;

        cmp_node = cur->list.next;
        while (cmp_node != head) {
            cmp_entry = list_entry(cmp_node, __typeof__(*cmp_entry), list);
            if (strcmp(cur->value, cmp_entry->value) < 0) {
                del_flag = true;
                break;
            }
            cmp_node = cmp_node->next;
        }
        if (del_flag) {
            list_del(&cur->list);
            free(cur->value);
            free(cur);
        }
    }

    return q_size(head);
}

/* Merge all the queues into one sorted queue, which is in ascending/descending
 * order */
int q_merge(struct list_head *head, bool descend)
{
    // https://leetcode.com/problems/merge-k-sorted-lists/
    return 0;
}
