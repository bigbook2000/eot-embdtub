/*
 * eob_dma.h
 *
 *  Created on: Jul 7, 2023
 *      Author: hjx
 */

#ifndef EOB_DMA_H_
#define EOB_DMA_H_

#include <stdint.h>

#include "eos_buffer.h"

#include "stm32f4xx_ll_dma.h"

//
// DMA缓存
//
typedef struct _stEOTDMAInfo
{
	// DMA
	DMA_TypeDef* dma_t;
	// 流通道
	uint32_t stream_t;

	// 当前剩余
	uint32_t retain;
	// 临时记录上一次剩余大小
	uint32_t retain_last;

	// 数据缓存，可复用
	EOTBuffer* data;
}
EOTDMAInfo;

void EOB_DMA_Recv_Init(EOTDMAInfo* tDMAInfo, DMA_TypeDef* tDMATypeDef, uint32_t nStream, uint32_t nPeriphAddress, uint16_t nSize);
int EOB_DMA_Recv(EOTDMAInfo* tDMAInfo, EOTBuffer* tRecv);
int EOB_DMA_Recv_Circular(EOTDMAInfo* tDMAInfo, EOTBuffer* tRecv);

void EOB_DMA_Send_Init(EOTDMAInfo* tDMAInfo, DMA_TypeDef* tDMATypeDef, uint32_t nStream, uint32_t nPeriphAddress, uint16_t nSize);
void EOB_DMA_Send(EOTDMAInfo* tDMAInfo, void* pSend, int nLength);


#endif /* EOB_DMA_H_ */
