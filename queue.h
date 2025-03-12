#ifndef LAB0_QUEUE_H
#define LAB0_QUEUE_H

/* This program implements a queue supporting both FIFO and LIFO
 * operations.
 *
 * It uses a circular doubly-linked list to represent the set of queue elements
 */

#include <stdbool.h>
#include <stddef.h>

#include "harness.h"
#include "list.h"

/**
 * element_t - Linked list element
 * @value: pointer to array holding string
 * @list: node of a doubly-linked list
 *
 * @value needs to be explicitly allocated and freed
 */
typedef struct {
    char *value;
    struct list_head list;
} element_t;

/**
 * queue_contex_t - The context managing a chain of queues
 * @q: pointer to the head of the queue
 * @chain: used by chaining the heads of queues
 * @size: the length of this queue
 * @id: the unique identification number
 */
typedef struct {
    struct list_head *q;
    struct list_head chain;
    int size;
    int id;
} queue_contex_t;

/* Operations on queue */

/**
 * q_new() - Create an empty queue whose next and prev pointer point to itself
 *
 * Return: NULL for allocation failed
 */
struct list_head *q_new();

/**
 * q_free() - Free all storage used by queue, no effect if header is NULL
 * @head: header of queue
 */
void q_free(struct list_head *head);

/**
 * q_insert_head() - Insert an element in the head
 * @head: header of queue
 * @s: string would be inserted
 *
 * Argument s points to the string to be stored.
 * The function must explicitly allocate space and copy the string into it.
 *
 * Return: true for success, false for allocation failed or queue is NULL
 */
bool q_insert_head(struct list_head *head, char *s);

/**
 * q_insert_tail() - Insert an element at the tail
 * @head: header of queue
 * @s: string would be inserted
 *
 * Argument s points to the string to be stored.
 * The function must explicitly allocate space and copy the string into it.
 *
 * Return: true for success, false for allocation failed or queue is NULL
 */
bool q_insert_tail(struct list_head *head, char *s);

/**
 * q_remove_head() - Remove the element from head of queue
 * @head: header of queue
 * @sp: output buffer where the removed string is copied
 * @bufsize: size of the string
 *
 * If sp is non-NULL and an element is removed, copy the removed string to *sp
 * (up to a maximum of bufsize-1 characters, plus a null terminator.)
 *
 * NOTE: "remove" is different from "delete"
 * The space used by the list element and the string should not be freed.
 * The only thing "remove" need to do is unlink it.
 *
 * Reference:
 * https://english.stackexchange.com/questions/52508/difference-between-delete-and-remove
 *
 * Return: the pointer to element, %NULL if queue is NULL or empty.
 */
element_t *q_remove_head(struct list_head *head, char *sp, size_t bufsize);

/**
 * q_remove_tail() - Remove the element from tail of queue
 * @head: header of queue
 * @sp: output buffer where the removed string is copied
 * @bufsize: size of the string
 *
 * Return: the pointer to element, %NULL if queue is NULL or empty.
 */
element_t *q_remove_tail(struct list_head *head, char *sp, size_t bufsize);

/**
 * q_release_element() - Release the element
 * @e: element would be released
 *
 * This function is intended for internal use only.
 */
static inline void q_release_element(element_t *e)
{
    test_free(e->value);
    test_free(e);
}

/**
 * q_size() - Get the size of the queue
 * @head: header of queue
 *
 * Return: the number of elements in queue, zero if queue is NULL or empty
 */
int q_size(struct list_head *head);

/**
 * q_delete_mid() - Delete the middle node in queue
 * @head: header of queue
 *
 * The middle node of a linked list of size n is the
 * ⌊n / 2⌋th node from the start using 0-based indexing.
 * If there're six elements, the third member should be deleted.
 *
 * Reference:
 * https://leetcode.com/problems/delete-the-middle-node-of-a-linked-list/
 *
 * Return: true for success, false if list is NULL or empty.
 */
bool q_delete_mid(struct list_head *head);

/**
 * q_delete_dup() - Delete all nodes that have duplicate string,
 *                  leaving only distinct strings from the original queue.
 * @head: header of queue
 *
 * Reference:
 * https://leetcode.com/problems/remove-duplicates-from-sorted-list-ii/
 *
 * Return: true for success, false if list is NULL or empty.
 */
bool q_delete_dup(struct list_head *head);

/**
 * q_swap() - Swap every two adjacent nodes
 * @head: header of queue
 *
 * Reference:
 * https://leetcode.com/problems/swap-nodes-in-pairs/
 */
void q_swap(struct list_head *head);

/**
 * q_reverse() - Reverse elements in queue
 * @head: header of queue
 *
 * No effect if queue is NULL or empty.
 * This function should not allocate or free any list elements
 * (e.g., by calling q_insert_head, q_insert_tail, or q_remove_head).
 * It should rearrange the existing ones.
 */
void q_reverse(struct list_head *head);

/**
 * q_reverseK() - Given the head of a linked list, reverse the nodes of the list
 * k at a time.
 * @head: header of queue
 * @k: is a positive integer and is less than or equal to the length of the
 * linked list.
 *
 * No effect if queue is NULL or empty. If there has only one element, do
 * nothing.
 *
 * Reference:
 * https://leetcode.com/problems/reverse-nodes-in-k-group/
 */
void q_reverseK(struct list_head *head, int k);

/**
 * q_sort() - Sort elements of queue in ascending/descending order
 * @head: header of queue
 * @descend: whether or not to sort in descending order
 *
 * No effect if queue is NULL or empty. If there has only one element, do
 * nothing.
 */
void q_sort(struct list_head *head, bool descend);

/**
 * q_ascend() - Delete every node which has a node with a strictly less
 * value anywhere to the right side of it.
 * @head: header of queue
 *
 * No effect if queue is NULL or empty. If there has only one element, do
 * nothing.
 *
 * Reference:
 * https://leetcode.com/problems/remove-nodes-from-linked-list/
 *
 * Return: the number of elements in queue after performing operation
 */
int q_ascend(struct list_head *head);

/**
 * q_descend() - Delete every node which has a node with a strictly greater
 * value anywhere to the right side of it.
 * @head: header of queue
 *
 * No effect if queue is NULL or empty. If there has only one element, do
 * nothing.
 *
 * Reference:
 * https://leetcode.com/problems/remove-nodes-from-linked-list/
 *
 * Return: the number of elements in queue after performing operation
 */
int q_descend(struct list_head *head);

/**
 * q_merge() - Merge all the queues into one sorted queue, which is in
 * ascending/descending order.
 * @head: header of chain
 * @descend: whether to merge queues sorted in descending order
 *
 * This function merge the second to the last queues in the chain into the first
 * queue. The queues are guaranteed to be sorted before this function is called.
 * No effect if there is only one queue in the chain. Allocation is disallowed
 * in this function. There is no need to free the 'queue_contex_t' and its
 * member 'q' since they will be released externally. However, q_merge() is
 * responsible for making the queues to be NULL-queue, except the first one.
 *
 * Reference:
 * https://leetcode.com/problems/merge-k-sorted-lists/
 *
 * Return: the number of elements in queue after merging
 */
int q_merge(struct list_head *head, bool descend);

#endif /* LAB0_QUEUE_H */
