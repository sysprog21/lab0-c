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
    } while (current != end->next);
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
    q_reverse_segment(head->next, head->prev);
}

/* Reverse the nodes of the list k at a time */
void q_reverseK(struct list_head *head, int k)
{
    // https://leetcode.com/problems/reverse-nodes-in-k-group/
    struct list_head *start = NULL, *end = NULL, *current = NULL;
    int index = 0, group = q_size(head) / k, current_group = 0;
    if (list_empty(head)) {
        return;
    }
    do {
        current = current->next;
        if (current_group < group) {
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

    element_t *entry;
    list_for_each_entry (entry, head, list) {
        printf("%s\n", entry->value);
    }
}

/* 比較兩個字串，若左邊大於右邊，回傳 true，否則回傳 false */
bool value_compare(char *left, char *right)
{
    if (strcmp(left, right) > 0) {
        return true;
    } else {
        return false;
    }
}

/* Remove every node which has a node with a strictly less value anywhere to
 * the right side of it */
int q_ascend(struct list_head *head)
{
    if (!head || list_empty(head) || list_is_singular(head)) {
        return 0;
    }

    struct list_head *current = head, *prev_node;
    char *min = list_entry(head->prev, element_t, list)->value;

    do {
        current = current->prev;
        if (!current || current == head) {
            break;
        }

        prev_node = current->prev;  // 儲存下一個節點
        element_t *entry = list_entry(current, element_t, list);
        if (!entry) {
            break;
        }

        // 如果 `entry->value` 大於 `min`，刪除
        if (value_compare(entry->value, min)) {
            if (current->next) {
                current->next->prev = current->prev;
            }
            if (current->prev) {
                current->prev->next = current->next;
            }

            if (entry->value) {
                free(entry->value);
            }
            free(entry);
        } else {
            min = entry->value;  // 更新最小值
        }
        current = prev_node;
    } while (current != head);

    return 0;
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
    struct list_head *current = head, *prev_node;
    char *max = list_entry(head->prev, element_t, list)->value;

    do {
        current = current->prev;
        if (!current || current == head) {
            break;  // 防止 current == NULL
        }

        prev_node = current->prev;  // 儲存下一個節點

        // 確保 current 有效
        element_t *entry = list_entry(current, element_t, list);
        if (!entry) {
            break;
        }

        if (value_compare(max, entry->value)) {
            if (current->next) {
                current->next->prev = current->prev;
            }
            if (current->prev) {
                current->prev->next = current->next;
            }

            if (entry->value) {
                free(entry->value);
            }
            free(entry);  // 釋放節點記憶體
        } else {
            max = entry->value;
        }
        current = prev_node;
    } while (current != head);

    return 0;
}


/* Merge all the queues into one sorted queue, which is in ascending/descending
 * order */
int q_merge(struct list_head *head, bool descend)
{
    // https://leetcode.com/problems/merge-k-sorted-lists/

    return 0;
}