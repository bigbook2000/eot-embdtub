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
	// ����һ��ͷ
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

	// ������
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
	// ��ʱ��ֹͣ
	if (tpTimer->flag == TIMER_STOP) return;

	if (tpTimer->index < 0)
	{
		// ��һ�γ�ʼ��
		tpTimer->tick = tick;
		tpTimer->index++;
		return;
	}

	if (tick < (tpTimer->tick + tpTimer->delay)) return;

	tpTimer->tick = tick;

	// ǰ���Ѿ���ʼ����0�ˣ����Ե�һ�ε���ʱindex=1
	tpTimer->index++;

	if (tpTimer->index >= tpTimer->loop)
	{
		// ֹͣ
		printf("Timer End: %d\r\n", tpTimer->id);
		tpTimer->flag = TIMER_STOP;
	}

	//printf("[%d] %d / %d / %d\r\n", tpTimer->flag, (int)tpTimer->tick, tpTimer->index, tpTimer->loop);

	// �ص��������ã������ظ�ʹ��Timer
	if (tpTimer->callback != NULL)
	{
		tpTimer->callback(tpTimer);
	}
}


/**
 * ��ʱ����
 * @param tpTimer ʱ��
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

	// ����
	tpTimer->flag = TIMER_RUN;
}

/**
 * ��ʱ���ã�һ�Σ�
 * @param tpTimer ʱ��
 * @param nDelay
 * @param pTag
 * @param fCallback ���ú���
 */
void EOS_Timer_StartCall(EOTTimer* tpTimer, uint64_t nDelay, void* pTag, EOFuncTimer fCallback)
{
	tpTimer->delay = nDelay;
	tpTimer->tick = 0L;
	tpTimer->index = -1;
	tpTimer->loop = TIMER_LOOP_ONCE;

	tpTimer->tag = pTag;
	tpTimer->callback = fCallback;

	// ����
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
