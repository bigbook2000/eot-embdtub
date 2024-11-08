/*
 * eob_debug.c
 *
 *  Created on: Jun 30, 2023
 *      Author: hjx
 *
 * 多任务系统中，如果同时处理打印串口，显示信息会出现不可预知的情形
 * 使用缓存来保证数据完整性，以适应多任务系统
 *
 */


#include "eob_debug.h"

#include "stm32f4xx_ll_usart.h"
#include "stm32f4xx_ll_dma.h"
#include "stm32f4xx_ll_iwdg.h"

#include "eos_buffer.h"
#include "eob_date.h"
#include "eob_dma.h"

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#ifdef _DEBUG_TASK_

#include "cmsis_os.h"

#define PRINT_ASYNC // 异步打印

// 双缓存输出，提高并发效率
#define DEBUG_OUTPUT_CACHE_SIZE		8192
char* s_DebugOutputCache1 = NULL;
char* s_DebugOutputCache2 = NULL;
int s_DebugOutputCacheLength1 = 0;

#define TASK_LOCK_BEGIN()		vTaskSuspendAll() // 进入锁
#define TASK_LOCK_END()		xTaskResumeAll() // 进入锁

#else // #ifdef _DEBUG_TASK_

#define TASK_LOCK_BEGIN()
#define TASK_LOCK_END()

#endif // #ifdef _DEBUG_TASK_

#define PRINT_WARNING 				"\n*** **** [%4d/%4d] *** ***\n\n"

#define UART_DEBUG 					USART1
#define DMA_DEBUG					DMA2
#define DMA_STREAM_DEBUG			LL_DMA_STREAM_2

// 缓存大小，要保证消费大于生产
#define DEBUG_BUFFER_SIZE			2048
// DMA输入数据缓存
D_STATIC_BUFFER_DECLARE(s_BufferDebugInput, DEBUG_BUFFER_SIZE)
// DMA信息
static EOTDMAInfo s_DMAInfoDebugInput;




//#pragma import(__use_no_semihosting)
//标准库需要的支持函数
struct __FILE
{
	int handle;
};

FILE __stdout;
//定义_sys_exit()以避免使用半主机模式
void _sys_exit(int x)
{
	x = x;
}
//重定义fputc函数
int fputc(int ch, FILE *f)
{
	while (LL_USART_IsActiveFlag_TC(UART_DEBUG) == RESET) { }
	LL_USART_TransmitData8(UART_DEBUG, (uint8_t)ch);
	return ch;
}

int __io_putchar(int ch)
{
	while (LL_USART_IsActiveFlag_TC(UART_DEBUG) == RESET) { }
	LL_USART_TransmitData8(UART_DEBUG, (uint8_t)ch);
	return ch;
}

//
// Propterties -> C/C++ Build -> Settings -> MCU Sttings
// Use float
//

int _write(int file, char* ptr, int len)
{
	int i;
	for (i=0; i<len; i++)
	{
		while (LL_USART_IsActiveFlag_TC(UART_DEBUG) == RESET) { }
		LL_USART_TransmitData8(UART_DEBUG, (uint8_t)*ptr++);
	}

	return len;
}

//void USART1_IRQHandler(void)
//{
//  if (LL_USART_IsActiveFlag_RXNE(UART_DEBUG) != RESET)
//  {
//	  //USART_ClearITPendingBit(UART_DEBUG, USART_IT_RXNE);
//	  uint8_t b = LL_USART_ReceiveData8(UART_DEBUG);
//	  EOS_Buffer_PushOne(&s_DebugInputBuffer, b);
//  }
//}

void EOB_Debug_Init(void)
{
	D_STATIC_BUFFER_INIT(s_BufferDebugInput, DEBUG_BUFFER_SIZE)

	// 分配输入缓存

#ifdef PRINT_ASYNC

	s_DebugOutputCache1 = malloc(DEBUG_OUTPUT_CACHE_SIZE);
	if (s_DebugOutputCache1 == NULL)
	{
		printf("*** Memory limit\n");
	}
	s_DebugOutputCacheLength1 = 0;

	s_DebugOutputCache2 = malloc(DEBUG_BUFFER_SIZE);
	if (s_DebugOutputCache2 == NULL)
	{
		printf("*** Memory limit\n");
	}

#endif // #ifdef PRINT_ASYNC

//	// 启动中断
//	NVIC_SetPriority(USART1_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0));
//	NVIC_EnableIRQ(USART1_IRQn);
//
//	// 手动启动RXNE
//	LL_USART_EnableIT_RXNE(UART_DEBUG);

	// 配置DMA
	EOB_DMA_Recv_Init(&s_DMAInfoDebugInput,
			DMA_DEBUG, DMA_STREAM_DEBUG, (uint32_t)&(UART_DEBUG->DR),
			&s_BufferDebugInput);

	// DMA中断
	LL_USART_EnableDMAReq_RX(UART_DEBUG);
}

