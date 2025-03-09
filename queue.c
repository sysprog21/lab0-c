#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "queue.h"

/* Create an empty queue */
struct list_head *q_new()
{
    struct list_head *head = malloc(sizeof(struct list_head));
    if (!head) {
        return NULL;
    }
    INIT_LIST_HEAD(head);
    return head;
}

/* Free all storage used by queue */
void q_free(struct list_head *head)
{
    element_t *entry, *safe = NULL;
    list_for_each_entry_safe (entry, safe, head, list) {
        if (entry->value) {
            free(entry->value);
        }
        free(entry);
    }
    free(head);
}

/* Insert an element at head of queue */
bool q_insert_head(struct list_head *head, char *s)
{
    if (!head) {
        return NULL;
    }
    element_t *new = malloc(sizeof(element_t));
    if (!new) {
        return false;
    }
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
    if (!head) {
        return false;
    }
    element_t *new = malloc(sizeof(element_t));
    if (!new) {
        return false;
    }
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
    if (list_empty(head)) {
        return NULL;
    }
    element_t *entry = list_first_entry(head, element_t, list);
    if (sp && entry->value) {
        strncpy(sp, entry->value, bufsize - 1);
        sp[bufsize - 1] = '\0';
    }
    list_del(&entry->list);
    return entry;
}

/* Remove an element from tail of queue */
element_t *q_remove_tail(struct list_head *head, char *sp, size_t bufsize)
{
    if (list_empty(head)) {
        return NULL;
    }
    element_t *entry = list_last_entry(head, element_t, list);
    if (sp && entry->value) {
        strncpy(sp, entry->value, bufsize - 1);
        sp[bufsize - 1] = '\0';
    }
    list_del(&entry->list);
    return entry;
}

/* Return number of elements in queue */
int q_size(struct list_head *head)
{
    element_t *entry;
    int count = 0;
    list_for_each_entry (entry, head, list) {
        count++;
    }
    return count;
}

// 偶數：取後一個
struct list_head *q_get_middle(struct list_head *head)
{
    if (!head || list_empty(head)) {
        return NULL;
    }
    struct list_head *slow = head->next;
    struct list_head *fast = head->next->next;
    while (fast != head && fast->next != head) {
        slow = slow->next;
        fast = fast->next->next;
    }
    return slow;
}

/* Delete the middle node in queue */
bool q_delete_mid(struct list_head *head)
{
    // https://leetcode.com/problems/delete-the-middle-node-of-a-linked-list/
    if (!head || list_empty(head)) {
        return false;
    }
    struct list_head *middle = q_get_middle(head);
    element_t *entry = list_entry(middle, element_t, list);
    list_del(middle);
    if (entry->value) {
        free(entry->value);
    }
    free(entry);
    return true;
}

/* Delete all nodes that have duplicate string */
bool q_delete_dup(struct list_head *head)
{
    // https://leetcode.com/problems/remove-duplicates-from-sorted-list-ii/
    if (list_empty(head)) {
        return true;
    }
    element_t *entry = NULL, *safe = NULL;
    bool dup = false;
    q_sort(head, false);
    list_for_each_entry_safe (entry, safe, head, list) {
        if (entry->list.next != head &&
            strcmp(entry->value, safe->value) == 0) {
            list_del(entry->list.next->prev);
            q_release_element(entry);
            dup = true;
        } else if (dup) {
            list_del(entry->list.next->prev);
            q_release_element(entry);
            dup = false;
        }
    }
    return true;
}

/* Swap every two adjacent nodes */
void q_swap(struct list_head *head)
{
    // https://leetcode.com/problems/swap-nodes-in-pairs/
    q_reverseK(head, 2);
}

/* Reverse elements  between start and end in queue */
void q_reverse_segment(struct list_head *start, struct list_head *end)
{
    if (!start || !end || start == end)
        return;
    struct list_head *tmp, *current = start, *start_prev = start->prev,
                           *end_next = end->next;
    do {
        tmp = current->next;
        current->next = current->prev;
        current->prev = tmp;
        current = tmp;
        printf("ddd\n");
    } while (current != end_next);
    // 修正反轉後的新連接
    if (start_prev)
        start_prev->next = end;  // 讓原本的前一個指向新的首部
    if (end_next)
        end_next->prev = start;  // 讓原本的下一個指向新的尾部

    start->next = end_next;  // start 現在是新的尾部，接回原本的下一個
    end->prev = start_prev;  // end 現在是新的首部，接回原本的前一個
}

/* Reverse elements in queue */
void q_reverse(struct list_head *head)
{
    if (!head || list_empty(head))
        return;
    q_reverse_segment(head->next, head->prev);
}

/* Reverse the nodes of the list k at a time */
void q_reverseK(struct list_head *head, int k)
{
    // https://leetcode.com/problems/reverse-nodes-in-k-group/

    if (!head || list_empty(head))
        return;

    struct list_head *start = head, *end = NULL, *current = start;
    int index = 0, group = q_size(head) / k, current_group = 0;
    if (list_empty(head)) {
        return;
    }
    int i = 0;
    do {
        current = current->next;
        printf("4\n");
        if (current_group < group) {
            printf("i: %d\n", i);
            index++;
            if (index == 1) {
                start = current;
            } else if (index == k) {
                end = current;
                q_reverse_segment(start, end);
            } else {
                index = 0;
                current_group++;
            }
        }
    } while (current != head);
}


/**
 * 若 `descend` 為 `true`，則當 `left->value` >= `right->value` 回傳 `true`。
 * 若 `descend` 為 `false`，則當 `left->value` <= `right->value` 回傳 `true`。
 *
 * @return `true` 表示 `left` 應該排在 `right` 前面，`false` 則相反。
 */
