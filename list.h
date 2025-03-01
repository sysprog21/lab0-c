/* Linux-like circular doubly-linked list implementation */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/**
 * Feature detection for 'typeof':
 * - Supported as a GNU extension in GCC/Clang.
 * - Part of C23 standard (ISO/IEC 9899:2024).
 *
 * Reference: https://gcc.gnu.org/onlinedocs/gcc/Typeof.html
 */
#if defined(__GNUC__) || defined(__clang__) ||         \
    (defined(__STDC__) && defined(__STDC_VERSION__) && \
     (__STDC_VERSION__ >= 202311L)) /* C23 ?*/
#define __LIST_HAVE_TYPEOF 1
#else
#define __LIST_HAVE_TYPEOF 0
#endif

/**
 * struct list_head - Node structure for a circular doubly-linked list
 * @next: Pointer to the next node in the list.
 * @prev: Pointer to the previous node in the list.
 *
 * Defines both the head and nodes of a circular doubly-linked list. The head's
 * @next points to the first node and @prev to the last node; in an empty list,
 * both point to the head itself. All nodes, including the head, share this
 * structure type.
 *
 * Nodes are typically embedded within a container structure holding actual
 * data, accessible via the list_entry() helper, which computes the container's
 * address from a node pointer. The list_* functions and macros provide an API
 * for manipulating this data structure efficiently.
 */
struct list_head {
    struct list_head *prev;
    struct list_head *next;
};

/**
 * container_of() - Calculate address of structure that contains address ptr
 * @ptr: pointer to member variable
 * @type: type of the structure containing ptr
 * @member: name of the member variable in struct @type
 *
 * Return: @type pointer of structure containing ptr
 */
#ifndef container_of
#if __LIST_HAVE_TYPEOF
#define container_of(ptr, type, member)                         \
    __extension__({                                             \
        const typeof(((type *) 0)->member) *__pmember = (ptr);  \
        (type *) ((char *) __pmember - offsetof(type, member)); \
    })
#else
#define container_of(ptr, type, member) \
    ((type *) ((char *) (ptr) - offsetof(type, member)))
#endif
#endif

/**
 * LIST_HEAD - Define and initialize a circular list head
 * @head: name of the new list
 */
#define LIST_HEAD(head) struct list_head head = {&(head), &(head)}

/**
 * INIT_LIST_HEAD() - Initialize empty list head
 * @head: Pointer to the list_head structure to initialize.
 *
 * It sets both @next and @prev to point to the structure itself. The
 * initialization applies to either a list head or an unlinked node that is
 * not yet part of a list.
 *
 * Unlinked nodes may be passed to functions using 'list_del()' or
 * 'list_del_init()', which are safe only on initialized nodes. Applying these
 * operations to an uninitialized node results in undefined behavior, such as
 * memory corruption or crashes.
 */
static inline void INIT_LIST_HEAD(struct list_head *head)
{
    head->next = head;
    head->prev = head;
}

/**
 * list_add - Insert a node at the beginning of a circular list
 * @node: Pointer to the list_head structure to add.
 * @head: Pointer to the list_head structure representing the list head.
 *
 * Adds the specified @node immediately after @head in a circular doubly-linked
 * list, effectively placing it at the beginning. The existing first node, if
 * any, shifts to follow @node, and the list's circular structure is maintained.
 */
static inline void list_add(struct list_head *node, struct list_head *head)
{
    struct list_head *next = head->next;

    next->prev = node;
    node->next = next;
    node->prev = head;
    head->next = node;
}

/**
 * list_add_tail() - Add a list node to the end of the list
 * @node: pointer to the new node
 * @head: pointer to the head of the list
 */
static inline void list_add_tail(struct list_head *node, struct list_head *head)
{
    struct list_head *prev = head->prev;

    prev->next = node;
    node->next = head;
    node->prev = prev;
    head->prev = node;
}

/**
 * list_del - Remove a node from a circular doubly-linked list
 * @node: Pointer to the list_head structure to remove.
 *
 * Removes @node from its list by updating the adjacent nodes’ pointers to
 * bypass it. The node’s memory and its containing structure, if any, are not
 * freed. After removal, @node is left unlinked and should be treated as
 * uninitialized; accessing its @next or @prev pointers is unsafe and may cause
 * undefined behavior.
 *
 * Even previously initialized but unlinked nodes become uninitialized after
 * this operation. To reintegrate @node into a list, it must be reinitialized
 * (e.g., via INIT_LIST_HEAD).
 *
 * If LIST_POISONING is enabled at build time, @next and @prev are set to
 * invalid addresses to trigger memory access faults on misuse. This feature is
 * effective only on systems that restrict access to these specific addresses.
 */
static inline void list_del(struct list_head *node)
{
    struct list_head *next = node->next;
    struct list_head *prev = node->prev;

    next->prev = prev;
    prev->next = next;

#ifdef LIST_POISONING
    node->next = NULL;
    node->prev = NULL;
#endif
}

