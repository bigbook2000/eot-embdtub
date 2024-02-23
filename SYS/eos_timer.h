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

// 应用ID编号从100起
#define TIMER_ID_APP 			1000

//
// 更为安全的单线程简单计时器
//
typedef struct _stEOTTimer
{
	// 编号
	int id;
	// 计时时刻
	uint64_t tick;
	// 时间间隔，毫秒
	uint64_t delay;
	// 当前索引
	int index;
	// 循环次数，0无限
	int loop;
	// 标记数据
	void* tag;

	// 内部运行状态
	int flag;
	EOFuncTimer callback;
}
EOTTimer;

/**
 * 初始化定时器
 * @param tpTimer 定时器对象
 * @param nId 内部编号
 */
void EOS_Timer_Init(EOTTimer* tpTimer, int nId, uint64_t nDelay, int nLoop, void* pTag, EOFuncTimer fCallback);

/**
 * 启动定时器
 */
void EOS_Timer_Start(EOTTimer* tpTimer);
/**
 * 停止计时器
 */
void EOS_Timer_Stop(EOTTimer* tpTimer);
/**
 * 更新定时器
 */
void EOS_Timer_Update(EOTTimer* tpTimer, uint64_t tick);

void EOS_Timer_StartDelay(EOTTimer* tpTimer, uint64_t nDelay, int nLoop, void* pTag);
void EOS_Timer_StartCall(EOTTimer* tpTimer, uint64_t nDelay, void* pTag, EOFuncTimer fCallback);

uint8_t EOS_Timer_IsFirst(EOTTimer* tpTimer);
uint8_t EOS_Timer_IsLast(EOTTimer* tpTimer);

#endif /* EOS_TIMER_H_ */
