/*
 * eos_keyvalue.c
 *
 *  Created on: Jan 31, 2024
 *      Author: hjx
 */

#include "eos_keyvalue.h"

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#define VALUE_NEW_STEP		8 // 每次分配大小，避免多次重复分配

/**
 * 计算一个hashCode，避免每次都校验字符串
 */
static int KeyValueHashCode(char* sStr, int nLen)
{
	int h = 0;
	if (nLen < 0) nLen = strnlen(sStr, KEY_LENGTH);
	int i;
	for (i=0; i<nLen; i++)
	{
		h = 31 * h + (uint8_t)sStr[i];
	}

	return h;
}


/**
 * 分配一个新的键值对
 * nValueSize = 0时仅仅初始化
 */
static EOTKeyValue* KeyValueNew(EOTList* tpList,
		int nHashCode, char* sKey, int nLen, int nValueSize)
{
	int newSize = sizeof(EOTKeyValue);
	EOTKeyValue* pKeyValueNew = malloc(newSize);

	strncpy(pKeyValueNew->key, sKey, KEY_LENGTH);
	pKeyValueNew->key[KEY_LENGTH - 1] = '\0';
	pKeyValueNew->key_length = nLen;

	if (nValueSize > 0)
	{
		pKeyValueNew->value = malloc(nValueSize);
		pKeyValueNew->value_size = nValueSize;
		pKeyValueNew->value[0] = '\0';
	}
	else
	{
		// 避免二次初始化
		pKeyValueNew->value_size = 0;
		pKeyValueNew->value = NULL;
	}

	// 添加一个新的
	// nHashCode 作为id
	EOS_List_Push(tpList, nHashCode, pKeyValueNew, newSize);

	return pKeyValueNew;
}

/**
 * 查找指定的键值对
 */
EOTKeyValue* EOS_KeyValue_Find(EOTList* tpList, int nHashCode, char* sKey, int nLen)
{
	EOTKeyValue* pKeyValueSel = NULL;
	EOTKeyValue* pKeyValue;
	EOTListItem* pItem = tpList->_head;

	while (pItem != NULL)
	{
		// id用来标识hash值
		if (pItem->id == nHashCode)
		{
			pKeyValue = (EOTKeyValue*)pItem->data;

			// 长度不同的可以直接跳过
			if (pKeyValue->key_length == nLen)
			{
				if (strncmp(sKey, pKeyValue->key, KEY_LENGTH) == 0)
				{
					// 找到相同的
					pKeyValueSel = pKeyValue;
					break;
				}
			}
		}

		pItem = pItem->_next;
	}

	return pKeyValueSel;
}



/**
 * 添加一个键值对，如果存在则替换
 */
EOTKeyValue* EOS_KeyValue_Set(EOTList* tpList, char* sKey, char* sValue)
{
	int nKeyLen = strnlen(sKey, KEY_LENGTH);
	if (nKeyLen >= KEY_LENGTH) return NULL; // 太长了

	int nHashCode = KeyValueHashCode(sKey, nKeyLen);

	// 包含结束符
	int nValueLen = strnlen(sValue, VALUE_SIZE)+1;
	if (nValueLen >= VALUE_SIZE) return NULL; // 太长了

	int newSize;

	EOTKeyValue* pKeyValueSel = EOS_KeyValue_Find(tpList, nHashCode, sKey, nKeyLen);
	if (pKeyValueSel == NULL)
	{
		// 初始化一个新的，不要赋值
		pKeyValueSel = KeyValueNew(tpList, nHashCode, sKey, nKeyLen, 0);
	}

	// 进行赋值
	if (pKeyValueSel->value_size < nValueLen)
	{
		// 内存不够 nValueLen+1 给\0保留空间
		newSize = (nValueLen / VALUE_NEW_STEP + 1) * VALUE_NEW_STEP;
		if (pKeyValueSel->value != NULL)
			free(pKeyValueSel->value);

		pKeyValueSel->value = malloc(newSize);
		pKeyValueSel->value_size = newSize;
	}

	if (nValueLen > 1)
		strncpy(pKeyValueSel->value, sValue, pKeyValueSel->value_size);
	else
		pKeyValueSel->value[0] = '\0';

	//_T("写入 %s=%s;", (const char*)pConfigItemSel->name, (const char*)pConfigItemSel->value);
	return pKeyValueSel;
}

char* EOS_KeyValue_Get(EOTList* tpList, char* sKey)
{
	// 地址永久存在，直接返回
	int nKeyLen = strnlen(sKey, KEY_LENGTH);
	if (nKeyLen >= KEY_LENGTH) return NULL; // 太长了

	int nHashCode = KeyValueHashCode(sKey, nKeyLen);

	EOTKeyValue* pKeyValueSel = EOS_KeyValue_Find(tpList, nHashCode, sKey, nKeyLen);

	// 不返回空，直接添加新的
	if (pKeyValueSel == NULL)
	{
		// 初始化新的，并赋值
		pKeyValueSel = KeyValueNew(tpList, nHashCode, sKey, nKeyLen, VALUE_NEW_STEP);
		pKeyValueSel->value[0] = '\0';
	}

	return pKeyValueSel->value;
}

char* EOS_KeyValue_GetString(EOTList* tpList, char* sKey, ...)
{
	char sAll[KEY_LENGTH];
	va_list args;
	va_start(args, sKey);
	vsnprintf(sAll, KEY_LENGTH, sKey, args);
	va_end(args);

	return EOS_KeyValue_Get(tpList, sAll);
}

int EOS_KeyValue_GetInt32(EOTList* tpList, char* sKey, ...)
{
	char sAll[KEY_LENGTH];
	va_list args;
	va_start(args, sKey);
	vsnprintf(sAll, KEY_LENGTH, sKey, args);
	va_end(args);

	const char* s = EOS_KeyValue_Get(tpList, sAll);
	return atoi(s);
}
double EOS_KeyValue_GetDouble(EOTList* tpList, char* sKey, ...)
{
	char sAll[KEY_LENGTH];
	va_list args;
	va_start(args, sKey);
	vsnprintf(sAll, KEY_LENGTH, sKey, args);
	va_end(args);

	const char* s = EOS_KeyValue_Get(tpList, sAll);
	return atof(s);
}
