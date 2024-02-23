/*
 * eos_list.c
 *
 *  Created on: May 9, 2023
 *      Author: hjx
 */

#include "eos_list.h"

#include <stdio.h>
#include <stdlib.h>

void EOS_List_Create(EOTList* tpList)
{
	tpList->_head = NULL;
	tpList->_tail = NULL;
	tpList->count = 0;
}

/**
 * 清空整个列表，并释放内存
 */
void EOS_List_Clear(EOTList* tpList, EOFuncListItemFree tItemFree)
{
	EOTListItem* item = tpList->_head;
	EOTListItem* itemFree;
	while (item != NULL)
	{
		itemFree = item;
		item = item->_next;

		if (tItemFree != NULL)
		{
			tItemFree(tpList, itemFree);
		}
		free(itemFree);
	}

	tpList->count = 0;
}

/**
 * 释放单个内存
 */
void EOS_List_ItemFree(EOTList* tpList, EOTListItem* tpListItm, EOFuncListItemFree tItemFree)
{
	if (tpListItm == NULL) return;
	if (tItemFree != NULL)
	{
		tItemFree(tpList, tpListItm);
	}
	free(tpListItm);
}

EOTListItem* EOS_List_Push(EOTList* tpList, int nId, void* pData, int nLength)
{
	int nSize = sizeof(EOTListItem);
	EOTListItem* item = malloc(nSize);
	if (item == NULL)
	{
		printf("*** malloc NULL");
		return NULL;
	}

	item->id = nId;
	item->data = pData;
	item->length = nLength;

	item->flag = 0;

	item->_prev = tpList->_tail;
	item->_next = NULL;

	if (tpList->_tail != NULL)
		tpList->_tail->_next = item;

	if (tpList->_head == NULL)
		tpList->_head = item;

	tpList->_tail = item;

	tpList->count++;

	return item;
}

/**
 * 从队列中取出信息项
 * 注意，信息项并未释放
 */
EOTListItem* EOS_List_Pop(EOTList* tpList)
{
	EOTListItem* item = tpList->_head;
	if (item == NULL)
	{
		// 空的
		return NULL;
	}

	tpList->_head = item->_next;

	if (tpList->_head != NULL)
		tpList->_head->_prev = NULL;
	else
		tpList->_tail = NULL;

	tpList->count--;

	return item;
}

EOTListItem* EOS_List_Top(EOTList* tpList)
{
	return tpList->_head;
}
