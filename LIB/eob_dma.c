/*
 * eob_dma.c
 *
 *  Created on: Jul 7, 2023
 *      Author: hjx
 */

#include "eob_dma.h"
#include "string.h"
#include "stdlib.h"

void EOB_DMA_Recv_Init(EOTDMAInfo* tDMAInfo,
		DMA_TypeDef* tDMATypeDef, uint32_t nStream, uint32_t nPeriphAddress,
		uint16_t nSize)
{
	// DMA基本上初始化之后不会再改变，所以只考虑分配内存，不考虑释放
	EOTBuffer* tData = (EOTBuffer*)malloc(sizeof(EOTBuffer));
	EOS_Buffer_Create(tData, nSize);

	memset(tDMAInfo, 0, sizeof(EOTDMAInfo));

	tDMAInfo->dma_t = tDMATypeDef;
	tDMAInfo->stream_t = nStream;
	tDMAInfo->data = tData;
	tDMAInfo->retain = 0;
	tDMAInfo->retain_last = tData->size;

	LL_DMA_SetMemoryAddress(tDMAInfo->dma_t, tDMAInfo->stream_t, (uint32_t)tData->buffer);
	LL_DMA_SetDataLength(tDMAInfo->dma_t, tDMAInfo->stream_t, tData->size);
	LL_DMA_SetPeriphAddress(tDMAInfo->dma_t, tDMAInfo->stream_t, nPeriphAddress);
	LL_DMA_EnableStream(tDMAInfo->dma_t, tDMAInfo->stream_t);
}

void EOB_DMA_Send_Init(EOTDMAInfo* tDMAInfo,
		DMA_TypeDef* tDMATypeDef, uint32_t nStream, uint32_t nPeriphAddress,
		uint16_t nSize)
{
	// DMA基本上初始化之后不会再改变，所以只考虑分配内存，不考虑释放
	EOTBuffer* tData = (EOTBuffer*)malloc(sizeof(EOTBuffer));
	EOS_Buffer_Create(tData, nSize);

	memset(tDMAInfo, 0, sizeof(EOTDMAInfo));

	tDMAInfo->dma_t = tDMATypeDef;
	tDMAInfo->stream_t = nStream;
	tDMAInfo->data = tData;
	tDMAInfo->retain = 0;
	tDMAInfo->retain_last = tData->size;

	LL_DMA_SetMemoryAddress(tDMAInfo->dma_t, tDMAInfo->stream_t, (uint32_t)tData->buffer);
	LL_DMA_SetPeriphAddress(tDMAInfo->dma_t, tDMAInfo->stream_t, nPeriphAddress);

	LL_DMA_EnableStream(tDMAInfo->dma_t, tDMAInfo->stream_t);
}

/**
 * 向DMA传递发送数据
 * @param tDMAInfo
 * @param pSend
 * @param nLength
 */
void EOB_DMA_Send(EOTDMAInfo* tDMAInfo, void* pSend, int nLength)
{
	EOTBuffer* tData = tDMAInfo->data;
	// 先锁定
	LL_DMA_DisableStream(tDMAInfo->dma_t, tDMAInfo->stream_t);

	switch (tDMAInfo->stream_t)
	{
	case LL_DMA_STREAM_0:
		LL_DMA_ClearFlag_TC0(tDMAInfo->dma_t);
		break;
	case LL_DMA_STREAM_1:
		LL_DMA_ClearFlag_TC1(tDMAInfo->dma_t);
		break;
	case LL_DMA_STREAM_2:
		LL_DMA_ClearFlag_TC2(tDMAInfo->dma_t);
		break;
	case LL_DMA_STREAM_3:
		LL_DMA_ClearFlag_TC3(tDMAInfo->dma_t);
		break;
	case LL_DMA_STREAM_4:
		LL_DMA_ClearFlag_TC4(tDMAInfo->dma_t);
		break;
	case LL_DMA_STREAM_5:
		LL_DMA_ClearFlag_TC5(tDMAInfo->dma_t);
		break;
	case LL_DMA_STREAM_6:
		LL_DMA_ClearFlag_TC6(tDMAInfo->dma_t);
		break;
	case LL_DMA_STREAM_7:
		LL_DMA_ClearFlag_TC7(tDMAInfo->dma_t);
		break;
	}

	//uint32_t nAddress = LL_DMA_GetMemoryAddress(tDMATypeDef, nStream);
	EOS_Buffer_Copy(tData, pSend, nLength);

	// tData->length 数据长度
	LL_DMA_SetDataLength(tDMAInfo->dma_t, tDMAInfo->stream_t, tData->length);
	LL_DMA_EnableStream(tDMAInfo->dma_t, tDMAInfo->stream_t);
}

