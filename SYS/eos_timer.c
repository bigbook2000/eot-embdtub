/*
 * eos_timer.c
 *
 *  Created on: Jun 30, 2023
 *      Author: hjx
 */


#include "eos_timer.h"

#include <stdio.h>

EOTTimer* EOS_Timer_Create()
{
	// 创建一个头
	EOTTimer* pTimer = malloc(sizeof(EOTTimer));
	pTimer->id = 0;
	pTimer->delay = 0L;
	pTimer->tick = 0L;
	pTimer->index = 0;
	pTimer->loop = 0;

	pTimer->tag = NULL;

	return pTimer;
}

void EOS_Timer_Init(EOTTimer* tpTimer, int nId, uint64_t nDelay, int nLoop, void* pTag, EOFuncTimer fCallback)
{
	tpTimer->id = nId;
	tpTimer->delay = nDelay;
	tpTimer->tick = 0L;
	tpTimer->index = -1;
	tpTimer->loop = nLoop;

	tpTimer->tag = pTag;
	tpTimer->callback = fCallback;

	if (tpTimer->loop <= 0) tpTimer->loop = TIMER_LOOP_INFINITE;

	// 不启动
	tpTimer->flag = TIMER_STOP;
}

void EOS_Timer_Start(EOTTimer* tpTimer)
{
	tpTimer->flag = TIMER_RUN;
}

void EOS_Timer_Stop(EOTTimer* tpTimer)
{
	printf("Timer Stop: %d\r\n", tpTimer->id);
	tpTimer->flag = TIMER_STOP;
}

void EOS_Timer_Update(EOTTimer* tpTimer, uint64_t tick)
{
	// 计时器停止
	if (tpTimer->flag == TIMER_STOP) return;

	if (tpTimer->index < 0)
	{
		// 第一次初始化
		tpTimer->tick = tick;
		tpTimer->index++;
		return;
	}

	if (tick < (tpTimer->tick + tpTimer->delay)) return;

	tpTimer->tick = tick;

	// 前面已经初始化成0了，所以第一次调用时index=1
	tpTimer->index++;

	if (tpTimer->index >= tpTimer->loop)
	{
		// 停止
		printf("Timer End: %d\r\n", tpTimer->id);
		tpTimer->flag = TIMER_STOP;
	}

	//printf("[%d] %d / %d / %d\r\n", tpTimer->flag, (int)tpTimer->tick, tpTimer->index, tpTimer->loop);

	// 回调，最后调用，可以重复使用Timer
	if (tpTimer->callback != NULL)
	{
		tpTimer->callback(tpTimer);
	}
}


/**
 * 延时启动
 * @param tpTimer 时钟
 * @param nDelay
 * @param nLoop
 * @param pTag
 */
void EOS_Timer_StartDelay(EOTTimer* tpTimer, uint64_t nDelay, int nLoop, void* pTag)
{
	tpTimer->delay = nDelay;
	tpTimer->tick = 0L;
	tpTimer->index = -1;
	tpTimer->loop = nLoop;

	tpTimer->tag = pTag;

	if (tpTimer->loop <= 0) tpTimer->loop = TIMER_LOOP_INFINITE;

	// 启动
	tpTimer->flag = TIMER_RUN;
}

/**
 * 延时调用（一次）
 * @param tpTimer 时钟
 * @param nDelay
 * @param pTag
 * @param fCallback 调用函数
 */
void EOS_Timer_StartCall(EOTTimer* tpTimer, uint64_t nDelay, void* pTag, EOFuncTimer fCallback)
{
	tpTimer->delay = nDelay;
	tpTimer->tick = 0L;
	tpTimer->index = -1;
	tpTimer->loop = TIMER_LOOP_ONCE;

	tpTimer->tag = pTag;
	tpTimer->callback = fCallback;

	// 启动
	tpTimer->flag = TIMER_RUN;
}

uint8_t EOS_Timer_IsFirst(EOTTimer* tpTimer)
{
	return (tpTimer->index == 1);
}
uint8_t EOS_Timer_IsLast(EOTTimer* tpTimer)
{
	return (tpTimer->index >= tpTimer->loop);
}
