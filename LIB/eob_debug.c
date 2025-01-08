/*
 * eob_debug.c
 *
 *  Created on: Jun 30, 2023
 *      Author: hjx
 *
 * ������ϵͳ�У����ͬʱ�����ӡ���ڣ���ʾ��Ϣ����ֲ���Ԥ֪������
 * ʹ�û�������֤���������ԣ�����Ӧ������ϵͳ
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

// ˫�����������߲���Ч��
#define DEBUG_OUTPUT_CACHE_SIZE		8192
char* s_DebugOutputCache1 = NULL;
char* s_DebugOutputCache2 = NULL;
int s_DebugOutputCacheLength1 = 0;

#ifdef _DEBUG_TASK_

#include "cmsis_os.h"

#define PRINT_ASYNC // �첽��ӡ

#define TASK_LOCK_BEGIN()		vTaskSuspendAll() // ������
#define TASK_LOCK_END()			xTaskResumeAll() // ������

#else // #ifdef _DEBUG_TASK_

#define TASK_LOCK_BEGIN()
#define TASK_LOCK_END()

#endif // #ifdef _DEBUG_TASK_

#define PRINT_WARNING 				"\n*** **** [%4d/%4d] *** ***\n\n"

#define UART_DEBUG 						USART1
#define DMA_DEBUG						DMA2
#define DMA_STREAM_DEBUG_INPUT			LL_DMA_STREAM_2
#define DMA_STREAM_DEBUG_OUTPUT			LL_DMA_STREAM_7

// �����С��Ҫ��֤���Ѵ�������
#define DEBUG_BUFFER_SIZE			2048
// DMA�������ݻ���
D_STATIC_BUFFER_DECLARE(s_BufferDebugInput, DEBUG_BUFFER_SIZE)
// DMA������Ϣ
static EOTDMAInfo s_DMAInfoDebugInput;

//// DMA������ݻ���
//D_STATIC_BUFFER_DECLARE(s_BufferDebugOutput, DEBUG_BUFFER_SIZE)
// DMA�����Ϣ
static EOTDMAInfo s_DMAInfoDebugOutput;


//#pragma import(__use_no_semihosting)
//��׼����Ҫ��֧�ֺ���
struct __FILE
{
	int handle;
};

FILE __stdout;
//����_sys_exit()�Ա���ʹ�ð�����ģʽ
void _sys_exit(int x)
{
	x = x;
}
//�ض���fputc����
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
	// ����DMA���뻺��
	D_STATIC_BUFFER_INIT(s_BufferDebugInput, DEBUG_BUFFER_SIZE)

	// �����������
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

//	// �����ж�
//	NVIC_SetPriority(USART1_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0));
//	NVIC_EnableIRQ(USART1_IRQn);
//
//	// �ֶ�����RXNE
//	LL_USART_EnableIT_RXNE(UART_DEBUG);

	// ����DMA

	EOB_DMA_Recv_Init(&s_DMAInfoDebugInput,
			DMA_DEBUG, DMA_STREAM_DEBUG_INPUT, (uint32_t)&(UART_DEBUG->DR),
			DEBUG_BUFFER_SIZE);
	// DMA�����ж�
	LL_USART_EnableDMAReq_RX(UART_DEBUG);

	EOB_DMA_Recv_Init(&s_DMAInfoDebugOutput,
			DMA_DEBUG, DMA_STREAM_DEBUG_OUTPUT, (uint32_t)&(UART_DEBUG->DR),
			DEBUG_BUFFER_SIZE);
	// DMA�����ж�
	LL_USART_EnableDMAReq_TX(UART_DEBUG);
}

/**
 * �Ƴ��жϣ���ֹ��תloaderʱ�쳣
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
 * �ض���
 * �����ĵ�����Ϣ���
 * ���뻺�棬������������ӡ
 */
void EOG_PrintFormat(char* sInfo, ...)
{
	// ������
	TASK_LOCK_BEGIN();

	va_list args;
	va_start(args, sInfo);
	int len = vsnprintf(&s_DebugOutputCache1[s_DebugOutputCacheLength1],
			(DEBUG_OUTPUT_CACHE_SIZE - s_DebugOutputCacheLength1), sInfo, args);
	va_end(args);

	// ����ƫ��
	s_DebugOutputCacheLength1 += len;

	TASK_LOCK_END();
	// �˳���
}