/**
 * list_del_init - Remove a node and reinitialize it as unlinked
 * @node: Pointer to the list_head structure to remove and reinitialize.
 *
 * Removes @node from its circular doubly-linked list using list_del() and then
 * reinitializes it as an unlinked node via INIT_LIST_HEAD(). Unlike list_del(),
 * which leaves the node uninitialized, this ensures @node is safely reset to an
 * empty, standalone state with @next and @prev pointing to itself.
 */
static inline void list_del_init(struct list_head *node)
{
    list_del(node);
    INIT_LIST_HEAD(node);
}

/**
 * list_empty - Test if a circular list has no nodes
 * @head: Pointer to the list_head structure representing the list head.
 *
 * Checks whether the circular doubly-linked list headed by @head is empty.
 * A list is empty if @head’s @next points to itself, indicating no nodes are
 * attached.
 *
 * Returns: 0 if the list has nodes, non-zero if the list is empty.
 */
static inline int list_empty(const struct list_head *head)
{
    return (head->next == head);
}

/**
 * list_is_singular() - Check if list head has exactly one node attached
 * @head: pointer to the head of the list
 *
 * Returns: 0 if the list is not singular, non-zero if the list has exactly one
 * entry.
 */
static inline int list_is_singular(const struct list_head *head)
{
    return (!list_empty(head) && head->prev == head->next);
}

/**
 * list_splice() - Add list nodes from a list to beginning of another list
 * @list: pointer to the head of the list with the node entries
 * @head: pointer to the head of the list
 *
 * All nodes from @list are added to the beginning of the list of @head.
 * It is similar to list_add but for multiple nodes. The @list head is not
 * modified and has to be initialized to be used as a valid list head/node
 * again.
 */
static inline void list_splice(struct list_head *list, struct list_head *head)
{
    struct list_head *head_first = head->next;
    struct list_head *list_first = list->next;
    struct list_head *list_last = list->prev;

    if (list_empty(list))
        return;

    head->next = list_first;
    list_first->prev = head;

    list_last->next = head_first;
    head_first->prev = list_last;
}

/**
 * list_splice_tail() - Add list nodes from a list to end of another list
 * @list: pointer to the head of the list with the node entries
 * @head: pointer to the head of the list
 *
 * All nodes from @list are added to to the end of the list of @head.
 * It is similar to list_add_tail but for multiple nodes. The @list head is not
 * modified and has to be initialized to be used as a valid list head/node
 * again.
 */
static inline void list_splice_tail(struct list_head *list,
                                    struct list_head *head)
{
    struct list_head *head_last = head->prev;
    struct list_head *list_first = list->next;
    struct list_head *list_last = list->prev;

    if (list_empty(list))
        return;

    head->prev = list_last;
    list_last->next = head;

    list_first->prev = head_last;
    head_last->next = list_first;
}

/**
 * list_splice_init() - Move list nodes from a list to beginning of another list
 * @list: pointer to the head of the list with the node entries
 * @head: pointer to the head of the list
 *
 * All nodes from @list are added to to the beginning of the list of @head.
 * It is similar to list_add but for multiple nodes.
 *
 * The @list head will not end up in an uninitialized state like when using
 * list_splice. Instead the @list is initialized again to the an empty
 * list/unlinked state.
 */
static inline void list_splice_init(struct list_head *list,
                                    struct list_head *head)
{
    list_splice(list, head);
    INIT_LIST_HEAD(list);
}

/**
 * list_splice_tail_init() - Move list nodes from a list to end of another list
 * @list: pointer to the head of the list with the node entries
 * @head: pointer to the head of the list
 *
 * All nodes from @list are added to to the end of the list of @head.
 * It is similar to list_add_tail but for multiple nodes.
 *
 * The @list head will not end up in an uninitialized state like when using
 * list_splice. Instead the @list is initialized again to the an empty
 * list/unlinked state.
 */
static inline void list_splice_tail_init(struct list_head *list,
                                         struct list_head *head)
{
    list_splice_tail(list, head);
    INIT_LIST_HEAD(list);
}

/**
 * list_cut_position() - Move beginning of a list to another list
 * @head_to: pointer to the head of the list which receives nodes
 * @head_from: pointer to the head of the list
 * @node: pointer to the node in which defines the cutting point
 *
 * All entries from the beginning of the list @head_from to (including) the
 * @node is moved to @head_to.
 *
 * @head_to is replaced when @head_from is not empty. @node must be a real
 * list node from @head_from or the behavior is undefined.
 */
static inline void list_cut_position(struct list_head *head_to,
                                     struct list_head *head_from,
                                     struct list_head *node)
{
    struct list_head *head_from_first = head_from->next;

    if (list_empty(head_from))
        return;

    if (head_from == node) {
        INIT_LIST_HEAD(head_to);
        return;
    }

    head_from->next = node->next;
    head_from->next->prev = head_from;

    head_to->prev = node;
    node->next = head_to;
    head_to->next = head_from_first;
    head_to->next->prev = head_to;
}

/**
 * list_move() - Move a list node to the beginning of the list
 * @node: pointer to the node
 * @head: pointer to the head of the list
 *
 * The @node is removed from its old position/node and add to the beginning of
 * @head
 */
