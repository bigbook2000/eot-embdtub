/*
 * eob_uart.c
 *
 *  Created on: Jul 7, 2023
 *      Author: hjx
 */

#include "eob_uart.h"

// ³õÊ¼»¯´®¿Ú
void EOB_Uart_Set(USART_TypeDef* uart, uint32_t nBaudRate)
{
	// LL_USART_Disable();

	//NVIC_SetPriority(USART1_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),5, 0));
	//NVIC_EnableIRQ(USART1_IRQn);

	LL_USART_InitTypeDef USART_InitStruct = {0};

	USART_InitStruct.BaudRate = nBaudRate;
	USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_8B;
	USART_InitStruct.StopBits = LL_USART_STOPBITS_1;
	USART_InitStruct.Parity = LL_USART_PARITY_NONE;
	USART_InitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;
	USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
	USART_InitStruct.OverSampling = LL_USART_OVERSAMPLING_16;

	LL_USART_Init(uart, &USART_InitStruct);
	LL_USART_ConfigAsyncMode(uart);

	LL_USART_Enable(uart);
}

void EOB_Uart_Send(USART_TypeDef* uart, void* pData, int nLength)
{
	int i;
	uint8_t* p = pData;
	for (i=0; i<nLength; i++)
	{
		while (LL_USART_IsActiveFlag_TC(uart) == RESET) { }
		LL_USART_TransmitData8(uart, p[i]);
	}

	//LL_USART_ClearFlag_TC(uart);
}
