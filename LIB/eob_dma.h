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
// DMA����
//
typedef struct _stEOTDMAInfo
{
	// DMA
	DMA_TypeDef* dma_t;
	// ��ͨ��
	uint32_t stream_t;

	// ��ǰʣ��
	uint32_t retain;
	// ��ʱ��¼��һ��ʣ���С
	uint32_t retain_last;

	// ���ݻ��棬�ɸ���
	EOTBuffer* data;
}
EOTDMAInfo;

void EOB_DMA_Recv_Init(EOTDMAInfo* tDMAInfo, DMA_TypeDef* tDMATypeDef, uint32_t nStream, uint32_t nPeriphAddress, uint16_t nSize);
int EOB_DMA_Recv(EOTDMAInfo* tDMAInfo, EOTBuffer* tRecv);
int EOB_DMA_Recv_Circular(EOTDMAInfo* tDMAInfo, EOTBuffer* tRecv);

void EOB_DMA_Send_Init(EOTDMAInfo* tDMAInfo, DMA_TypeDef* tDMATypeDef, uint32_t nStream, uint32_t nPeriphAddress, uint16_t nSize);
void EOB_DMA_Send(EOTDMAInfo* tDMAInfo, void* pSend, int nLength);


#endif /* EOB_DMA_H_ */
