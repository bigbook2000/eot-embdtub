/*
 * eos_timer.h
 *
 *  Created on: Jun 30, 2023
 *      Author: hjx
 */

#ifndef EOS_TIMER_H_
#define EOS_TIMER_H_

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

typedef struct _stEOTTimer EOTTimer;
typedef void (*EOFuncTimer)(EOTTimer *timer);

#define TIMER_LOOP_INFINITE 	0x7FFFFFFF
#define TIMER_LOOP_ONCE 		0x1

#define TIMER_RUN 				0x1
#define TIMER_STOP 				0x0

// Ӧ��ID��Ŵ�100��
#define TIMER_ID_APP 			1000

//
// ��Ϊ��ȫ�ĵ��̼߳򵥼�ʱ��
//
typedef struct _stEOTTimer
{
	// ���
	int id;
	// ��ʱʱ��
	uint64_t tick;
	// ʱ����������
	uint64_t delay;
	// ��ǰ����
	int index;
	// ѭ��������0����
	int loop;
	// �������
	void* tag;

	// �ڲ�����״̬
	int flag;
	EOFuncTimer callback;
}
EOTTimer;

/**
 * ��ʼ����ʱ��
 * @param tpTimer ��ʱ������
 * @param nId �ڲ����
 */
void EOS_Timer_Init(EOTTimer* tpTimer, int nId, uint64_t nDelay, int nLoop, void* pTag, EOFuncTimer fCallback);

/**
 * ������ʱ��
 */
void EOS_Timer_Start(EOTTimer* tpTimer);
/**
 * ֹͣ��ʱ��
 */
void EOS_Timer_Stop(EOTTimer* tpTimer);
/**
 * ���¶�ʱ��
 */
void EOS_Timer_Update(EOTTimer* tpTimer, uint64_t tick);

void EOS_Timer_StartDelay(EOTTimer* tpTimer, uint64_t nDelay, int nLoop, void* pTag);
void EOS_Timer_StartCall(EOTTimer* tpTimer, uint64_t nDelay, void* pTag, EOFuncTimer fCallback);

uint8_t EOS_Timer_IsFirst(EOTTimer* tpTimer);
uint8_t EOS_Timer_IsLast(EOTTimer* tpTimer);

#endif /* EOS_TIMER_H_ */