static inline void list_move(struct list_head *node, struct list_head *head)
{
    list_del(node);
    list_add(node, head);
}

/**
 * list_move_tail() - Move a list node to the end of the list
 * @node: pointer to the node
 * @head: pointer to the head of the list
 *
 * The @node is removed from its old position/node and add to the end of @head
 */
static inline void list_move_tail(struct list_head *node,
                                  struct list_head *head)
{
    list_del(node);
    list_add_tail(node, head);
}

/**
 * list_entry() - Get the entry for this node
 * @node: pointer to list node
 * @type: type of the entry containing the list node
 * @member: name of the list_head member variable in struct @type
 *
 * Return: @type pointer of entry containing node
 */
#define list_entry(node, type, member) container_of(node, type, member)

/**
 * list_first_entry() - Get first entry of the list
 * @head: pointer to the head of the list
 * @type: type of the entry containing the list node
 * @member: name of the list_head member variable in struct @type
 *
 * Return: @type pointer of first entry in list
 */
#define list_first_entry(head, type, member) \
    list_entry((head)->next, type, member)

/**
 * list_last_entry() - Get last entry of the list
 * @head: pointer to the head of the list
 * @type: type of the entry containing the list node
 * @member: name of the list_head member variable in struct @type
 *
 * Return: @type pointer of last entry in list
 */
#define list_last_entry(head, type, member) \
    list_entry((head)->prev, type, member)

/**
 * list_for_each - Iterate over list nodes
 * @node: list_head pointer used as iterator
 * @head: pointer to the head of the list
 *
 * The nodes and the head of the list must be kept unmodified while
 * iterating through it. Any modifications to the the list will cause undefined
 * behavior.
 */
#define list_for_each(node, head) \
    for (node = (head)->next; node != (head); node = node->next)

/**
 * list_for_each_entry - Iterate over a list of entries
 * @entry: Pointer to the structure type, used as the loop iterator.
 * @head: Pointer to the list_head structure representing the list head.
 * @member: Name of the list_head member within the structure type of @entry.
 *
 * Iterates over a circular doubly-linked list, starting from the first node
 * after @head until reaching @head again. The macro assumes the list structure
 * remains unmodified during iteration; any changes (e.g., adding/removing
 * nodes) may result in undefined behavior.
 */
#if __LIST_HAVE_TYPEOF
#define list_for_each_entry(entry, head, member)                   \
    for (entry = list_entry((head)->next, typeof(*entry), member); \
         &entry->member != (head);                                 \
         entry = list_entry(entry->member.next, typeof(*entry), member))
#else
/* The negative width bit-field makes a compile-time error for use of this. It
 * works in the same way as BUILD_BUG_ON_ZERO macro of Linux kernel.
 */
#define list_for_each_entry(entry, head, member) \
    for (entry = (void *) 1; sizeof(struct { int i : -1; }); ++(entry))
#endif

/**
 * list_for_each_safe - Iterate over list nodes, allowing removal
 * @node: Pointer to a list_head structure, used as the loop iterator.
 * @safe: Pointer to a list_head structure, storing the next node for safe
 *        iteration.
 * @head: Pointer to the list_head structure representing the list head.
 *
 * Iterates over a circular doubly-linked list, starting from the first node
 * after @head and continuing until reaching @head again. This macro allows
 * safe removal of the current node (@node) during iteration by pre-fetching
 * the next node into @safe. Other modifications to the list structure (e.g.,
 * adding nodes or altering @head) may result in undefined behavior.
 */
#define list_for_each_safe(node, safe, head)                     \
    for (node = (head)->next, safe = node->next; node != (head); \
         node = safe, safe = node->next)

/**
 * list_for_each_entry_safe - Iterate over a list, allowing node removal
 * @entry: Pointer to the structure type, used as the loop iterator.
 * @safe: Pointer to the structure type, storing the next entry for safe
 * iteration.
 * @head: Pointer to the list_head structure representing the list head.
 * @member: Name of the list_head member within the structure type of @entry.
 *
 * Iterates over a circular doubly-linked list, starting from the first node
 * after @head and continuing until reaching @head again. This macro permits
 * safe removal of the current node (@entry) during iteration by pre-fetching
 * the next node into @safe. Other modifications to the list structure (e.g.,
 * adding nodes or altering @head) may result in undefined behavior.
 */
#if __LIST_HAVE_TYPEOF
#define list_for_each_entry_safe(entry, safe, head, member)            \
    for (entry = list_entry((head)->next, typeof(*entry), member),     \
        safe = list_entry(entry->member.next, typeof(*entry), member); \
         &entry->member != (head); entry = safe,                       \
        safe = list_entry(safe->member.next, typeof(*entry), member))
#else
#define list_for_each_entry_safe(entry, safe, head, member)  \
    for (entry = (void *) 1; sizeof(struct { int i : -1; }); \
         ++(entry), ++(safe))
#endif

#undef __LIST_HAVE_TYPEOF

#ifdef __cplusplus
}
#endif
