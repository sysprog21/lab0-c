
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

    if (ele == NULL) {
        return false;
    }
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

    if (ele == NULL) {
        return false;
    }

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
    if (head == NULL || head == head->next) {
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
    if (head == NULL || head == head->next) {
        return NULL;
    }

    element_t *ele = container_of(head->prev, element_t, list);

    list_del(head->prev);

    if (sp != NULL) {
        strncpy(sp, ele->value, bufsize - 1);
        sp[bufsize - 1] = '\0';
    }

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

    struct list_head *mid = head->next;
    while (len != 0) {
        mid = mid->next;
        --len;
    }

    ele = container_of(mid, element_t, list);

    list_del(mid);

    q_release_element(ele);

    // https://leetcode.com/problems/delete-the-middle-node-of-a-linked-list/
    return true;
}

/* Delete all nodes that have duplicate string */
bool q_delete_dup(struct list_head *head)
{
    // https://leetcode.com/problems/remove-duplicates-from-sorted-list-ii/
    if (head == NULL || head->next == head) {
        return false;
    }

    struct list_head *post = head->next;
    char *duplicate_value;
    while (post != head && post->next != head) {
        element_t *ele = container_of(post, element_t, list);
        element_t *ele_next = container_of(post->next, element_t, list);
        // printf("ele = %s\n", ele->value);
        // printf("ele_next = %s\n", ele_next->value);
        if (strcmp(ele->value, ele_next->value) == 0) {
            // printf("%s", ele->value);
            duplicate_value = ele->value;
            while (post->next != head) {
                element_t *ele_dul = container_of(post->next, element_t, list);

                if (strcmp(ele_dul->value, duplicate_value) == 0) {
                    list_del(post->next);
                    q_release_element(ele_dul);
                } else {
                    break;
                }
            }
            post = post->next;
            list_del(post->prev);
            q_release_element(ele);
        } else {
            post = post->next;
        }
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
    while (first->next != head && first->next->next != head) {
        // head -> 2 -> 3 tmp = 1
        tmp = first->next;
        first->next = first->next->next;
        first->next->prev = tmp->prev;
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

void swap(element_t *pre, element_t *post)
{
    char *tmp = pre->value;
    pre->value = post->value;
    post->value = tmp;
}

/* Reverse elements in queue */
void q_reverse(struct list_head *head)
{
    if (head == NULL) {
        return;
    }

    int len = q_size(head) / 2;
    if (len == 0) {
        return;
    }

    struct list_head *pre = head->next;
    struct list_head *post = head->prev;

    while (len != 0) {
        element_t *pre_ele = container_of(pre, element_t, list);
        element_t *post_ele = container_of(post, element_t, list);
        swap(pre_ele, post_ele);
        pre = pre->next;
        post = post->prev;
        len--;

        // ele = q_remove_head(head, NULL, 0);
        // printf("%s", ele->value);
        // list_add(&ele->list, head);
        // q_insert_tail(head, ele->value);
    }
}

struct list_head *split(struct list_head *head)
{
    struct list_head *fast = head->next, *slow = head;

    while (fast && fast->next) {
        fast = fast->next->next;
        slow = slow->next;
    }
    struct list_head *tmp = slow->next;
    slow->next = NULL;
    return tmp;
}

struct list_head *merge1(struct list_head *first, struct list_head *second)
{
    if (!second)
        return first;
    if (!first) {
        return second;
    }
    //  printf("merge");
    element_t *ele_first = container_of(first, element_t, list);
    element_t *ele_second = container_of(second, element_t, list);

    if (strcmp(ele_first->value, ele_second->value) < 0) {
        first->next = merge1(first->next, second);
        // first->next->prev = first;
        // first->prev = NULL;
        // printf("first");
        return first;
    } else {
        second->next = merge1(first, second->next);
        // second->next->prev = second;
        // second->prev = NULL;
        // printf("second");
        return second;
    }
}

struct list_head *merge_sort(struct list_head *head)
{
    if (head == NULL || head->next == NULL) {
        return head;
    }

    struct list_head *second = split(head);

    head = merge_sort(head);
    second = merge_sort(second);

    return merge1(head, second);
}


typedef unsigned char u8;
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
typedef int
    __attribute__((nonnull(2, 3))) (*list_cmp_func_t)(void *,
                                                      const struct list_head *,
                                                      const struct list_head *);
int sort_comp(void *p, const struct list_head *a, const struct list_head *b)
{
    return strcmp(list_entry(a, element_t, list)->value,
                  list_entry(b, element_t, list)->value);
}


__attribute__((nonnull(2, 3, 4))) static struct list_head *
merge(void *priv, list_cmp_func_t cmp, struct list_head *a, struct list_head *b)
{
    struct list_head *head = NULL, **tail = &head;

    for (;;) {
        /* if equal, take 'a' -- important for sort stability */
        if (cmp(priv, a, b) <= 0) {
            *tail = a;
            tail = &a->next;
            a = a->next;
            if (!a) {
                *tail = b;
                break;
            }
        } else {
            *tail = b;
            tail = &b->next;
            b = b->next;
            if (!b) {
                *tail = a;
                break;
            }
        }
    }
    return head;
}

__attribute__((nonnull(2, 3, 4, 5))) static void merge_final(
    void *priv,
    list_cmp_func_t cmp,
    struct list_head *head,
    struct list_head *a,
    struct list_head *b)
{
    struct list_head *tail = head;
    u8 count = 0;

    for (;;) {
        /* if equal, take 'a' -- important for sort stability */
        if (cmp(priv, a, b) <= 0) {
            tail->next = a;
            a->prev = tail;
            tail = a;
            a = a->next;
            if (!a)
                break;
        } else {
            tail->next = b;
            b->prev = tail;
            tail = b;
            b = b->next;
            if (!b) {
                b = a;
                break;
            }
        }
    }

    /* Finish linking remainder of list b on to tail */
    tail->next = b;
    do {
        /*
         *       * If the merge is highly unbalanced (e.g. the input is
         *               * already sorted), this loop may run many iterations.
         *                       * Continue callbacks to the client even though
         * no
         *                               * element comparison is needed, so the
         * client's cmp()
         *                                       * routine can invoke
         * cond_resched() periodically.
         *                                               */
        if (unlikely(!++count))
            cmp(priv, b, b);
        b->prev = tail;
        tail = b;
        b = b->next;
    } while (b);

    /* And the final links to make a circular doubly-linked list */
    tail->next = head;
    head->prev = tail;
}

__attribute__((nonnull(2, 3))) void list_sort(void *priv,
                                              struct list_head *head,
                                              list_cmp_func_t cmp)
{
    struct list_head *list = head->next, *pending = NULL;
    size_t count = 0; /* Count of pending */

    if (list == head->prev) /* Zero or one elements */
        return;

    /* Convert to a null-terminated singly-linked list. */
    head->prev->next = NULL;

    /*
     *   * Data structure invariants:
     *       * - All lists are singly linked and null-terminated; prev
     *           *   pointers are not maintained.
     *               * - pending is a prev-linked "list of lists" of sorted
     *                   *   sublists awaiting further merging.
     *                       * - Each of the sorted sublists is power-of-two in
     * size.
     *                           * - Sublists are sorted by size and age,
     * smallest & newest at front.
     *                               * - There are zero to two sublists of each
     * size.
     *                                   * - A pair of pending sublists are
     * merged as soon as the number
     *                                       *   of following pending elements
     * equals their size (i.e.
     *                                           *   each time count reaches an
     * odd multiple of that size).
     *                                               *   That ensures each later
     * final merge will be at worst 2:1.
     *                                                   * - Each round consists
     * of:
     *                                                       *   - Merging the
     * two sublists selected by the highest bit
     *                                                           *     which
     * flips when count is incremented, and
     *                                                               *   -
     * Adding an element from the input as a size-1 sublist.
     *                                                                   */
    do {
        size_t bits;
        struct list_head **tail = &pending;

        /* Find the least-significant clear bit in count */
        for (bits = count; bits & 1; bits >>= 1)
            tail = &(*tail)->prev;
        /* Do the indicated merge */
        if (likely(bits)) {
            struct list_head *a = *tail, *b = a->prev;

            a = merge(priv, cmp, b, a);
            /* Install the merged result in place of the inputs */
            a->prev = b->prev;
            *tail = a;
        }

        /* Move one element from input list to pending */
        list->prev = pending;
        pending = list;
        list = list->next;
        pending->next = NULL;
        count++;
    } while (list);

    /* End of input; merge together all the pending lists. */
    list = pending;
    pending = pending->prev;
    for (;;) {
        struct list_head *next = pending->prev;

        if (!next)
            break;
        list = merge(priv, cmp, pending, list);
        pending = next;
    }
    /* The final merge, rebuilding prev links */
    merge_final(priv, cmp, head, pending, list);
}

/* Sort elements of queue in ascending order */
void q_sort(struct list_head *head)
{
    if (head == NULL || head->next == head) {
        return;
    }
    //    struct list_head *list = head->next;
    //    head->prev->next = NULL;
    //    list = merge_sort(list);
    //   head->next = list;

    // struct list_head *i = head;
    // while (i->next != NULL) {
    //     i->next->prev = i;
    //     i = i->next;
    // }
    // head->prev = i;
    // i->next = head;
    //
    list_sort(NULL, head, sort_comp);
}
