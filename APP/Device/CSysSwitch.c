/*
 * CSysSwitch.c
 *
 *  Created on: Feb 1, 2024
 *      Author: hjx
 *
 * D3/D4 Input
 * D5/D6 Output
 * 请将IO口都设置为拉低Pull-down
 *
 */

#include "CDevice.h"

#include "stm32f4xx_ll_gpio.h"

#include "cmsis_os.h"

#include <string.h>
#include <stdio.h>

#include "eos_inc.h"
#include "eob_debug.h"
#include "eob_tick.h"



/**
 * 输入输出统一管理
 * 输出口默认设置成低电平
 */
static TCtrlInfo s_IOList[] =
{
	{ GPIOD, LL_GPIO_PIN_3, 0x0, 0x0, 0, CTRL_NONE },
	{ GPIOD, LL_GPIO_PIN_4, 0x0, 0x0, 1, CTRL_NONE },
	{ GPIOD, LL_GPIO_PIN_5, 0x0, 0x0, 2, CTRL_NONE },
	{ GPIOD, LL_GPIO_PIN_6, 0x0, 0x0, 3, CTRL_NONE }
};

#define CTRL_CHANGE_EVENT_MAX 	8
static FuncCtrlChange s_CtrlChangeEventList[CTRL_CHANGE_EVENT_MAX];
static uint8_t s_CtrlChangeEventCount = 0;


/**
 * 转换实际收到的数据从device到sensor
 */
static double DoSensorDataRt(TSensor* pSensor, void* pTag, uint8_t* pData, int nLength)
{
	TRegisterData* pRegRt;

	pRegRt = &(pSensor->data_rt);

	if (pSensor->offset >= CTRL_IO_COUNT)
	{
		_T("传感器参数设置错误 %s -> %d", pSensor->name, pSensor->offset);
		return 0.0;
	}

	TCtrlInfo* pCtrlInfo = &s_IOList[pSensor->offset];
	uint16_t v = pCtrlInfo->set;

	// 防止浮点数问题,0.9999
	double d = v + 0.1;

	// 复制到大寄存器，内部始终为double
	DATA_TYPE_SET(&(pRegRt->data), DATA_TYPE_DOUBLE, &d);

	_T("传感器实时数据 %s = (%f, %d) %f",
			pSensor->name, d, v, pRegRt->data);

	return d;
}

void SysSwitch_Start(TDevInfo* pDevInfo, uint64_t tick, EOTDate* pDate)
{
}
void SysSwitch_End(TDevInfo* pDevInfo, uint64_t tick, EOTDate* pDate)
{
}

void SysSwitch_Update(uint8_t nActiveId, TDevInfo* pDevInfo, uint64_t tick, EOTDate* pDate)
{
	TCtrlInfo* pCtrlInfo = NULL;
	// 检查输入状态

	int k, i;
	// 低电平
	uint8_t ts;

	uint8_t isChange = 0;

	for (k=0; k<CTRL_IO_COUNT; k++)
	{
		pCtrlInfo = &s_IOList[k];

		ts = LL_GPIO_IsInputPinSet(pCtrlInfo->gpio, pCtrlInfo->pin);

		// 只有变化才处理
		if (ts == pCtrlInfo->set) continue;


		pCtrlInfo->set = ts;
		_T("触发SysSwitch事件[%d]: %d", pCtrlInfo->id, ts);

		// 对于有实时要求的，通过事件触发处理
		for (i=0; i<s_CtrlChangeEventCount; i++)
		{
			if (s_CtrlChangeEventList[i] != NULL)
			{
				s_CtrlChangeEventList[i](pDevInfo, pCtrlInfo, ts);
			}
		}

		isChange = 1;
	}

	if (isChange)
	{
		//
		// 将设备采集的数据对应到sensor上
		//
		F_Sensor_DataSet(pDevInfo->id, (EOFuncSensorData)DoSensorDataRt, pDevInfo, NULL, 0);
	}
}

void F_Device_SysSwitch_Init(TDevInfo* pDevInfo, char* sType, char* sParams)
{
	if (strcmp(sType, "SysSwitch") != 0) return;

	memset(s_CtrlChangeEventList, 0, sizeof(FuncCtrlChange) * CTRL_CHANGE_EVENT_MAX);

	pDevInfo->device = DEVICE_SYS_SWITCH;

	// 注册事件
	pDevInfo->start_cb = (FuncDeviceStart)SysSwitch_Start;
	pDevInfo->end_cb = (FuncDeviceEnd)SysSwitch_End;
	pDevInfo->update_cb = (FuncDeviceUpdate)SysSwitch_Update;
}

void F_SysSwitch_ChangeEvent_Add(FuncCtrlChange tCallbackChange)
{
	if (s_CtrlChangeEventCount >= CTRL_CHANGE_EVENT_MAX)
	{
		_T("无法再触发SysSwitch事件");
		return;
	}

	s_CtrlChangeEventList[s_CtrlChangeEventCount] = tCallbackChange;
	s_CtrlChangeEventCount++;
}

/*
 * 设置输出口电平
 * id从1开始
 */
uint8_t F_SysSwitch_Output_Set(uint8_t nOutId, uint8_t nSet)
{
	if (nOutId <= 0 || nOutId > CTRL_IO_COUNT)
	{
		_T("输出端口错误: %d", nOutId);
		return CTRL_NONE;
	}

	TCtrlInfo* pCtrlInfo = &s_IOList[nOutId-1];
	_T("Switch开关[%d]: %d", pCtrlInfo->id, pCtrlInfo->set);

	pCtrlInfo->set = nSet;
	if (nSet == CTRL_SET)
		LL_GPIO_SetOutputPin(pCtrlInfo->gpio, pCtrlInfo->pin); // 设置低电平
	else
		LL_GPIO_ResetOutputPin(pCtrlInfo->gpio, pCtrlInfo->pin); // 设置高电平

	// 用于输出口
	return LL_GPIO_IsInputPinSet(pCtrlInfo->gpio, pCtrlInfo->pin);
}

uint8_t F_SysSwitch_Input_IsSet(uint8_t nInId)
{
	if (nInId <= 0 || nInId > CTRL_IO_COUNT)
	{
		_T("输入端口错误: %d", nInId);
		return 0;
	}

	TCtrlInfo* pCtrlInfo = &s_IOList[nInId-1];

	return LL_GPIO_IsInputPinSet(pCtrlInfo->gpio, pCtrlInfo->pin);
}
