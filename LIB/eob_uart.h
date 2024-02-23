/*
 * eob_uart.h
 *
 *  Created on: Jul 7, 2023
 *      Author: hjx
 */

#ifndef EOB_UART_H_
#define EOB_UART_H_

#include "stm32f4xx_ll_usart.h"

#include <stdint.h>

// ³õÊ¼»¯´®¿Ú
void EOB_Uart_Set(USART_TypeDef* uart, uint32_t nBaudRate);
void EOB_Uart_Send(USART_TypeDef* uart, void* pData, int nLength);

#endif /* EOB_UART_H_ */
