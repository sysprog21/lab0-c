void q_sort(struct list_head *head)
{
    /* insertion sort */
    /* Remove every node which has a node with a strictly greater value anywhere
     * to the right side of it */
    /*https://npes87184.github.io/2015-09-12-linkedListSort/ */

    if (!head)
        return;
    struct list_head *temp = head;

    int size = 0;
    while (temp) {
        size++;
        temp = temp->next;
    }

    struct list_head *curr = head->next;
    struct list_head *prev = head;
    struct list_head *tail = head;

    for (int i = 1; i < size; i++) {
        temp = head;
        prev = head;
        // find inserting location
        element_t *t_nod = list_entry(temp, element_t, list),
                  *c_nod = list_entry(curr, element_t, list);
        for (int j = 0; j < i && *t_nod->value < *c_nod->value; j++) {
            temp = temp->next;
            if (j != 0)
                prev = prev->next;
        }
        // insert
        if (temp == head) {
            tail->next = curr->next;
            curr->next = head;
            head = curr;
        } else if (temp == curr) {
            tail = tail->next;
        } else {
            prev->next = curr;
            tail->next = curr->next;
            curr->next = temp;
        }
        curr = tail->next;
    }
    return;
}
