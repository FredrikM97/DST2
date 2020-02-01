/* test.c */
#include "kernel.h"
#include "dlist.h"
#include "utest.h"
#include <limits.h>

list  * readylist;

int main(void)
{
	listobj * obj = NULL;
	readylist = NULL;
	assert(isEqualPointer(readylist, NULL));
	assert(isEqualPointer(obj, NULL));

	// new list
	readylist = create_list();
	assert(isNotEqualPointer(readylist, NULL));
	assert(isEmptyList(readylist));
	readylist->pHead->nTCnt = 0;
	readylist->pTail->nTCnt = UINT_MAX;

	// new node #0
	obj = create_listobj(10);
	assert(isEqualPointer(obj, obj));

	// insert
	insert(readylist, obj);
	assert(isEmptyList(readylist) == 0);
	assert(isEqualPointer(obj, readylist->pHead->pNext));
	assert(isEqualPointer(obj, readylist->pTail->pPrevious));
	assert(isEqualPointer(readylist->pHead->pNext, readylist->pTail->pPrevious));
	obj = NULL;
	assert(isEqualPointer(obj, NULL));

	// new node #1
	obj = create_listobj(11);
	assert(isEqualInt(obj->nTCnt, 11));
	assert(isNotEqualPointer(obj, NULL));
	insert(readylist, obj);

	assert(isEqualPointer(obj, readylist->pTail->pPrevious->pPrevious));
	assert(isEqualPointer(obj, readylist->pHead->pNext));
	assert(isEqualPointer(readylist->pTail->pPrevious, readylist->pHead->pNext->pNext));

	// extract node #1
	obj = extract(obj);
	assert(isNotEqualPointer(obj, NULL));
	assert(isEqualInt(obj->nTCnt, 11));
	free(obj);

	assert(isEmptyList(readylist) == 0);
	assert(isEqualPointer(readylist->pHead->pNext, readylist->pTail->pPrevious));

	// new node #1
	obj = create_listobj(9);
	assert(isNotEqualPointer(obj, NULL));
	assert(isEqualInt(obj->nTCnt, 9));
	insert(readylist, obj);
	assert(isEqualPointer(obj, readylist->pHead->pNext));

	assert(isEqualInt(1, 1) && "Comparing 1 and 1, yeilds no assert");
	assert(isEqualInt(1, 2) && "This is an expected outcome of comparing 1 and 2");

	return 0;
}

