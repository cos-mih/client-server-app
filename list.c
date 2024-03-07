#include <stdio.h>
#include <stdlib.h>

#include "list.h"

/*
 * Function making the insertion of a new list cell with the "info" field given (*elem) in a given list (l).
 */
void insert_in_list(list *l, void *elem) {
    list new = (list)calloc(1, sizeof(struct cell));
    if (!new) {
        perror("bad alloc\n");
        return;
    }

    new->info = elem;
    new->next = NULL;

    list p;
    if (!(*l)) {  // check if list is empty, in which case insert the new cell as the beggining of the list
        *l = new;
        return;
    }
    for (p = *l; p->next != NULL; p = p->next);
    p->next = new;
}

/*
 * Function removing a list cell with a given "info" field, identified using the "equal" function, comparing
 * two "info" type data structures.
 */
void remove_from_list(list *l, void *elem, int equal(void *, void *)) {
    list p = *l;

    if (equal(p->info, elem)) {  // remove first cell
        *l = p->next;
        free(p->info);
        free(p);
        return;
    }

    list prev = p;
    for (p = p->next; p != NULL; p = p->next) {
        if (equal(p->info, elem)) {
            prev->next = p->next;
            free(p->info);
            free(p);
            return;
        }
        prev = p;
    }
}

/*
 * Function deallocating the memory allocated for a list, using a given function (free_elem) for
 * deallocating each "info" field accordingly.
 */
void free_list(list *l, void free_elem(void *)) {
    list p = *l;
    while(p) {
        list aux = p;
        p = p->next;
        free_elem(aux->info);
        free(aux);
    }

    *l = NULL;
}