/**
 * DMA单次模式接收数据
 * 要保证消费大于生产
 *
 * 将数据追加 tRecv 中
 *
 */
int EOB_DMA_Recv(EOTDMAInfo* tDMAInfo, EOTBuffer* tRecv)
{
	EOTBuffer* tData = tDMAInfo->data;

	// 先锁住DMA
	LL_DMA_DisableStream(tDMAInfo->dma_t, tDMAInfo->stream_t);

	// DMA剩余大小
	uint32_t nRetain = LL_DMA_GetDataLength(tDMAInfo->dma_t, tDMAInfo->stream_t);
	int nLength = tData->size - nRetain;
	if (nLength > 0)
	{
		// 设置数据长度
		tData->length = nLength;
		// 注意，这是追加
		if (EOS_Buffer_PushBuffer(tRecv, tData) < 0)
		{
			// 目标内存不够
			return -1;
		}
		// tData->length 长度已经为0
	}

	switch (tDMAInfo->stream_t)
	{
	case LL_DMA_STREAM_0:
		LL_DMA_ClearFlag_TC0(tDMAInfo->dma_t);
		break;
	case LL_DMA_STREAM_1:
		LL_DMA_ClearFlag_TC1(tDMAInfo->dma_t);
		break;
	case LL_DMA_STREAM_2:
		LL_DMA_ClearFlag_TC2(tDMAInfo->dma_t);
		break;
	case LL_DMA_STREAM_3:
		LL_DMA_ClearFlag_TC3(tDMAInfo->dma_t);
		break;
	case LL_DMA_STREAM_4:
		LL_DMA_ClearFlag_TC4(tDMAInfo->dma_t);
		break;
	case LL_DMA_STREAM_5:
		LL_DMA_ClearFlag_TC5(tDMAInfo->dma_t);
		break;
	case LL_DMA_STREAM_6:
		LL_DMA_ClearFlag_TC6(tDMAInfo->dma_t);
		break;
	case LL_DMA_STREAM_7:
		LL_DMA_ClearFlag_TC7(tDMAInfo->dma_t);
		break;
	}

	// 继续接收
	LL_DMA_SetDataLength(tDMAInfo->dma_t, tDMAInfo->stream_t, tData->size);
	LL_DMA_EnableStream(tDMAInfo->dma_t, tDMAInfo->stream_t);

	// 返回长度
	return nLength;
}

/**
 * DMA循环模式接收数据
 */
int EOB_DMA_Recv_Circular(EOTDMAInfo* tDMAInfo, EOTBuffer* tRecv)
{
	EOTBuffer* tData = tDMAInfo->data;

	// 数据剩余长度
	tDMAInfo->retain = LL_DMA_GetDataLength(tDMAInfo->dma_t, tDMAInfo->stream_t);
	if (tDMAInfo->retain_last == tDMAInfo->retain) return 0;

	int count, index;
	if (tDMAInfo->retain_last > tDMAInfo->retain)
	{
		// 正常
		count = tDMAInfo->retain_last - tDMAInfo->retain;
		index = tData->size - tDMAInfo->retain_last;
		// 将DMA数据复制到内存中
		if (EOS_Buffer_Push(tRecv, &(tData->buffer[index]), count) < 0)
		{
			return -1;
		}
	}
	else
	{
		// 循环，数据分成了2段
		count = tDMAInfo->retain_last;
		index = tData->size - tDMAInfo->retain_last;
		// 将DMA数据复制到内存中
		if (EOS_Buffer_Push(tRecv, &(tData->buffer[index]), count) < 0)
		{
			return -1;
		}

		count = tData->size - tDMAInfo->retain;
		// 将DMA数据复制到内存中
		if (EOS_Buffer_Push(tRecv, &(tData->buffer[0]), count) < 0)
		{
			return -1;
		}
	}

	tDMAInfo->retain_last = tDMAInfo->retain;

	return tRecv->length;
}
