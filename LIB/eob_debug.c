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
#include "stm32f4xx_ll_utils.h"

#include "eos_inc.h"

#include "eos_buffer.h"
#include "eob_date.h"
#include "eob_dma.h"

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

// 双缓存输出，提高并发效率
#define DEBUG_OUTPUT_CACHE_SIZE		8192
char* s_DebugOutputCache1 = NULL;
char* s_DebugOutputCache2 = NULL;
int s_DebugOutputCacheLength1 = 0;

#ifdef _DEBUG_TASK_

#include "cmsis_os.h"

#define PRINT_ASYNC // 异步打印

#define TASK_LOCK_BEGIN()		vTaskSuspendAll() // 进入锁
#define TASK_LOCK_END()			xTaskResumeAll() // 进入锁

#else // #ifdef _DEBUG_TASK_

#define TASK_LOCK_BEGIN()
#define TASK_LOCK_END()

#endif // #ifdef _DEBUG_TASK_

#define PRINT_WARNING 				"\n*** **** [%4d/%4d] *** ***\n\n"

#define UART_DEBUG 						USART1
#define DMA_DEBUG						DMA2
#define DMA_STREAM_DEBUG_INPUT			LL_DMA_STREAM_2
#define DMA_STREAM_DEBUG_OUTPUT			LL_DMA_STREAM_7

// 缓存大小，要保证消费大于生产
#define DEBUG_BUFFER_SIZE			2048
// DMA输入数据缓存
D_STATIC_BUFFER_DECLARE(s_BufferDebugInput, DEBUG_BUFFER_SIZE)
// DMA输入信息
static EOTDMAInfo s_DMAInfoDebugInput;

//// DMA输出数据缓存
//D_STATIC_BUFFER_DECLARE(s_BufferDebugOutput, DEBUG_BUFFER_SIZE)
// DMA输出信息
static EOTDMAInfo s_DMAInfoDebugOutput;


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
	// 分配DMA输入缓存
	D_STATIC_BUFFER_INIT(s_BufferDebugInput, DEBUG_BUFFER_SIZE)

	// 分配输出缓存
	s_DebugOutputCache1 = malloc(DEBUG_OUTPUT_CACHE_SIZE);
	if (s_DebugOutputCache1 == NULL)
	{
		printf("*** Memory limit\n");
	}
	s_DebugOutputCacheLength1 = 0;

#ifdef PRINT_ASYNC

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
			DMA_DEBUG, DMA_STREAM_DEBUG_INPUT, (uint32_t)&(UART_DEBUG->DR),
			DEBUG_BUFFER_SIZE);
	// DMA接收中断
	LL_USART_EnableDMAReq_RX(UART_DEBUG);

	EOB_DMA_Recv_Init(&s_DMAInfoDebugOutput,
			DMA_DEBUG, DMA_STREAM_DEBUG_OUTPUT, (uint32_t)&(UART_DEBUG->DR),
			DEBUG_BUFFER_SIZE);
	// DMA发送中断
	LL_USART_EnableDMAReq_TX(UART_DEBUG);
}

/**
 * 移除中断，防止跳转loader时异常
 */
void EOB_Debug_DeInit(void)
{
	LL_DMA_DisableStream(DMA_DEBUG, DMA_STREAM_DEBUG_INPUT);
	LL_DMA_DeInit(DMA_DEBUG, DMA_STREAM_DEBUG_INPUT);
	LL_USART_Disable(UART_DEBUG);

	LL_DMA_DisableStream(DMA_DEBUG, DMA_STREAM_DEBUG_OUTPUT);
	LL_DMA_DeInit(DMA_DEBUG, DMA_STREAM_DEBUG_OUTPUT);
	LL_USART_Disable(UART_DEBUG);
}

EOTBuffer* EOB_Debug_InputData(void)
{
	return &s_BufferDebugInput;
}


/**
 * 重定义
 * 基本的调试信息输出
 * 加入缓存，并不是真正打印
 */
void EOG_PrintFormat(char* sInfo, ...)
{
	// 进入锁
	TASK_LOCK_BEGIN();

	va_list args;
	va_start(args, sInfo);
	int len = vsnprintf(&s_DebugOutputCache1[s_DebugOutputCacheLength1],
			(DEBUG_OUTPUT_CACHE_SIZE - s_DebugOutputCacheLength1), sInfo, args);
	va_end(args);

	// 长度偏移
	s_DebugOutputCacheLength1 += len;

	TASK_LOCK_END();
	// 退出锁
}


/**
 * 重定义
 * 输出换行
 */
