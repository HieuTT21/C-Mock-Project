#ifndef __LINKEDLISTS_H__
#define __LINKEDLISTS_H__

#include "stdint.h"
#include "stdio.h"

typedef struct Node
{
    uint32_t 		currentAddress;
    struct Node *	next;
} Node_t;

typedef struct
{
    Node_t * head;
} LinkedList_t;

LinkedList_t* createLinkedList();
void removeHead(LinkedList_t *list);
void addToHead(LinkedList_t *list, uint16_t address);
void freeLinkedList(LinkedList_t* list);

#endif
