/*
 * eos_list.h
 *
 *  Created on: May 9, 2023
 *      Author: hjx
 */

#ifndef EOS_LIST_H_
#define EOS_LIST_H_

#include <stdint.h>

//
// 单一双向链表
//
typedef struct _stEOTListItem
{
	// 编号
	int id;
	// 数据
	void* data;
	// 数据长度
	int length;
	// 标识
	int flag;

	struct _stEOTListItem* _next;
	struct _stEOTListItem* _prev;
}
EOTListItem;

typedef struct _stEOTList
{
	struct _stEOTListItem* _head;
	struct _stEOTListItem* _tail;
	int count;
}
EOTList;

// 用于释放回调
typedef void (*EOFuncListItemFree)(EOTList* tpList, EOTListItem* tpItem);

void EOS_List_Create(EOTList* tpList);
// 清空整个列表
void EOS_List_Clear(EOTList* tpList, EOFuncListItemFree tItemFree);
// 释放单个内存
void EOS_List_ItemFree(EOTList* tpList, EOTListItem* tpListItm, EOFuncListItemFree tItemFree);

EOTListItem* EOS_List_Push(EOTList* tpList, int nId, void* pData, int nLength);
EOTListItem* EOS_List_Pop(EOTList* tpList);
EOTListItem* EOS_List_Top(EOTList* tpList);

#endif /* EOS_LIST_H_ */
