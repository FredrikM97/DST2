#ifndef _DLIST_H_
#define _DLIST_H_
#include "kernel.h"

list * create_list();
listobj * create_listobj(int num);
void insert(list * mylist, listobj * pObj);
listobj * extract(listobj * pObj);

#endif
