/*
 * CDevManager.c
 *
 *  Created on: Dec 29, 2023
 *      Author: hjx
 * USART2 标准TTL串口，用于LCD显示
 * USART3 485从机，用于进行数据交换
 *
 * UART4 485，LED交互设备
 * UART5 485主机，传感器输入
 */

#include "CDevManager.h"

#include <string.h>
#include <stdlib.h>

#include "cmsis_os.h"

#include "eos_inc.h"
#include "eos_timer.h"
#include "eob_debug.h"
#include "eob_date.h"
#include "eob_tick.h"
#include "eob_w25q.h"

#include "eob_uart.h"

#include "Config.h"
#include "CDevice.h"
#include "CSensor.h"

static TDevInfo* s_ListDevice = NULL;
static uint8_t s_DeviceCount = 0;


static void DeviceInit(TDevInfo* pDevInfo, int nIndex)
{
	char* s;
	pDevInfo->id = nIndex - 1;
	pDevInfo->status = 0;

	pDevInfo->data_count = 0;
	pDevInfo->param_count = 0;
	pDevInfo->param = NULL;

	s = F_ConfigGetString("device%d.type", nIndex);
	if (strnstr(s, "UART3", 8) != NULL)
		pDevInfo->type = DEVICE_TYPE_UART3;
	else if (strnstr(s, "UART4", 8) != NULL)
		pDevInfo->type = DEVICE_TYPE_UART4;
	else if (strnstr(s, "UART5", 8) != NULL)
		pDevInfo->type = DEVICE_TYPE_UART5;
	else if (strnstr(s, "INPUT1", 8) != NULL)
		pDevInfo->type = DEVICE_TYPE_INPUT1;
	else if (strnstr(s, "INPUT2", 8) != NULL)
		pDevInfo->type = DEVICE_TYPE_INPUT2;
	else if (strnstr(s, "OUTPUT1", 8) != NULL)
		pDevInfo->type = DEVICE_TYPE_OUTPUT1;
	else if (strnstr(s, "OUTPUT2", 8) != NULL)
		pDevInfo->type = DEVICE_TYPE_OUTPUT1;
	else if (strnstr(s, "ADC1", 8) != NULL)
		pDevInfo->type = DEVICE_TYPE_ADC1_8;
	else if (strnstr(s, "ADC2", 8) != NULL)
		pDevInfo->type = DEVICE_TYPE_ADC1_9;
	else
	{
		_T("*** 设备类型错误 %s", s);
	}

	char* sType = F_ConfigGetString("device%d.protocol", nIndex);
	char* sParams = F_ConfigGetString("device%d.command", nIndex);

	pDevInfo->device = DEVICE_NONE;

	F_Device_SysSwitch_Init(pDevInfo, sType, sParams);
	F_Device_SysADC_Init(pDevInfo, sType, sParams);
	F_Device_ModbusRTU_Init(pDevInfo, sType, sParams);
	F_Device_TempHumi_Init(pDevInfo, sType, sParams);

	if (pDevInfo->device == DEVICE_NONE)
	{
		_T("未配置设备类型 %s", sType);
	}
}

void F_DevManager_Init(void)
{
	int i;
	int cnt;

	// 初始化DMA
	F_UART_DMA_Init();

	cnt = F_ConfigGetInt32("device.count");
	s_DeviceCount = cnt;
	if (cnt > 0)
	{
		s_ListDevice = pvPortMalloc(sizeof(TDevInfo) * s_DeviceCount);
		if (s_ListDevice == NULL)
		{
			_D("*** xAlloc NULL");
		}

		for (i=0; i<cnt; i++)
		{
			DeviceInit(&s_ListDevice[i], i+1);
		}
	}

	cnt = F_ConfigGetInt32("sensor.count");
	if (cnt <= 0)
	{
		_T("未配置传感器参数");
		return;
	}
	TSensor* pSensorList = F_Sensor_Init(cnt);

	uint8_t nIndex;
	char* sName;
	char* sDevice;
	char* sTick;
	char* sData;

	for (i=0; i<cnt; i++)
	{
		nIndex = i + 1;
		sName = F_ConfigGetString("sensor%d.name", nIndex);
		sDevice = F_ConfigGetString("sensor%d.device", nIndex);
		sData = F_ConfigGetString("sensor%d.data", nIndex);
		sTick = F_ConfigGetString("sensor%d.tick", nIndex);

		pSensorList[i].id = i;
		F_Sensor_Type(&(pSensorList[i]), sName, sDevice, sTick, sData);
	}
}

//uint8_t t[8] = { 0x01, 0x04, 0x00, 0x00, 0x00, 0x02, 0x71, 0xCB };

static uint8_t s_DeviceIndex = 0xFF;
static uint64_t s_TickDevice = 0L;
void F_DevManager_Update(uint64_t tick)
{
	// 设备要分片，每隔10秒
	uint64_t nDelay;
	TDevInfo* pDevInfo;
	int i;

	EOTDate tDate = {0};
	EOB_Date_Get(&tDate);

	// 更新DMA
	F_UART_DMA_Update();

	if (s_DeviceCount <= 0) return;

	nDelay = 10000 / s_DeviceCount;
	if (EOB_Tick_Check(&s_TickDevice, nDelay))
	{
		// 当前激活时间片
		if (s_DeviceIndex < s_DeviceCount)
		{
			pDevInfo = &s_ListDevice[s_DeviceIndex];

			if (pDevInfo->end_cb != NULL)
				pDevInfo->end_cb(pDevInfo, tick, &tDate);
		}

		s_DeviceIndex++;
		if (s_DeviceIndex >= s_DeviceCount) s_DeviceIndex = 0;
		pDevInfo = &s_ListDevice[s_DeviceIndex];

		if (pDevInfo->start_cb != NULL)
			pDevInfo->start_cb(pDevInfo, tick, &tDate);
	}

	for (i=0; i<s_DeviceCount; i++)
	{
		pDevInfo = &s_ListDevice[i];
		if (pDevInfo->update_cb != NULL)
			pDevInfo->update_cb(s_DeviceIndex, pDevInfo, tick, &tDate);
	}

	// 定时处理传感器
	F_Sensor_Update(tick, &tDate);
}
