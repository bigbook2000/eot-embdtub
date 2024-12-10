/*
 * eob_debug.h
 *
 *  Created on: Jun 30, 2023
 *      Author: hjx
 *
 * ��֧�ֶ������ӡ������Ҫ������FreeRTOS����ʹ�� _DEBUG_TASK_ Ԥ�����
 *
 */

#ifndef EOB_DEBUG_H_
#define EOB_DEBUG_H_

#include <stdint.h>
#include <stdio.h>

#include "eos_buffer.h"

//
// �Ƿ�֧�ֶ�����
//
// �� _DEBUG_TASK_ Ԥ���� ��������FreeRTOS���������
//
// #define _DEBUG_TASK_

// ����DEBUG_LIMIT���ֶ��������
#define DEBUG_LIMIT		1000

// ���ڵ��Դ�ӡ
void EOB_Debug_Init(void);
// IAP��תʱ�����ͷ�
void EOB_Debug_DeInit(void);
// ��������
EOTBuffer* EOB_Debug_InputData(void);
// DMAģʽ
void EOB_Debug_Update(void);

void EOB_Debug_Print(char* sInfo, ...);
void EOB_Debug_PrintLine(char* sInfo, ...);
void EOB_Debug_PrintTime(void);
void EOB_Debug_PrintBin(void* pData, int nLength);

// ���
#define _P(...) EOB_Debug_Print(__VA_ARGS__)
// ��ʽ�������������
#define _T(...) EOB_Debug_PrintLine(__VA_ARGS__)
// ����
#define _D(...) EOB_Debug_PrintTime();EOB_Debug_Print("[%s:%d]", __FILE__, __LINE__);EOB_Debug_Print(__VA_ARGS__);EOB_Debug_Print("\n")

// ���ʱ��
#define _Tt() EOB_Debug_PrintTime()
// ���������
#define _Tmb(p, n) EOB_Debug_PrintBin(p, n)

// ԭʼ��ӡ
#define _Pf(...) printf(__VA_ARGS__)
// �������
#define _Pn() printf("\n")

#endif /* EOB_DEBUG_H_ */
