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
    struct list_head *q;

    q = (struct list_head *) malloc(sizeof(struct list_head));

    if (q == NULL) {
        return NULL;
    }

    INIT_LIST_HEAD(q);

    return q;
}

/* Free all storage used by queue */
void q_free(struct list_head *l)
{
    if (l == NULL) {
        return;
    }

    // struct list_head *li = l;
    // list_empty dose not return trun after this, the entry is in an undefined
    // state. According to The Linux Kernel API.
    //   while (list_empty(l) != false) {
    //       li = li->next;
    //     list_del(li);
    //   free(li);
    // }

    while (list_empty(l) == false) {
        element_t *tmp = q_remove_head(l, NULL, 0);
        if (tmp == NULL) {
            return;
        }
        q_release_element(tmp);
    }

    free(l);
}

/* Insert an element at head of queue */
bool q_insert_head(struct list_head *head, char *s)
{
    if (head == NULL) {
        return false;
    }

    element_t *ele = (element_t *) malloc(sizeof(element_t));

    ele->value = strdup(s);

    if (ele->value == NULL) {
        free(ele);
        return false;
    }

    list_add(&ele->list, head);

    return true;
}

/* Insert an element at tail of queue */
bool q_insert_tail(struct list_head *head, char *s)
{
    if (head == NULL) {
        return false;
    }

    element_t *ele = (element_t *) malloc(sizeof(element_t));

    ele->value = strdup(s);

    if (ele->value == NULL) {
        free(ele);
        return false;
    }

    list_add_tail(&ele->list, head);

    return true;
}

/* Remove an element from head of queue */
element_t *q_remove_head(struct list_head *head, char *sp, size_t bufsize)
{
    if (head == NULL) {
        return NULL;
    }

    element_t *ele = container_of(head->next, element_t, list);

    list_del(head->next);

    if (sp != NULL) {
        strncpy(sp, ele->value, bufsize - 1);
        sp[bufsize - 1] = '\0';
    }

    return ele;
}

/* Remove an element from tail of queue */
element_t *q_remove_tail(struct list_head *head, char *sp, size_t bufsize)
{
    element_t *ele = container_of(head->prev, element_t, list);

    list_del(head->prev);

    return ele;
}

/* Return number of elements in queue */
int q_size(struct list_head *head)
{
    if (!head) {
        return 0;
    }


    int len = 0;
    struct list_head *li;

    list_for_each (li, head)
        len++;
    return len;
}

/* Delete the middle node in queue */
bool q_delete_mid(struct list_head *head)
{
    if (head == NULL) {
        return false;
    }

    int len = q_size(head) / 2;
    element_t *ele;

    if (len == 0) {
        return false;
    }

    struct list_head *mid = head;

    while (len != 0) {
        mid = mid->next;
        --len;
    }

    ele = container_of(mid, element_t, list);

    mid->next = mid->prev;

    q_release_element(ele);

    // https://leetcode.com/problems/delete-the-middle-node-of-a-linked-list/
    return true;
}

/* Delete all nodes that have duplicate string */
bool q_delete_dup(struct list_head *head)
{
    // https://leetcode.com/problems/remove-duplicates-from-sorted-list-ii/
    if (head == NULL) {
        return false
    };

    if (head->next == head) {
        return false;
    }

    struct list_head *pre = head, *post = head->next;
    char *duplicate_value = '';

    while (post != head) {
        element_t *ele = container_of(post, element_t, list);
        element_t *ele_next = container_of(post->next, element_t, list);
        if (ele->value == ele_next->value || ele->value == duplicate_value) {
            duplicate_value = ele->value;
            q_release_element(ele);
        } else {
            pre->next = post;
            pre = pre->next;
        }
        post = post->next;
    }

    return true;
}

/* Swap every two adjacent nodes */
void q_swap(struct list_head *head)
{
    // https://leetcode.com/problems/swap-nodes-in-pairs/
    if (head == NULL) {
        return;
    }

    struct list_head *first = head, *tmp;
    // head -> 1 -> 2 -> 3
    while (first->next != head || first->next->next != head) {
        // head -> 2 -> 3 tmp = 1
        tmp = first->next;
        first->next = first->next->next;
        tmp->prev = first->next->prev;
        // head -> 2 -> 3
        //         1 -> 3
        tmp->next = first->next->next;
        tmp->next->prev = tmp;

        // head -> 2 -> 1 -> 3
        first->next->next = tmp;
        tmp->prev = first->next;

        first = first->next->next;
    }
}

/* Reverse elements in queue */
void q_reverse(struct list_head *head)
{
    if (head == NULL) {
        return;
    }

    int len = q_size(head);
    element_t *ele;

    while (len != 0) {
        ele = q_remove_head(head, NULL, 0);
        q_insert_tail(head, ele->value);
        len--;
    }
}

/* Sort elements of queue in ascending order */
void q_sort(struct list_head *head)
{
    if (head == NULL) {
        return;
    }

    

}