void EOG_PrintLine(void)
{
	// 进入锁
	TASK_LOCK_BEGIN();

	if (s_DebugOutputCacheLength1 < (DEBUG_OUTPUT_CACHE_SIZE - 1))
	{
		s_DebugOutputCache1[s_DebugOutputCacheLength1] = '\n';
		s_DebugOutputCacheLength1++;
		s_DebugOutputCache1[s_DebugOutputCacheLength1] = '\0';
	}

	TASK_LOCK_END();
	// 退出锁
}


/**
 * 格式化输出时间
 */
void EOG_PrintTime(void)
{
	EOTDate tpDate;
	EOB_Date_Get(&tpDate);

	// 进入锁
	TASK_LOCK_BEGIN();

	int len = snprintf(&s_DebugOutputCache1[s_DebugOutputCacheLength1],
			(DEBUG_OUTPUT_CACHE_SIZE - s_DebugOutputCacheLength1),
			"[%02d/%02d %02d:%02d:%02d]",
			tpDate.month, tpDate.date, tpDate.hour, tpDate.minute, tpDate.second);
	s_DebugOutputCacheLength1 += len;

	TASK_LOCK_END();
	// 退出锁
}

/**
 * 打印内存，主要用于调试
 */
void EOG_PrintBin(void* pData, int nLength)
{
	uint8_t* pBuffer = pData;
	int i, pos;

	int nLength0 = nLength;

	// 限制，最多输出64个，调试用
	if (nLength > 64) nLength = 64;

	pos = s_DebugOutputCacheLength1;

	// 进入锁
	TASK_LOCK_BEGIN();

	if ((s_DebugOutputCacheLength1 + nLength * 3) < (DEBUG_OUTPUT_CACHE_SIZE-8))
	{
		for (i=0; i<nLength; i++)
		{
			pos += snprintf(&s_DebugOutputCache1[pos],
					(DEBUG_OUTPUT_CACHE_SIZE - pos), "%02X ", *pBuffer);
			++pBuffer;
		}
		if (nLength0 != nLength)
		{
			// 表示截断
			pos += snprintf(&s_DebugOutputCache1[pos],
					(DEBUG_OUTPUT_CACHE_SIZE - pos), " ....");
		}

		s_DebugOutputCache1[pos] = '\n';
		++pos;
		s_DebugOutputCache1[pos] = '\0';
		// 不计算结束符
		s_DebugOutputCacheLength1 = pos;
	}
	else
	{
		pos += snprintf(&s_DebugOutputCache1[pos],
				(DEBUG_OUTPUT_CACHE_SIZE - pos), "<%04d> ... ... ", nLength0);
	}

	TASK_LOCK_END();
	// 退出锁
}

void EOG_PrintSend(void)
{
	// 进入锁
	TASK_LOCK_BEGIN();

	// 安全保证字符结束
	s_DebugOutputCache1[DEBUG_BUFFER_SIZE-1] = '\0';

	EOTDMAInfo* tDMAInfo = &s_DMAInfoDebugOutput;

	int pos = 0;
	int count = s_DebugOutputCacheLength1;
	int i, write;
	for (i=0; i<8; i++)
	{
		if (pos >= count) break;

		write = tDMAInfo->data->size;
		if (write > count) write = count;

		// 一定要先等发送完成
		while (LL_USART_IsActiveFlag_TC(UART_DEBUG) == RESET) { }

		EOB_DMA_Send(tDMAInfo, &s_DebugOutputCache1[pos], write);

		LL_mDelay(1);

		pos += write;
		count -= write;
	}
//	if (s_DebugOutputCacheLength1 > 0)
//	{
//		printf(s_DebugOutputCache1);
//	}

	// 输出之后重置
	s_DebugOutputCacheLength1 = 0;

	TASK_LOCK_END();
	// 退出锁

	// 在输出中喂狗
	// 若无输出，则认为已经死机
	LL_IWDG_ReloadCounter(IWDG);
}

// 最后剩余大小
int s_DebugDMARetain = DEBUG_BUFFER_SIZE;

// DMA模式
// 要保证消费大于生产
void EOB_Debug_Update(void)
{
	//uint8_t* pBuffer;
	//int i, pos;
	int nLen = EOB_DMA_Recv(&s_DMAInfoDebugInput, &s_BufferDebugInput);
	if (nLen < 0)
	{
		_T("**** Debug DMA Error\n");
	}
	else if (nLen > 0)
	{
		s_BufferDebugInput.buffer[s_BufferDebugInput.length] = '\0';
		_T("Debug DMA 接收数据 [%d]\n", s_BufferDebugInput.length);
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
