/*
 * eos_buffer.c
 *
 *  Created on: Jun 30, 2023
 *      Author: hjx
 */

#include "eos_buffer.h"

#include "stdint.h"
#include "stdlib.h"
#include "string.h"

EOTBuffer* EOS_Buffer_Create(EOTBuffer* tpBuffer, int nSize)
{
	// ʹ��malloc�����ٶ�OS����
	tpBuffer->buffer = malloc(nSize);
	tpBuffer->size = nSize;
	tpBuffer->length = 0;
	tpBuffer->pos = 0;
	tpBuffer->tag = 0;

	return tpBuffer;
}

EOTBuffer* EOS_Buffer_Destory(EOTBuffer* tpBuffer)
{
	if (tpBuffer != NULL)
	{
		tpBuffer->length = 0;
		tpBuffer->pos = 0;
		tpBuffer->tag = 0;

		if (tpBuffer->buffer != NULL)
		{
			free(tpBuffer->buffer);
			tpBuffer->buffer = NULL;
		}
	}

	return NULL;
}

// ���
void EOS_Buffer_Clear(EOTBuffer* tpBuffer)
{
	tpBuffer->length = 0;
	tpBuffer->pos = 0;
	tpBuffer->tag = 0;
}

void EOS_Buffer_Reset(EOTBuffer* tpBuffer)
{
	memset(tpBuffer->buffer, 0, tpBuffer->size);
}
int EOS_Buffer_Set(EOTBuffer* tpBuffer, int nPos, void* pData, int nLength)
{
	if (tpBuffer->buffer == NULL) return -1;

	// ���治��
	if ((nPos + nLength) > tpBuffer->size) return -1;

	memcpy(&tpBuffer->buffer[nPos], pData, nLength);
	return nLength;
}
int EOS_Buffer_Get(EOTBuffer* tpBuffer, int nPos, void* pData, int nLength)
{
	if (tpBuffer->buffer == NULL) return -1;

	memcpy(pData, &tpBuffer->buffer[nPos], nLength);
	return nLength;
}

/**
 * ����һ���ڴ棬��0��ʼ
 */
int EOS_Buffer_CopyBuffer(EOTBuffer* tpBufferDst, EOTBuffer* tpBufferSrc)
{
	memcpy(tpBufferDst->buffer, tpBufferSrc->buffer, tpBufferSrc->length);
	tpBufferDst->length = tpBufferSrc->length;

	return tpBufferDst->length;
}

/**
 * ����һ���ڴ棬��0��ʼ
 */
int EOS_Buffer_Copy(EOTBuffer* tpBuffer, void* pData, int nLength)
{
	memcpy(tpBuffer->buffer, pData, nLength);
	tpBuffer->length = nLength;

	return nLength;
}


int EOS_Buffer_PushOne(EOTBuffer* tpBuffer, unsigned char bByte)
{
	if (tpBuffer->buffer == NULL) return -1;

	// ��ѭ��
	if (tpBuffer->length >= tpBuffer->size) return -1;

	tpBuffer->buffer[tpBuffer->length] = bByte;
	tpBuffer->length++;

	return tpBuffer->length;
}

int EOS_Buffer_Push(EOTBuffer* tpBuffer, void* pData, int nLength)
{
	// û��������Ҫ����
	if (nLength <= 0) return tpBuffer->length;

	if (tpBuffer->buffer == NULL) return -1;

	int size = tpBuffer->size - tpBuffer->length;
	if (size > nLength) size = nLength;

	// ���治��
	if (size <= 0) return -1;

	memcpy(&tpBuffer->buffer[tpBuffer->length], pData, size);
	tpBuffer->length += size;

	if (tpBuffer->length < tpBuffer->size)
	{
		// �����ַ����������������
		tpBuffer->buffer[tpBuffer->length] = '\0';
	}

	return tpBuffer->length;
}

/**
 * ��tpBufferSrc�е�����׷�ӵ�tpBufferDst��
 * ���� tpBufferDst ����
 * ��� tpBufferSrc ����
 *
 */
int EOS_Buffer_PushBuffer(EOTBuffer* tpBufferDst, EOTBuffer* tpBufferSrc)
{
	if (tpBufferDst->buffer == NULL) return -1;

	int size = tpBufferDst->size - tpBufferDst->length;
	if (size > tpBufferSrc->length) size = tpBufferSrc->length;

	// ���治��
	if (size <= 0) return -1;

	memcpy(&tpBufferDst->buffer[tpBufferDst->length], tpBufferSrc->buffer, size);
	tpBufferDst->length += size;

	if (tpBufferDst->length < tpBufferDst->size)
	{
		// �����ַ����������������
		tpBufferDst->buffer[tpBufferDst->length] = '\0';
	}

	// ���Դ����
	tpBufferSrc->buffer[0] = '\0';
	tpBufferSrc->length = 0;

	return tpBufferDst->length;
}

