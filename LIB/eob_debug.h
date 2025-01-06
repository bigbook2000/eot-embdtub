/*
 * eob_debug.h
 *
 *  Created on: Jun 30, 2023
 *      Author: hjx
 *
 * 可支持多任务打印，但需要依赖与FreeRTOS，请使用 _DEBUG_TASK_ 预定义宏
 *
 */

#ifndef EOB_DEBUG_H_
#define EOB_DEBUG_H_

#include <stdint.h>
#include <stdio.h>

#include "eos_inc.h"
#include "eos_buffer.h"

//
// 是否支持多任务
//
// 打开 _DEBUG_TASK_ 预定义 则依赖于FreeRTOS的任务管理
//
// #define _DEBUG_TASK_

// 超过DEBUG_LIMIT需手动清除缓存
#define DEBUG_LIMIT		1000

// 用于调试打印
void EOB_Debug_Init(void);
// IAP跳转时必须释放
void EOB_Debug_DeInit(void);
// 输入数据
EOTBuffer* EOB_Debug_InputData(void);
// DMA模式
void EOB_Debug_Update(void);

#endif /* EOB_DEBUG_H_ */
