/*
 * eob_tick.h
 *
 *  Created on: May 9, 2023
 *      Author: hjx
 *
 * 使用STM32 TIM7进行标准计时操作
 *
 */

#ifndef EOB_TICK_H_
#define EOB_TICK_H_

#include <stdint.h>

// 使用一个系统TIM计数，毫秒

uint64_t EOB_Tick_Get(void);
uint8_t EOB_Tick_Check(uint64_t* last, uint64_t delay);

#endif /* EOB_TICK_H_ */