/**
 * �ض���
 * �������
 */
void EOG_PrintLine(void)
{
	// ������
	TASK_LOCK_BEGIN();

	if (s_DebugOutputCacheLength1 < (DEBUG_OUTPUT_CACHE_SIZE - 1))
	{
		s_DebugOutputCache1[s_DebugOutputCacheLength1] = '\n';
		s_DebugOutputCacheLength1++;
		s_DebugOutputCache1[s_DebugOutputCacheLength1] = '\0';
	}

	TASK_LOCK_END();
	// �˳���
}


/**
 * ��ʽ�����ʱ��
 */
void EOG_PrintTime(void)
{
	EOTDate tpDate;
	EOB_Date_Get(&tpDate);

	// ������
	TASK_LOCK_BEGIN();

	int len = snprintf(&s_DebugOutputCache1[s_DebugOutputCacheLength1],
			(DEBUG_OUTPUT_CACHE_SIZE - s_DebugOutputCacheLength1),
			"[%02d/%02d %02d:%02d:%02d]",
			tpDate.month, tpDate.date, tpDate.hour, tpDate.minute, tpDate.second);
	s_DebugOutputCacheLength1 += len;

	TASK_LOCK_END();
	// �˳���
}

/**
 * ��ӡ�ڴ棬��Ҫ���ڵ���
 */
void EOG_PrintBin(void* pData, int nLength)
{
	uint8_t* pBuffer = pData;
	int i, pos;

	int nLength0 = nLength;

	// ���ƣ�������64����������
	if (nLength > 64) nLength = 64;

	pos = s_DebugOutputCacheLength1;

	// ������
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
			// ��ʾ�ض�
			pos += snprintf(&s_DebugOutputCache1[pos],
					(DEBUG_OUTPUT_CACHE_SIZE - pos), " ....");
		}

		s_DebugOutputCache1[pos] = '\n';
		++pos;
		s_DebugOutputCache1[pos] = '\0';
		// �����������
		s_DebugOutputCacheLength1 = pos;
	}
	else
	{
		pos += snprintf(&s_DebugOutputCache1[pos],
				(DEBUG_OUTPUT_CACHE_SIZE - pos), "<%04d> ... ... ", nLength0);
	}

	TASK_LOCK_END();
	// �˳���
}

void EOG_PrintSend(void)
{
	// ������
	TASK_LOCK_BEGIN();

	// ��ȫ��֤�ַ�����
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

		// һ��Ҫ�ȵȷ������
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

	// ���֮������
	s_DebugOutputCacheLength1 = 0;

	TASK_LOCK_END();
	// �˳���

	// �������ι��
	// �������������Ϊ�Ѿ�����
	LL_IWDG_ReloadCounter(IWDG);
}

// ���ʣ���С
int s_DebugDMARetain = DEBUG_BUFFER_SIZE;

// DMAģʽ
// Ҫ��֤���Ѵ�������
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
		_T("Debug DMA �������� [%d]\n", s_BufferDebugInput.length);
	}

#ifdef PRINT_ASYNC
	// ������
	TASK_LOCK_BEGIN();

	int len = 0;
	if (s_DebugOutputCacheLength1 > 0)
	{
		// �����������
		len = s_DebugOutputCacheLength1;
		if (len >= DEBUG_BUFFER_SIZE-1)
		{
			// �ȴ�ӡһ����
			len = DEBUG_BUFFER_SIZE-1;
			//printf("**** Print part %d\n", s_DebugOutputCacheLength1);
		}

		memcpy(s_DebugOutputCache2, s_DebugOutputCache1, len);
		s_DebugOutputCache2[len] = '\0';

		s_DebugOutputCacheLength1 -= len; // ��������
	}
	TASK_LOCK_END();
	// �˳���

	// ���������ӡ
	if (len > 0)
		printf((const char*)s_DebugOutputCache2);

	// �������ι��
	// �������������Ϊ�Ѿ�����
	LL_IWDG_ReloadCounter(IWDG);
#endif // #ifdef PRINT_ASYNC
}
