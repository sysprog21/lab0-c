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
    /* TODO: What if malloc returned NULL? */
    /* Return NULL if could not allocate space. */
    if (!q)
        return NULL;
    q->head = NULL;
    q->tail = NULL;
    q->size = 0;
    return q;
}

/* Free all storage used by queue */
void q_free(queue_t *q)
{
    if (!q)
        return;
    list_ele_t *node = q->head;
    while (node != NULL) {
        list_ele_t *tmp = node;
        node = node->next;
        free(tmp->value);
        free(tmp);
        q->size--;
    }
    /* TODO: How about freeing the list elements and the strings? */
    /* Free queue structure */
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
    if (!q) {
        return false;
    }
    list_ele_t *newh;
    /* TODO: What should you do if the q is NULL? */
    newh = malloc(sizeof(list_ele_t));
    if (!newh) {
        return false;
    }
    /* Don't forget to allocate space for the string and copy it */
    /* What if either call to malloc returns NULL? */

    newh->value = malloc(strlen(s) + 1);
    if (!newh->value) {
        free(newh);
        return false;
    }
    q->size++;
    strncpy(newh->value, s, strlen(s) + 1);
    if (!q->head) {
        /* Queue haven't contain any element */
        newh->next = NULL;
        q->head = q->tail = newh;
    } else {
        /* Queue already have element */
        newh->next = q->head;
        q->head = newh;
    }
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
    /* TODO: You need to write the complete code for this function */
    /* Remember: It should operate in O(1) time */
    /* TODO: Remove the above comment when you are about to implement. */
    if (!q) {
        return false;
    }
    list_ele_t *newh;
    newh = malloc(sizeof(list_ele_t));
    if (!newh) {
        return false;
    }

    newh->value = malloc(strlen(s) + 1);
    if (!newh->value) {
        free(newh);
        return false;
    }
    q->size++;
    strncpy(newh->value, s, strlen(s) + 1);
    q->head = q->tail ? q->head : newh;
    newh->next = NULL;
    if (q->tail)
        q->tail->next = newh;
    q->tail = newh;
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
    if (!q || !q->head)
        return false;
    list_ele_t *node = q->head;
    q->head = q->head->next;

    if (bufsize != 0) {
        if (strlen(node->value) > bufsize) {
            strncpy(sp, node->value, bufsize);
            sp[bufsize - 1] = '\0';
        } else {
            strncpy(sp, node->value, strlen(node->value) + 1);
        }
    }
    free(node->value);
    free(node);
    q->size--;
    return true;
}

/*
 * Return number of elements in queue.
 * Return 0 if q is NULL or empty
 */
int q_size(queue_t *q)
{
    /* TODO: You need to write the code for this function */
    /* Remember: It should operate in O(1) time */
    /* TODO: Remove the above comment when you are about to implement. */
    if (!q || !q->head)
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
    /* TODO: You need to write the code for this function */
    /* TODO: Remove the above comment when you are about to implement. */
    if (!q || !q->head || q->size == 1)
        return;

    /* A pointer to already reversed linked lis, whicih always
       point at head of reversed list */
    list_ele_t *reverse_list = q->head;
    /* A pointer to the remaining list */
    list_ele_t *list_to_do = q->head->next;

    q->tail = reverse_list;
    reverse_list->next = NULL;

    while (list_to_do) {
        list_ele_t *tmp = list_to_do;
        list_to_do = list_to_do->next;
        tmp->next = reverse_list;
        reverse_list = tmp;
    }
    q->head = reverse_list;
}

/*
 * Sort elements of queue in ascending order
 * No effect if q is NULL or empty. In addition, if q has only one
 * element, do nothing.
 */
void q_sort(queue_t *q)
/*
{
    if(!q || !q->head || q->size == 1)
        return;
    //list_ele_t *temp = q->head;
    // tail indicates the tail of sorted list
    list_ele_t *l_tail = q->tail;

    int size = q->size;
    list_ele_t *curr = q->head;
    list_ele_t *prev = q->head;
    list_ele_t *min_Prev = q->head;
    list_ele_t *minNode = q->head;
    for(int i = size; i > 0; i--) {
        curr = q->head;
        prev = q->head;
        min_Prev = q->head;
        minNode = q->head;
        // find the min
        for(int j = 0; j < i; j++) {
            if(strcmp(curr->value,minNode->value) < 0) {
                minNode = curr;
                min_Prev = prev;
            }
            curr = curr->next;
            if(j != 0) prev = prev->next;
        }
        // printf("%s\n",minNode->value);
        // let l_tail->next = min, because l_tail is last min.
        if(minNode == q->head) {
            l_tail->next = q->head;
            q->head = q->head->next;
            l_tail = l_tail->next;
            l_tail->next = NULL;
        }else{
            l_tail->next = minNode;
            min_Prev->next = minNode->next;
            l_tail = l_tail->next;
            minNode->next = NULL;
        }
        q->tail = l_tail;
    }


}*/
{
    if (!q || !q->head || q->size == 1)
        return;
    list_ele_t *temp;

    // get size first
    int size = q->size;
    // curr is the next element of sorted list
    list_ele_t *curr = q->head->next;
    // prev->next == curr
    list_ele_t *prev;
    // tail indicates the tail of sorted list
    list_ele_t *l_tail = q->head;

    for (int i = 1; i < size; i++) {
        temp = q->head;
        prev = q->head;
        // find the location to be inserted
        for (int j = 0; j < i && strcmp(temp->value, curr->value) < 0; j++) {
            temp = temp->next;
            if (j != 0)
                prev = prev->next;
        }
        // insert
        if (temp == q->head) {
            l_tail->next = curr->next;
            curr->next = q->head;
            q->head = curr;
        } else if (temp == curr) {
            l_tail = l_tail->next;
        } else {
            prev->next = curr;
            l_tail->next = curr->next;
            curr->next = temp;
        }
        curr = l_tail->next;
    }
}
