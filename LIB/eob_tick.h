/*
 * eob_tick.h
 *
 *  Created on: May 9, 2023
 *      Author: hjx
 *
 * ʹ��STM32 TIM7���б�׼��ʱ����
 *
 */

#ifndef EOB_TICK_H_
#define EOB_TICK_H_

#include <stdint.h>

// ʹ��һ��ϵͳTIM����������

uint64_t EOB_Tick_Get(void);
uint8_t EOB_Tick_Check(uint64_t* last, uint64_t delay);

#endif /* EOB_TICK_H_ */
