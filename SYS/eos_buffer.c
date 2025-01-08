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
	// 使用malloc，减少对OS依赖
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

// 清空
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

	// 缓存不够
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
 * 拷贝一块内存，从0开始
 */
int EOS_Buffer_CopyBuffer(EOTBuffer* tpBufferDst, EOTBuffer* tpBufferSrc)
{
	memcpy(tpBufferDst->buffer, tpBufferSrc->buffer, tpBufferSrc->length);
	tpBufferDst->length = tpBufferSrc->length;

	return tpBufferDst->length;
}

/**
 * 拷贝一块内存，从0开始
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

	// 不循环
	if (tpBuffer->length >= tpBuffer->size) return -1;

	tpBuffer->buffer[tpBuffer->length] = bByte;
	tpBuffer->length++;

	return tpBuffer->length;
}

int EOS_Buffer_Push(EOTBuffer* tpBuffer, void* pData, int nLength)
{
	// 没有数据需要拷贝
	if (nLength <= 0) return tpBuffer->length;

	if (tpBuffer->buffer == NULL) return -1;

	int size = tpBuffer->size - tpBuffer->length;
	if (size > nLength) size = nLength;

	// 缓存不够
	if (size <= 0) return -1;

	memcpy(&tpBuffer->buffer[tpBuffer->length], pData, size);
	tpBuffer->length += size;

	if (tpBuffer->length < tpBuffer->size)
	{
		// 方便字符串操作，避免溢出
		tpBuffer->buffer[tpBuffer->length] = '\0';
	}

	return tpBuffer->length;
}

/**
 * 将tpBufferSrc中的数据追加到tpBufferDst中
 * 增加 tpBufferDst 长度
 * 清空 tpBufferSrc 长度
 *
 */
int EOS_Buffer_PushBuffer(EOTBuffer* tpBufferDst, EOTBuffer* tpBufferSrc)
{
	if (tpBufferDst->buffer == NULL) return -1;

	int size = tpBufferDst->size - tpBufferDst->length;
	if (size > tpBufferSrc->length) size = tpBufferSrc->length;

	// 缓存不够
	if (size <= 0) return -1;

	memcpy(&tpBufferDst->buffer[tpBufferDst->length], tpBufferSrc->buffer, size);
	tpBufferDst->length += size;

	if (tpBufferDst->length < tpBufferDst->size)
	{
		// 方便字符串操作，避免溢出
		tpBufferDst->buffer[tpBufferDst->length] = '\0';
	}

	// 清除源数据
	tpBufferSrc->buffer[0] = '\0';
	tpBufferSrc->length = 0;

	return tpBufferDst->length;
}

/**
 * 从tpBuffer中取出指定数量的nCount数据
 * 返回实际数量
 */
int EOS_Buffer_Pop(EOTBuffer* tpBuffer, char* pOut, int nCount)
{
	if (tpBuffer->buffer == NULL) return -1;

	// 数量不足
	if (tpBuffer->length < nCount) nCount = tpBuffer->length;

	if (pOut != NULL)
	{
		memcpy(pOut, tpBuffer->buffer, nCount);
	}

	int nRetain = tpBuffer->length - nCount;
	if (nRetain > 0)
	{
		// 平移剩下的数据
		memmove(&tpBuffer->buffer[0], &tpBuffer->buffer[nCount], nRetain);
	}

	tpBuffer->length = nRetain;

	return nCount;
}


// 不拷贝，直接返回一行（节省内存）
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

	// 下次不重头再找
	tpBuffer->pos = i - 1;

	// 未找到换行标识返回0
	if (i >= tpBuffer->length) return -1;

	// 将换行符改为字符结束符，方便字符串操作
	tpBuffer->buffer[i - 1] = '\0';

	// i 为\n所在的位置
	return i;
}

/**
 * 检查是否包含换行符
 * 如果包含，则拷贝到pOut，pOut不包含换行符
 * @return 返回包含换行符所在的位置
 */
int EOS_Buffer_PopReturn(EOTBuffer* tpBuffer, char* pOut, int nCount)
{
	pOut[0] = '\0';

	// nIndex 为\n所在的位置
	int nIndex = EOS_Buffer_GetReturn(tpBuffer);
	if (nIndex < 0) return -1;

	// 重置查找标识
	tpBuffer->pos = 0;

	// 拷贝时包含\r\n
	nIndex++;

	if (nIndex > nCount)
	{
		// 缓存不够
		EOS_Buffer_Pop(tpBuffer, NULL, nIndex);
		return -1;
	}

	EOS_Buffer_Pop(tpBuffer, pOut, nIndex);
	pOut[nIndex - 1] = '\0';

	return nIndex;
}

/**
 * 读取字符串标识
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

	// 跳过分割符
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

