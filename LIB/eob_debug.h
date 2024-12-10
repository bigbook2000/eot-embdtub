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

void EOB_Debug_Print(char* sInfo, ...);
void EOB_Debug_PrintLine(char* sInfo, ...);
void EOB_Debug_PrintTime(void);
void EOB_Debug_PrintBin(void* pData, int nLength);

// 输出
#define _P(...) EOB_Debug_Print(__VA_ARGS__)
// 格式化输出，带换行
#define _T(...) EOB_Debug_PrintLine(__VA_ARGS__)
// 调试
#define _D(...) EOB_Debug_PrintTime();EOB_Debug_Print("[%s:%d]", __FILE__, __LINE__);EOB_Debug_Print(__VA_ARGS__);EOB_Debug_Print("\n")

// 输出时间
#define _Tt() EOB_Debug_PrintTime()
// 输出二进制
#define _Tmb(p, n) EOB_Debug_PrintBin(p, n)

// 原始打印
#define _Pf(...) printf(__VA_ARGS__)
// 输出换行
#define _Pn() printf("\n")

#endif /* EOB_DEBUG_H_ */
