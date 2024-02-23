/*
 * eob_tick.c
 *
 *  Created on: May 9, 2023
 *      Author: hjx
 *
 * 使用基本时钟TIM7作为全局计时器
 * 精度为毫秒，软件使用已足够
 *
 * TIM_InitStruct.Prescaler = 9;
 * TIM_InitStruct.CounterMode = LL_TIM_COUNTERMODE_UP;
 * TIM_InitStruct.Autoreload = 8399;
 * LL_TIM_Init(TIM7, &TIM_InitStruct);
 * LL_TIM_EnableARRPreload(TIM7);
 * LL_TIM_SetTriggerOutput(TIM7, LL_TIM_TRGO_UPDATE);
 * LL_TIM_DisableMasterSlaveMode(TIM7);
 *
 */

#include "stm32f4xx_ll_tim.h"

static uint64_t s_Global_Tick = 0L;

/**
 * 请手动添加中断
 * NVIC_SetPriority(TIM7_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),5, 0));
 * NVIC_EnableIRQ(TIM7_IRQn);
 *
 * LL_TIM_EnableIT_UPDATE(TIM7);
 * LL_TIM_EnableCounter(TIM7);
 *
 */
void TIM7_IRQHandler(void)
{
	if (LL_TIM_IsActiveFlag_UPDATE(TIM7) == SET)
	{
		++s_Global_Tick;
		LL_TIM_ClearFlag_UPDATE(TIM7);
	}
}

uint64_t EOB_Tick_Get(void)
{
	return s_Global_Tick;
}

// 检测一个时刻
uint8_t EOB_Tick_Check(uint64_t* last, uint64_t delay)
{
	if (s_Global_Tick < ((*last) + delay)) return 0;

	(*last) = s_Global_Tick;
	return 1;
}
