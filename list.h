#ifndef _LIST_H
#define _LIST_H 1


#include <stdio.h>
#include <stdlib.h>

/*
 * Generic list cell structure.
 */
typedef struct cell {
    void *info;
    struct cell *next;
} *list;

void insert_in_list(list *, void *);
void remove_from_list(list *, void *, int (void *, void *)); 
void free_list(list *, void (void *));

#endif