static inline bool compare(struct list_head *left,
                           struct list_head *right,
                           bool descend)
{
    int result = strcmp(list_entry(left, element_t, list)->value,
                        list_entry(right, element_t, list)->value);
    return descend ? result >= 0 : result <= 0;
}


struct list_head *merge(struct list_head *left,
                        struct list_head *right,
                        bool descend)
{
    left->prev->next = NULL;
    right->prev->next = NULL;

    struct list_head *head = NULL, **ptr = &head, *prev = NULL;


    while (left && right) {
        if (compare(left, right, descend)) {
            *ptr = left;
            left = left->next;
        } else {
            *ptr = right;
            right = right->next;
        }
        (*ptr)->prev = prev;
        prev = *ptr;
        ptr = &(*ptr)->next;
    }


    if (left) {
        *ptr = left;
        (*ptr)->prev = prev;
    } else {
        *ptr = right;
        (*ptr)->prev = prev;
    }

    struct list_head *current = *ptr;
    while (current != NULL) {
        current->prev = prev;
        prev = current;
        current = current->next;
    }

    if (head) {
        struct list_head *tail = prev;
        tail->next = head;
        head->prev = tail;
    }

    return head;
}

struct list_head *q_merge_two_lists(struct list_head *head, bool descend)
{
    if (list_empty(head) || head->prev == head) {
        list_del_init(head);
        return head;
    }
    struct list_head *middle = q_get_middle(head);
    if (!middle || middle == head) {
        return head;  // 避免無窮遞迴
    }

    struct list_head *tmp = middle->prev;

    // initialize the right list
    middle->prev = head->prev;
    head->prev->next = middle;


    // initialize the left list with the head
    head->prev = tmp;
    tmp->next = head;



    struct list_head *left = q_merge_two_lists(head, descend);
    struct list_head *right = q_merge_two_lists(middle, descend);
    return merge(left, right, descend);
}

/* Sort elements of queue in ascending/descending order with merge sort*/
// 現在有 q_get_middle 可以拿到中間的節點，再用 list_cut_position 切開
void q_sort(struct list_head *head, bool descend)
{
    if (list_empty(head) || list_is_singular(head)) {
        return;
    }
    // temporary remove head
    struct list_head *first = head->next;
    struct list_head *last = head->prev;
    first->prev = last;
    last->next = first;

    struct list_head *sort_first = q_merge_two_lists(first, descend);


    head->next = sort_first;
    head->prev = sort_first->prev;
    sort_first->prev->next = head;
    sort_first->prev = head;
}

/* Remove every node which has a node with a strictly less value anywhere to
 * the right side of it */
int q_ascend(struct list_head *head)
{
    if (!head || list_empty(head) || list_is_singular(head)) {
        return 0;
    }
    // reverse for_each
    struct list_head *current = head->next->next, *prev_node = NULL;
    struct list_head *max = head->next;  // 從第一個節點開始往後找

    while (current != head) {
        prev_node = current->next;  // 儲存下一個節點
        // 確保 current 有效
        element_t *entry = list_entry(current, element_t, list);
        if (!entry) {
            break;
        }
        if (compare(max, current,
                    true)) {  // 如果 max 大於 entry->value，則刪除 entry
            list_del(current);
            q_release_element(entry);
        } else {
            max = current;
        }
        current = prev_node;
    }

    return q_size(head);
}

/* Remove every node which has a node with a strictly greater value anywhere to
 * the right side of it */
int q_descend(struct list_head *head)
{
    // https://leetcode.com/problems/remove-nodes-from-linked-list/
    if (!head || list_empty(head) || list_is_singular(head)) {
        return 0;
    }
    // reverse for_each
    struct list_head *current = head->prev->prev, *prev_node = NULL;
    struct list_head *max = head->prev;  // 從最後一個節點開始往回找

    while (current != head) {
        prev_node = current->prev;  // 儲存上一個節點
        // 確保 current 有效
        element_t *entry = list_entry(current, element_t, list);
        if (!entry) {
            break;
        }
        if (compare(max, current,
                    true)) {  // 如果 max 大於 entry->value，則刪除 entry
            list_del(current);
            q_release_element(entry);
        } else {
            max = current;
        }
        current = prev_node;
    }

    return q_size(head);
}



void merge_lists_with_sentinel_node(struct list_head *l1,
                                    struct list_head *l2,
                                    bool descend)
{
    struct list_head *curr = l1->next;
    struct list_head *next = l2->next;
    struct list_head *head = l1, **ptr = &(head->next), *prev = head;

    while (curr != l1 && next != l2) {
        if (compare(curr, next, descend)) {
            *ptr = curr;
            curr = curr->next;
        } else {
            *ptr = next;
            next = next->next;
        }
        (*ptr)->prev = prev;
        prev = *ptr;
        ptr = &(*ptr)->next;
    }

    if (curr != l1) {  // l1還在輸出！！
        *ptr = curr;
        curr->prev = prev;
    } else {
        *ptr = next;
        next->prev = prev;
        l2->prev->next = l1;
        l1->prev = l2->prev;
    }
    INIT_LIST_HEAD(l2);
}


#define element_next(pos, member) \
    list_entry((pos)->member.next, typeof(*(pos)), member)

int q_merge(struct list_head *head, bool descend)
{
    if (!head || list_empty(head))
        return 0;

    queue_contex_t *entry = list_first_entry(head, queue_contex_t, chain),
                   *next;
    struct list_head *curr = entry->q;

    for (next = element_next(entry, chain); &next->chain != head;
         next = element_next(next, chain)) {
        merge_lists_with_sentinel_node(curr, next->q, descend);
    }

    return q_size(entry->q);
}