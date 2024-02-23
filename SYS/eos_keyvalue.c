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

#define VALUE_NEW_STEP		8 // ÿ�η����С���������ظ�����

/**
 * ����һ��hashCode������ÿ�ζ�У���ַ���
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
 * ����һ���µļ�ֵ��
 * nValueSize = 0ʱ������ʼ��
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
		// ������γ�ʼ��
		pKeyValueNew->value_size = 0;
		pKeyValueNew->value = NULL;
	}

	// ���һ���µ�
	// nHashCode ��Ϊid
	EOS_List_Push(tpList, nHashCode, pKeyValueNew, newSize);

	return pKeyValueNew;
}

/**
 * ����ָ���ļ�ֵ��
 */
EOTKeyValue* EOS_KeyValue_Find(EOTList* tpList, int nHashCode, char* sKey, int nLen)
{
	EOTKeyValue* pKeyValueSel = NULL;
	EOTKeyValue* pKeyValue;
	EOTListItem* pItem = tpList->_head;

	while (pItem != NULL)
	{
		// id������ʶhashֵ
		if (pItem->id == nHashCode)
		{
			pKeyValue = (EOTKeyValue*)pItem->data;

			// ���Ȳ�ͬ�Ŀ���ֱ������
			if (pKeyValue->key_length == nLen)
			{
				if (strncmp(sKey, pKeyValue->key, KEY_LENGTH) == 0)
				{
					// �ҵ���ͬ��
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
 * ���һ����ֵ�ԣ�����������滻
 */
EOTKeyValue* EOS_KeyValue_Set(EOTList* tpList, char* sKey, char* sValue)
{
	int nKeyLen = strnlen(sKey, KEY_LENGTH);
	if (nKeyLen >= KEY_LENGTH) return NULL; // ̫����

	int nHashCode = KeyValueHashCode(sKey, nKeyLen);

	// ����������
	int nValueLen = strnlen(sValue, VALUE_SIZE)+1;
	if (nValueLen >= VALUE_SIZE) return NULL; // ̫����

	int newSize;

	EOTKeyValue* pKeyValueSel = EOS_KeyValue_Find(tpList, nHashCode, sKey, nKeyLen);
	if (pKeyValueSel == NULL)
	{
		// ��ʼ��һ���µģ���Ҫ��ֵ
		pKeyValueSel = KeyValueNew(tpList, nHashCode, sKey, nKeyLen, 0);
	}

	// ���и�ֵ
	if (pKeyValueSel->value_size < nValueLen)
	{
		// �ڴ治�� nValueLen+1 ��\0�����ռ�
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

	//_T("д�� %s=%s;", (const char*)pConfigItemSel->name, (const char*)pConfigItemSel->value);
	return pKeyValueSel;
}

char* EOS_KeyValue_Get(EOTList* tpList, char* sKey)
{
	// ��ַ���ô��ڣ�ֱ�ӷ���
	int nKeyLen = strnlen(sKey, KEY_LENGTH);
	if (nKeyLen >= KEY_LENGTH) return NULL; // ̫����

	int nHashCode = KeyValueHashCode(sKey, nKeyLen);

	EOTKeyValue* pKeyValueSel = EOS_KeyValue_Find(tpList, nHashCode, sKey, nKeyLen);

	// �����ؿգ�ֱ������µ�
	if (pKeyValueSel == NULL)
	{
		// ��ʼ���µģ�����ֵ
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
