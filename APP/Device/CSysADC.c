/*
 * CSysADC.c
 *
 *  Created on: Feb 2, 2024
 *      Author: hjx
 */

#include "CDevice.h"

#include <string.h>
#include <stdlib.h>

#include "stm32f4xx_ll_dma.h"
#include "stm32f4xx_ll_adc.h"

#include "cmsis_os.h"

#include "eos_inc.h"
#include "eob_tick.h"
#include "eob_debug.h"

#define DMA_ADC					DMA2
#define DMA_STREAM_ADC			LL_DMA_STREAM_0

// 毫秒
#define SAMPLE_TIME				6000

#define ADC_CHANNEL_COUNT 		2
static uint16_t s_ADCDMABuffer[ADC_CHANNEL_COUNT] = { 0, 0 };



typedef struct _stADCData
{
	// 索引
	uint8_t id;
	uint8_t r1;

	uint16_t value;
	uint16_t min;
	uint16_t max;

	double total;
	uint32_t count;
}
TADCData;

// 2个通道
static TADCData s_ADCData[ADC_CHANNEL_COUNT];

void SysADC_Start(TDevInfo* pDevInfo, uint64_t tick, EOTDate* pDate)
{
}
void SysADC_End(TDevInfo* pDevInfo, uint64_t tick, EOTDate* pDate)
{
}

static uint64_t s_TickADC = 0L;
void SysADC_Update(uint8_t nActiveId, TDevInfo* pDevInfo, uint64_t tick, EOTDate* pDate)
{
	// 48M Div6 = 8M
	// 480+15个采样周期 = 62.5us

	// 二次滤波
	int i;
	uint16_t w;
	for (i=0; i<ADC_CHANNEL_COUNT; i++)
	{
		w = s_ADCDMABuffer[i];
		s_ADCData[i].total += w;
		s_ADCData[i].count++;

		if (s_ADCData[i].min > w) s_ADCData[i].min = w;
		if (s_ADCData[i].max < w) s_ADCData[i].max = w;
	}

	double d;
	if (EOB_Tick_Check(&s_TickADC, SAMPLE_TIME))
	{
		for (i=0; i<ADC_CHANNEL_COUNT; i++)
		{
			if (s_ADCData[i].count > 0)
			{
				// 10 / 15
				d = s_ADCData[i].total / s_ADCData[i].count;

				if (d > 0xFFF) d = 0xFFF;
				if (d < 0x0) d = 0x0;

				s_ADCData[i].value = d;

				_T("ADC数值[%d]: %d (%d, %d) (%0.2f / %d)", i,
						s_ADCData[i].value, s_ADCData[i].min, s_ADCData[i].max,
						s_ADCData[i].total, s_ADCData[i].count);

				s_ADCData[i].total = 0.0;
				s_ADCData[i].count = 0;
				s_ADCData[i].min = 0x7FFF;
				s_ADCData[i].max = 0;
			}
		}
	}
}

void F_Device_SysADC_Init(TDevInfo* pDevInfo, char* sType, char* sParams)
{
	if (strcmp(sType, "SysADC") != 0) return;

	_T("初始化ADC DMA");

	memset(&s_ADCData, 0, sizeof(TADCData) * ADC_CHANNEL_COUNT);
	int i;
	for (i=0; i<0; i++)
	{
		s_ADCData[i].min = 0x7FFF;
		s_ADCData[i].max = 0;
	}

	LL_DMA_SetMemoryAddress(DMA_ADC, DMA_STREAM_ADC, (uint32_t)(&s_ADCDMABuffer));
	LL_DMA_SetDataLength(DMA_ADC, DMA_STREAM_ADC, ADC_CHANNEL_COUNT); // Halfword
	LL_DMA_SetPeriphAddress(DMA_ADC, DMA_STREAM_ADC, (uint32_t)(&ADC1->DR));
	LL_DMA_EnableStream(DMA_ADC, DMA_STREAM_ADC);

	// 不要在启动之后调用任何配置，否则引起错乱
	LL_ADC_Enable(ADC1);

	pDevInfo->device = DEVICE_SYS_ADC;

	// 注册事件
	pDevInfo->start_cb = (FuncDeviceStart)SysADC_Start;
	pDevInfo->end_cb = (FuncDeviceEnd)SysADC_End;
	pDevInfo->update_cb = (FuncDeviceUpdate)SysADC_Update;
}