void EOB_Debug_DeInit(void)
{
	LL_DMA_DisableStream(DMA_DEBUG, DMA_STREAM_DEBUG);
	LL_DMA_DeInit(DMA_DEBUG, DMA_STREAM_DEBUG);
	LL_USART_Disable(UART_DEBUG);
}

EOTBuffer* EOB_Debug_InputData(void)
{
	return &s_BufferDebugInput;
}

/**
 * 基本的调试信息输出
 * 加入缓存，并不是真正打印
 */
static char s_PrintInfo[DEBUG_BUFFER_SIZE];
void EOB_Debug_Print(char* sInfo, ...)
{
	va_list args;
	va_start(args, sInfo);
	int len = vsnprintf(s_PrintInfo, DEBUG_BUFFER_SIZE-1, sInfo, args);
	va_end(args);
	s_PrintInfo[DEBUG_BUFFER_SIZE-1] = '\0';

#ifdef PRINT_ASYNC

	// 进入锁
	TASK_LOCK_BEGIN();

	if ((s_DebugOutputCacheLength1 + len) < (DEBUG_OUTPUT_CACHE_SIZE-1))
	{
		// 包含结束符
		strncpy(&s_DebugOutputCache1[s_DebugOutputCacheLength1], s_PrintInfo, len+1);
	}
	else
	{
		printf(PRINT_WARNING, len, s_DebugOutputCacheLength1);
	}
	s_DebugOutputCacheLength1 += len;

	//printf("** [%d/%d] %s **\r\n", len, s_DebugOutputLength, s_DebugOutputBufer);

	TASK_LOCK_END();
	// 退出锁
#else // #ifdef PRINT_ASYNC

	s_PrintInfo[len]= '\0'; // 空语句

	// 直接实时输出
	printf(s_PrintInfo);
	// 在输出中喂狗
	// 若无输出，则认为已经死机
	LL_IWDG_ReloadCounter(IWDG);
#endif // #ifdef PRINT_ASYNC

}

/**
 * 打印一行格式化时间的信息
 */
void EOB_Debug_PrintLine(char* sInfo, ...)
{
	EOTDate tpDate;
	EOB_Date_Get(&tpDate);

	va_list args;
	va_start(args, sInfo);
	int len = vsnprintf(s_PrintInfo, DEBUG_BUFFER_SIZE-1, sInfo, args);
	va_end(args);
	s_PrintInfo[DEBUG_BUFFER_SIZE-1] = '\0';

#ifdef PRINT_ASYNC
	// 进入锁
	TASK_LOCK_BEGIN();

	int pos = s_DebugOutputCacheLength1;
	if ((s_DebugOutputCacheLength1 + len + 17) < (DEBUG_OUTPUT_CACHE_SIZE-1))
	{
		sprintf(&s_DebugOutputCache1[pos], "[%02d/%02d %02d:%02d:%02d]",
				tpDate.month, tpDate.date, tpDate.hour, tpDate.minute, tpDate.second);
		pos += 16;

		strncpy(&s_DebugOutputCache1[pos], s_PrintInfo, len);
		pos += len;

		s_DebugOutputCache1[pos] = '\n';
		++pos;
		s_DebugOutputCache1[pos] = '\0'; // 结束符

		// 不计算结束符
		s_DebugOutputCacheLength1 = pos;
	}
	else
	{
		printf(PRINT_WARNING, len, s_DebugOutputCacheLength1);
	}

	TASK_LOCK_END();
	// 退出锁
#else // #ifdef PRINT_ASYNC

	s_PrintInfo[len]= '\0'; // 空语句

	// 直接实时输出
	printf("[%02d/%02d %02d:%02d:%02d]%s\n",
			tpDate.month, tpDate.date, tpDate.hour, tpDate.minute, tpDate.second,
			s_PrintInfo);

	// 在输出中喂狗
	// 若无输出，则认为已经死机
	LL_IWDG_ReloadCounter(IWDG);
#endif // #ifdef PRINT_ASYNC
}

