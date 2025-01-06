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

#include "eos_inc.h"
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

#endif /* EOB_DEBUG_H_ */
