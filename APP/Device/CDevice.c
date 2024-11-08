/*
 * CDevice.c
 *
 *  Created on: Jan 27, 2024
 *      Author: hjx
 */

#include "CDevice.h"

#include <string.h>
#include <stdlib.h>

#include "cmsis_os.h"
#include "crc.h"

#include "stm32f4xx_ll_utils.h"
#include "stm32f4xx_ll_usart.h"
#include "stm32f4xx_ll_dma.h"
#include "stm32f4xx_ll_gpio.h"

#include "eos_inc.h"
#include "eob_dma.h"
#include "eob_uart.h"
#include "eob_debug.h"

#include "Config.h"
#include "Global.h"

static EOTDMAInfo s_DMAInfoRecv3;
static EOTDMAInfo s_DMAInfoRecv4;
static EOTDMAInfo s_DMAInfoRecv5;

D_STATIC_BUFFER_DECLARE(s_DMABufferRecv3, SIZE_256)
D_STATIC_BUFFER_DECLARE(s_DMABufferRecv4, SIZE_256)
D_STATIC_BUFFER_DECLARE(s_DMABufferRecv5, SIZE_256)

static EOTDMAInfo s_DMAInfoSend3;
D_STATIC_BUFFER_DECLARE(s_DMABufferSend3, SIZE_128)
static EOTDMAInfo s_DMAInfoSend4;
D_STATIC_BUFFER_DECLARE(s_DMABufferSend4, SIZE_128)
static EOTDMAInfo s_DMAInfoSend5;
D_STATIC_BUFFER_DECLARE(s_DMABufferSend5, SIZE_128)

void F_SetModbusRTU(TDevInfo* pDevInfo, char* sCommand)
{
	// ת���������ַ�������д������Ҫ����256
	int nLen = strnlen(sCommand, SIZE_256);
	if ((nLen+1) % 3 != 0)
	{
		_T("*** �豸Э��������ȴ���[%d]: %s", nLen, sCommand);
		return;
	}
	int i;
	uint8_t d1, d2;

	uint8_t nCount = (nLen+1) / 3;
	uint8_t* pData = pvPortMalloc(nCount);
	_T("�豸Э�����[%d/%d] %s", nLen, nCount, sCommand);

	for (i=0; i<nCount; i++)
	{
		d1 = sCommand[i * 3 + 0];
		d2 = sCommand[i * 3 + 1];

		CHAR_HEX(d1);
		CHAR_HEX(d2);

		pData[i] = (d1<<4)+d2;
	}

	pDevInfo->update_flag = UPDATE_FLAG_TIME; // ��Ƭִ��

	pDevInfo->param_count = nCount;
	pDevInfo->param = pData;
}

int F_CheckModbusRTU(TDevInfo* pDevInfo, uint8_t* pData, int nLength)
{
	if (nLength < 6) return -1;

	// ����̫�������ܴ���
	if (nLength >= 128) return nLength;

	// ModbusRTU ��1λ��ַ�룬��2λ������
	uint8_t address = pDevInfo->param[0];
	uint8_t action = pDevInfo->param[1];

	// ��������ʼ��ʼ
	int i;
	uint8_t len;
	uint8_t c1, c2;
	uint16_t ck, cf;
	for (i=2; i<nLength; i++)
	{
		if (pData[i-2] == address && pData[i-1] == action)
		{
			len = pData[i];
			if (len > DEV_DATA_MAX)
			{
				_T("*** �豸���ݳ��ȳ��� %d/%d", len, DEV_DATA_MAX);
				return nLength;
			}

			// 01 03 04 D1 D2 D3 D4 CF CF
			if ((nLength - i - 1) < (len + 2)) continue;

			c1 = pData[i + len + 1];
			c2 = pData[i + len + 2];

			// ��λ��ǰ����λ�ں�
			ck = c2 << 8| c1;
			cf = F_ModbusCRC16(&pData[i-2], len + 3);

			if (ck != cf)
			{
				_T("Modbus[%d/%d]: %02X %02X %04X/%04X, len = %d(%d)",
						i, nLength, c1, c2, ck, cf, len, i + len + 3);
			}
			else
			{
				pDevInfo->data_count = len;
				memcpy(pDevInfo->data, &pData[i+1], len);

				//_T("�豸����");
				_Tmb(pDevInfo->data, pDevInfo->data_count);
			}

			// i Ϊ�������ڵ�������+1��Ϊ���ݳ��ȣ���+2У��λ
			return i + len + 3;
		}
	}

	// δ��ȷ�������������ݼ�����ȡ
	return 0;
}


/**
 * ��ʼ�����ڲ���
 */
