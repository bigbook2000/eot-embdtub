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


typedef struct _stCtrlInfo
{
	GPIO_TypeDef* gpio;
	uint32_t pin;

	uint8_t r1;
	uint8_t r2;

	uint8_t id;
	uint8_t set;
}
TCtrlInfo;

#define CTRL_INPUT_COUNT 		2
#define CTRL_OUTPUT_COUNT 		2

static TCtrlInfo s_InputList[] =
{
		{ GPIOD, LL_GPIO_PIN_3, 0x0, 0x0, 0x0, CTRL_NONE },
		{ GPIOD, LL_GPIO_PIN_4, 0x0, 0x0, 0x1, CTRL_NONE }
};
static TCtrlInfo s_OutputList[] =
{
		{ GPIOD, LL_GPIO_PIN_5, 0x0, 0x0, 0x0, 0x0 },
		{ GPIOD, LL_GPIO_PIN_6, 0x0, 0x0, 0x1, 0x0 }
};

#define CTRL_CHANGE_EVENT_MAX 	8
static FuncCtrlChange s_CtrlChangeEventList[CTRL_CHANGE_EVENT_MAX];
static uint8_t s_CtrlChangeEventCount = 0;

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
	if (pDevInfo->type == DEVICE_TYPE_INPUT1)
	{
		pCtrlInfo = &s_InputList[0];
	}
	else
	{
		pCtrlInfo = &s_InputList[1];
	}

	int i;
	// 低电平
	uint8_t ts;
	ts = LL_GPIO_IsInputPinSet(pCtrlInfo->gpio, pCtrlInfo->pin);
	if (ts != pCtrlInfo->set)
	{
		pCtrlInfo->set = ts;
		_T("触发SysSwitch事件[%d]: %d", pCtrlInfo->id, ts);

		for (i=0; i<s_CtrlChangeEventCount; i++)
		{
			if (s_CtrlChangeEventList[i] != NULL)
			{
				s_CtrlChangeEventList[i](pDevInfo, ts);
			}
		}
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

void F_SysSwitch_Output_Set(uint8_t nInId, uint8_t nSet)
{
	if (nInId <= 0 || nInId > CTRL_OUTPUT_COUNT)
	{
		_T("输出端口错误: %d", nInId);
		return;
	}

	TCtrlInfo* pCtrlInfo = &s_OutputList[nInId-1];
	_T("Switch开关[%d]: %d", pCtrlInfo->id, pCtrlInfo->set);

	pCtrlInfo->set = nSet;
	if (nSet == CTRL_SET)
		LL_GPIO_SetOutputPin(pCtrlInfo->gpio, pCtrlInfo->pin); // 设置低电平
	else
		LL_GPIO_ResetOutputPin(pCtrlInfo->gpio, pCtrlInfo->pin); // 设置高电平
}

uint8_t F_SysSwitch_Input_IsSet(uint8_t nInId)
{
	if (nInId <= 0 || nInId > CTRL_OUTPUT_COUNT)
	{
		_T("输入端口错误: %d", nInId);
		return 0;
	}

	TCtrlInfo* pCtrlInfo = &s_InputList[nInId-1];

	return LL_GPIO_IsInputPinSet(pCtrlInfo->gpio, pCtrlInfo->pin);
}
