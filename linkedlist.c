#include "stdlib.h"
#include "linkedlist.h"

LinkedList_t* createLinkedList() 
{
    LinkedList_t* newLinkedList = (LinkedList_t*) malloc(sizeof(LinkedList_t));
    newLinkedList->head = NULL;
    return newLinkedList;
}

/*
Function: add a new node at the end of the list
Parameter: Linkedlist, Device struct
Functionality: if the list is empty, create a node. Else, run through the list until it hits the end, then create a new node
*/

void removeHead(LinkedList_t *list)
{
    if (list->head == NULL)
    {
        printf("List is empty\n");
        return;
    }
    Node_t *temp = list->head;
    list->head = list->head->next;
    free(temp);
}

void addToHead(LinkedList_t *list, uint16_t address)
{
    Node_t * newHead = (Node_t *)malloc(sizeof(Node_t));

	newHead->currentAddress = address;
	newHead->next = list->head;
	list->head = newHead;
}

void freeLinkedList(LinkedList_t* list) 
{
    Node_t* currentHead = list->head;
    while (currentHead!= NULL) 
	{
		Node_t* nextNode = currentHead->next;
		free(currentHead);
		currentHead = nextNode;
	}
	free(list);
}