static void UartInit(char* sName, USART_TypeDef* uart,
		DMA_TypeDef* dmaRecv, uint32_t nStreamRecv,
		EOTDMAInfo* pDMAInfoRecv, EOTBuffer* pBufferRecv,
		DMA_TypeDef* dmaSend, uint32_t nStreamSend,
		EOTDMAInfo* pDMAInfoSend, EOTBuffer* pBufferSend)
{
	char* sUart = F_ConfigGetString(sName);
	_T("��ʼ��%s: %s", sName, sUart);

	char s[VALUE_SIZE];
	char* ss[8];
	uint8_t cnt = 8;
	// 9600,8,0,NONE

	strcpy(s, sUart); // �����ƻ��Էָ�
	if (EOG_SplitString(s, -1, ',', (char**)ss, &cnt) != 4)
	{
		_T("*** �������ô���");
		return;
	}

	//uint32_t nBaudRate = atoi(ss[0]);
	//EOB_Uart_Set(uart, nBaudRate);

	// ����DMA
	EOB_DMA_Recv_Init(pDMAInfoRecv, dmaRecv,
			nStreamRecv, (uint32_t)&(uart->DR),
			pBufferRecv);
	// DMA����
	LL_USART_EnableDMAReq_RX(uart);

	EOB_DMA_Send_Init(pDMAInfoSend, dmaSend,
			nStreamSend, (uint32_t)&(uart->DR),
			pBufferSend);
	// DMA����
	LL_USART_EnableDMAReq_TX(uart);
}


static void DMAUpdate(EOTDMAInfo* pDMAInfo, EOTBuffer* pBuffer)
{
	// δ��ʼ��
	if (pDMAInfo->dma_t == NULL) return;

	int ret = EOB_DMA_Recv(pDMAInfo);
	if (ret < 0)
	{
		_T("**** DMA Error [%d] %d - %d",
				pDMAInfo->data->length, pDMAInfo->retain, pDMAInfo->retain_last);
	}

	if (ret <= 0) return;
	//_Tmb(pBuffer->buffer, ret);
}

void F_UART_DMA_Init(void)
{
	// ��ʼ������
	D_STATIC_BUFFER_INIT(s_DMABufferRecv3, SIZE_256);
	D_STATIC_BUFFER_INIT(s_DMABufferRecv4, SIZE_256);
	D_STATIC_BUFFER_INIT(s_DMABufferRecv5, SIZE_256);

	D_STATIC_BUFFER_INIT(s_DMABufferSend3, SIZE_128);
	D_STATIC_BUFFER_INIT(s_DMABufferSend4, SIZE_128);
	D_STATIC_BUFFER_INIT(s_DMABufferSend5, SIZE_128);

	UartInit("uart3", USART3,
			DMA1, LL_DMA_STREAM_1, &s_DMAInfoRecv3, &s_DMABufferRecv3,
			DMA1, LL_DMA_STREAM_3, &s_DMAInfoSend3, &s_DMABufferSend3);
	UartInit("uart4", UART4,
			DMA1, LL_DMA_STREAM_2, &s_DMAInfoRecv4, &s_DMABufferRecv4,
			DMA1, LL_DMA_STREAM_4, &s_DMAInfoSend4, &s_DMABufferSend4);
	UartInit("uart5", UART5,
			DMA1, LL_DMA_STREAM_0, &s_DMAInfoRecv5, &s_DMABufferRecv5,
			DMA1, LL_DMA_STREAM_7, &s_DMAInfoSend5, &s_DMABufferSend5);

}
void F_UART_DMA_Update(void)
{
	DMAUpdate(&s_DMAInfoRecv3, &s_DMABufferRecv3);
	DMAUpdate(&s_DMAInfoRecv4, &s_DMABufferRecv4);
	DMAUpdate(&s_DMAInfoRecv5, &s_DMABufferRecv5);
}

void F_UART_DMA_Send(TDevInfo* pDevInfo)
{
	_Tmb(pDevInfo->param, pDevInfo->param_count);

	EOTDMAInfo* tDMAInfo;
	if (pDevInfo->type == DEVICE_TYPE_UART3)
		tDMAInfo = &s_DMAInfoSend3;
	else if (pDevInfo->type == DEVICE_TYPE_UART4)
		tDMAInfo = &s_DMAInfoSend4;
	else if (pDevInfo->type == DEVICE_TYPE_UART5)
		tDMAInfo = &s_DMAInfoSend5;

	EOS_Buffer_Copy(tDMAInfo->data, pDevInfo->param, pDevInfo->param_count);
	EOB_DMA_Send(tDMAInfo);

	//			LL_DMA_SetMemoryAddress(DMA1, LL_DMA_STREAM_7, (uint32_t)t);
	//
	//			LL_DMA_ClearFlag_TC7(DMA1);
	//			LL_DMA_DisableStream(DMA1, LL_DMA_STREAM_7);
	//			LL_DMA_SetDataLength(DMA1, LL_DMA_STREAM_7, 8);
	//			LL_DMA_EnableStream(DMA1, LL_DMA_STREAM_7);
}

EOTBuffer* F_UART_DMA_Buffer(TDevInfo* pDevInfo)
{
	EOTDMAInfo* tDMAInfo;
	if (pDevInfo->type == DEVICE_TYPE_UART3)
		tDMAInfo = &s_DMAInfoRecv3;
	else if (pDevInfo->type == DEVICE_TYPE_UART4)
		tDMAInfo = &s_DMAInfoRecv4;
	else if (pDevInfo->type == DEVICE_TYPE_UART5)
		tDMAInfo = &s_DMAInfoRecv5;
	else
		return NULL;

	return tDMAInfo->data;
}


