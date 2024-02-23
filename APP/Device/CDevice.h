/*
 * CDevModbusRTU.h
 *
 *  Created on: Jan 27, 2024
 *      Author: hjx
 *
 * ÿ���豸���Լ�������ʱ��Ƭ��һ��4������
 * ��ʼ��Init
 * ʱ��Ƭ��ʼʱ����Start������ʱ����End
 * �м����Update
 *
 * ���� F_Device_Type������ ��ʼ���豸
 *
 */

#ifndef CDEVMODBUSRTU_H_
#define CDEVMODBUSRTU_H_

#include <stdint.h>

#include <CSensor.h>

#include "eos_buffer.h"
#include "eob_date.h"

// �豸����
#define DEVICE_TYPE_UART2		2
#define DEVICE_TYPE_UART3		3
#define DEVICE_TYPE_UART4		4
#define DEVICE_TYPE_UART5		5

#define DEVICE_TYPE_INPUT1		11
#define DEVICE_TYPE_INPUT2		12
#define DEVICE_TYPE_OUTPUT1		21
#define DEVICE_TYPE_OUTPUT2		22

#define DEVICE_TYPE_ADC1_8		58
#define DEVICE_TYPE_ADC1_9		59

// �豸����
#define DEVICE_NONE				0
#define DEVICE_SYS_SWITCH		1
#define DEVICE_SYS_ADC			2
#define DEVICE_MODBUSRTU		10
#define DEVICE_TEMPHUMI			101

// ÿ���豸����ļĴ���������󳤶�
#define DEV_DATA_MAX			32

typedef struct _stDevInfo TDevInfo;
typedef void (*FuncDeviceStart)(TDevInfo* pDevInfo, uint64_t tick, EOTDate* pDate);
typedef void (*FuncDeviceEnd)(TDevInfo* pDevInfo, uint64_t tick, EOTDate* pDate);
typedef void (*FuncDeviceUpdate)(uint8_t nActiveId, TDevInfo* pDevInfo, uint64_t tick, EOTDate* pDate);

#define UPDATE_FLAG_TIME		1
#define UPDATE_FLAG_NONE		0

typedef struct _stDevInfo
{
	FuncDeviceStart start_cb;
	FuncDeviceEnd end_cb;
	FuncDeviceUpdate update_cb;

	// ����
	uint8_t id;

	// �豸����
	uint8_t device;

	// �˿�
	uint8_t type;

	// ״̬
	uint8_t status;

	// ���±�ʶ�������modbus����Ƭִ�У������ͻ
	uint8_t update_flag;

	uint8_t r3;

	// ��������
	uint8_t param_count;
	// ���ݳ���
	uint8_t data_count;

	// ����
	uint8_t data[DEV_DATA_MAX];
	// ����
	uint8_t* param;
}
TDevInfo;



// IO

#define CTRL_SET 		0x1 // �ߵ�ƽ
#define CTRL_RESET 		0x0 // �͵�ƽ
#define CTRL_NONE 		0xFF // δ֪
typedef void (*FuncCtrlChange)(TDevInfo* pDevInfo, uint8_t nCtrlSet);

void F_Device_SysSwitch_Init(TDevInfo* pDevInfo, char* sType, char* sParams);
void F_SysSwitch_ChangeEvent_Add(FuncCtrlChange tCallbackChange);
void F_SysSwitch_Output_Set(uint8_t nInId, uint8_t nSet);

// ADC

void F_Device_SysADC_Init(TDevInfo* pDevInfo, char* sType, char* sParams);

// UART Modbus

// �豸�Ƿ����ڴ�������
#define DEV_STATUS_WAIT		0
#define DEV_STATUS_BUSY		1

void F_UART_DMA_Init(void);
void F_UART_DMA_Update(void);
void F_UART_DMA_Send(TDevInfo* pDevInfo);
EOTBuffer* F_UART_DMA_Buffer(TDevInfo* pDevInfo);

void F_SetModbusRTU(TDevInfo* pDevInfo, char* sCommand);
int F_CheckModbusRTU(TDevInfo* pDevInfo, uint8_t* pData, int nLength);

void F_Device_ModbusRTU_Init(TDevInfo* pDevInfo, char* sType, char* sParams);
void F_Device_TempHumi_Init(TDevInfo* pDevInfo, char* sType, char* sParams);

#endif /* CDEVMODBUSRTU_H_ */