// 格式化输出时间
void EOB_Debug_PrintTime()
{
	EOTDate tpDate;
	EOB_Date_Get(&tpDate);

#ifdef PRINT_ASYNC
	// 进入锁
	TASK_LOCK_BEGIN();

	if ((s_DebugOutputCacheLength1 + 16) < (DEBUG_OUTPUT_CACHE_SIZE-1))
	{
		int len = sprintf(&s_DebugOutputCache1[s_DebugOutputCacheLength1],
				"[%02d/%02d %02d:%02d:%02d]",
				tpDate.month, tpDate.date, tpDate.hour, tpDate.minute, tpDate.second);
		s_DebugOutputCacheLength1 += len;
		s_DebugOutputCache1[s_DebugOutputCacheLength1] = '\0';
	}
	else
	{
		printf(PRINT_WARNING, 16, s_DebugOutputCacheLength1);
	}

	TASK_LOCK_END();
	// 退出锁
#else // #ifdef PRINT_ASYNC

	sprintf(s_PrintInfo,
			"[%02d/%02d %02d:%02d:%02d]",
			tpDate.month, tpDate.date, tpDate.hour, tpDate.minute, tpDate.second);
	// 直接实时输出
	printf(s_PrintInfo);
	// 在输出中喂狗
	// 若无输出，则认为已经死机
	LL_IWDG_ReloadCounter(IWDG);
#endif // #ifdef PRINT_ASYNC
}
// 打印内存
void EOB_Debug_PrintBin(void* pData, int nLength)
{
	uint8_t* pBuffer = pData;
	int i, pos;

#ifdef PRINT_ASYNC
	// 进入锁
	TASK_LOCK_BEGIN();

	if ((s_DebugOutputCacheLength1 + nLength * 3) < (DEBUG_OUTPUT_CACHE_SIZE-1))
	{
		pos = s_DebugOutputCacheLength1;
		for (i=0; i<nLength; i++)
		{
			pos += sprintf(&s_DebugOutputCache1[pos], "%02X ", *pBuffer);
			++pBuffer;
		}

		s_DebugOutputCache1[pos] = '\n';
		++pos;
		s_DebugOutputCache1[pos] = '\0';
		// 不计算结束符
		s_DebugOutputCacheLength1 = pos;
	}
	else
	{
		printf(PRINT_WARNING, nLength * 3, s_DebugOutputCacheLength1);
	}

	TASK_LOCK_END();
	// 退出锁
#else // #ifdef PRINT_ASYNC

	pos = 0;
	for (i=0; i<nLength; i++)
	{
		pos += sprintf(&s_PrintInfo[pos], "%02X ", *pBuffer);
		++pBuffer;
	}

	s_PrintInfo[pos] = '\n';
	++pos;
	s_PrintInfo[pos] = '\0';
	// 直接实时输出
	printf(s_PrintInfo);
	// 在输出中喂狗
	// 若无输出，则认为已经死机
	LL_IWDG_ReloadCounter(IWDG);
#endif // #ifdef PRINT_ASYNC
}

// 最后剩余大小
int s_DebugDMARetain = DEBUG_BUFFER_SIZE;

// DMA模式
// 要保证消费大于生产
void EOB_Debug_Update(void)
{
	//uint8_t* pBuffer;
	//int i, pos;
	int nLen = EOB_DMA_Recv(&s_DMAInfoDebugInput);
	if (nLen < 0)
	{
		printf("**** Debug DMA Error\n");
	}
	else if (nLen > 0)
	{
		s_BufferDebugInput.buffer[s_BufferDebugInput.length] = '\0';
		printf("Debug DMA 接收数据 [%d]\n", s_BufferDebugInput.length);
//		pos = 0;
//		pBuffer = s_BufferDebugInput.buffer;
//		for (i=0; i<s_BufferDebugInput.length; i++)
//		{
//			pos += sprintf(&s_PrintInfo[pos], "%02X ", *pBuffer);
//			++pBuffer;
//		}
//
//		s_PrintInfo[pos] = '\n';
//		++pos;
//		s_PrintInfo[pos] = '\0';
//		// 直接实时输出
//		printf(s_PrintInfo);
	}

#ifdef PRINT_ASYNC
	// 进入锁
	TASK_LOCK_BEGIN();

	int len = 0;
	if (s_DebugOutputCacheLength1 > 0)
	{
		// 这里仅仅复制
		len = s_DebugOutputCacheLength1;
		if (len >= DEBUG_BUFFER_SIZE-1)
		{
			// 先打印一部分
			len = DEBUG_BUFFER_SIZE-1;
			//printf("**** Print part %d\n", s_DebugOutputCacheLength1);
		}

		memcpy(s_DebugOutputCache2, s_DebugOutputCache1, len);
		s_DebugOutputCache2[len] = '\0';

		s_DebugOutputCacheLength1 -= len; // 减少数量
	}
	TASK_LOCK_END();
	// 退出锁

	// 不在锁里打印
	if (len > 0)
		printf((const char*)s_DebugOutputCache2);

	// 在输出中喂狗
	// 若无输出，则认为已经死机
	LL_IWDG_ReloadCounter(IWDG);
#endif // #ifdef PRINT_ASYNC
}