/**
 * ��tpBuffer��ȡ��ָ��������nCount����
 * ����ʵ������
 */
int EOS_Buffer_Pop(EOTBuffer* tpBuffer, char* pOut, int nCount)
{
	if (tpBuffer->buffer == NULL) return -1;

	// ��������
	if (tpBuffer->length < nCount) nCount = tpBuffer->length;

	if (pOut != NULL)
	{
		memcpy(pOut, tpBuffer->buffer, nCount);
	}

	int nRetain = tpBuffer->length - nCount;
	if (nRetain > 0)
	{
		// ƽ��ʣ�µ�����
		memmove(&tpBuffer->buffer[0], &tpBuffer->buffer[nCount], nRetain);
	}

	tpBuffer->length = nRetain;

	return nCount;
}


// ��������ֱ�ӷ���һ�У���ʡ�ڴ棩
int EOS_Buffer_GetReturn(EOTBuffer* tpBuffer)
{
	if (tpBuffer->buffer == NULL) return -1;

	if (tpBuffer->length <= 1) return -1;

	unsigned char c = 0x0;
	unsigned char* p = &tpBuffer->buffer[tpBuffer->pos];
	c = *p;

	int i = 0;
	for (i=tpBuffer->pos; i<tpBuffer->length; ++i)
	{
		//printf("%02X %02X [%d]\r\n", c, *p, i);
		if (c == '\r' && (*p) == '\n') break;
		c = *p;
		++p;
	}

	// �´β���ͷ����
	tpBuffer->pos = i - 1;

	// δ�ҵ����б�ʶ����0
	if (i >= tpBuffer->length) return -1;

	// �����з���Ϊ�ַ��������������ַ�������
	tpBuffer->buffer[i - 1] = '\0';

	// i Ϊ\n���ڵ�λ��
	return i;
}

/**
 * ����Ƿ�������з�
 * ����������򿽱���pOut��pOut���������з�
 * @return ���ذ������з����ڵ�λ��
 */
int EOS_Buffer_PopReturn(EOTBuffer* tpBuffer, char* pOut, int nCount)
{
	pOut[0] = '\0';

	// nIndex Ϊ\n���ڵ�λ��
	int nIndex = EOS_Buffer_GetReturn(tpBuffer);
	if (nIndex < 0) return -1;

	// ���ò��ұ�ʶ
	tpBuffer->pos = 0;

	// ����ʱ����\r\n
	nIndex++;

	if (nIndex > nCount)
	{
		// ���治��
		EOS_Buffer_Pop(tpBuffer, NULL, nIndex);
		return -1;
	}

	EOS_Buffer_Pop(tpBuffer, pOut, nIndex);
	pOut[nIndex - 1] = '\0';

	return nIndex;
}

/**
 * ��ȡ�ַ�����ʶ
 */
int EOS_Buffer_PopFlag(EOTBuffer* tpBuffer, char* pOut, char* sFlag)
{
	pOut[0] = '\0';

	char* p = strnstr((const char*)tpBuffer->buffer, sFlag, tpBuffer->length);
	if (p == NULL) return -1;

	int cnt = (uint32_t)(p) - (uint32_t)(tpBuffer->buffer);
	if (cnt < 0) return -1;

	//printf("EOS_Buffer_PopFlag: %d\r\n", cnt);
	if (cnt > 0)
	{
		memcpy(pOut, tpBuffer->buffer, cnt);
	}
	pOut[cnt] = '\0';

	// �����ָ��
	cnt += strlen(sFlag);

	if (tpBuffer->length > cnt)
	{
		memmove(&tpBuffer->buffer[0], &tpBuffer->buffer[cnt], tpBuffer->length - cnt);
	}
	tpBuffer->length = tpBuffer->length - cnt;
	//printf("tpBuffer->length: %d\r\n", tpBuffer->length);

	return cnt;
}

int EOS_Buffer_PopBuffer(EOTBuffer* tpBuffer, EOTBuffer* tpBufferOut)
{
	int cnt = tpBuffer->length;
	if ((tpBufferOut->length + cnt) > tpBufferOut->size)
	{
		return -1;
	}

	uint8_t* p = &(tpBufferOut->buffer[tpBufferOut->length]);
	memcpy(p, tpBuffer->buffer, cnt);
	tpBufferOut->length += cnt;

	return cnt;
}

