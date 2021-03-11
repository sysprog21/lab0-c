#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness.h"
#include "queue.h"

/*
 * Create empty queue.
 * Return NULL if could not allocate space.
 */
queue_t *q_new()
{
    queue_t *q = malloc(sizeof(queue_t));
    if (!q)
        return NULL;
    q->head = q->tail = NULL;
    q->size = 0;
    return q;
}

/* Free all storage used by queue */
void q_free(queue_t *q)
{
    // if q is NULL, return false
    if (!q)
        return;
    while (q->head != NULL) {
        list_ele_t *temp = q->head;
        q->head = temp->next;
        free(temp->value);
        free(temp);
    }
    free(q);
}

/*
 * Attempt to insert element at head of queue.
 * Return true if successful.
 * Return false if q is NULL or could not allocate space.
 * Argument s points to the string to be stored.
 * The function must explicitly allocate space and copy the string into it.
 */
bool q_insert_head(queue_t *q, char *s)
{
    // if q is NULL, return false
    if (!q)
        return false;

    // create a element
    // return false if malloc fail
    list_ele_t *newh;
    newh = malloc(sizeof(list_ele_t));
    if (!newh)
        return false;

    // create a string
    char *temp = malloc(sizeof(char) * (strlen(s) + 1));
    if (!temp) {
        free(newh);
        return false;
    }

    // copy string
    newh->value = temp;
    strncpy(newh->value, s, (strlen(s) + 1));

    // insert element to head
    newh->next = q->head;
    q->head = newh;
    // if there is no element in queue
    if (q->size == 0) {
        q->tail = newh;
    }
    q->size++;
    return true;
}

/*
 * Attempt to insert element at tail of queue.
 * Return true if successful.
 * Return false if q is NULL or could not allocate space.
 * Argument s points to the string to be stored.
 * The function must explicitly allocate space and copy the string into it.
 */
bool q_insert_tail(queue_t *q, char *s)
{
    // if q is NULL, return false
    if (!q)
        return false;

    // create a element
    // return false if malloc fail
    list_ele_t *newh;
    newh = malloc(sizeof(list_ele_t));
    if (!newh)
        return false;

    // create a string
    char *temp = malloc(sizeof(char) * (strlen(s) + 1));
    if (!temp) {
        free(newh);
        return false;
    }

    // copy string
    newh->value = temp;
    strncpy(newh->value, s, (strlen(s) + 1));

    // insert element to head
    newh->next = NULL;
    if (q->size == 0) {
        q->head = newh;
        q->tail = newh;
    } else {
        q->tail->next = newh;
        q->tail = newh;
    }
    q->size++;
    return true;
}

/*
 * Attempt to remove element from head of queue.
 * Return true if successful.
 * Return false if queue is NULL or empty.
 * If sp is non-NULL and an element is removed, copy the removed string to *sp
 * (up to a maximum of bufsize-1 characters, plus a null terminator.)
 * The space used by the list element and the string should be freed.
 */
bool q_remove_head(queue_t *q, char *sp, size_t bufsize)
{
    // if q is NULL or empty, return false
    if (!q || q->size == 0)
        return false;

    // remove an element from queue
    list_ele_t *target = q->head;
    q->head = q->head->next;

    // if sp is non-NULL, copy the string to *sp
    if (sp != NULL)
        strncpy(sp, target->value, bufsize);

    // free the element
    free(target->value);
    free(target);
    q->size--;
    return true;
}

/*
 * Return number of elements in queue.
 * Return 0 if q is NULL or empty
 */
int q_size(queue_t *q)
{
    // if q is NULL
    if (!q)
        return 0;
    return q->size;
}

/*
 * Reverse elements in queue
 * No effect if q is NULL or empty
 * This function should not allocate or free any list elements
 * (e.g., by calling q_insert_head, q_insert_tail, or q_remove_head).
 * It should rearrange the existing ones.
 */
void q_reverse(queue_t *q)
{
    // if q is NULL or empty
    if (!q || q->size == 0)
        return;

    // assign tail to head
    q->tail = q->head;

    // change every element's next from head
    list_ele_t *rev_target = NULL, *current = q->head, *next;
    while (current) {
        next = current->next;
        current->next = rev_target;
        rev_target = current;
        current = next;
    }
    // assign head
    q->head = rev_target;
}

/*
 * Sort elements of queue in ascending order
 * No effect if q is NULL or empty. In addition, if q has only one
 * element, do nothing.
 */
void q_sort(queue_t *q)
{
    /* TODO: You need to write the code for this function */
    /* TODO: Remove the above comment when you are about to implement. */
}
