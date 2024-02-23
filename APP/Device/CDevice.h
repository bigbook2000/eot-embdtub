/*
 * CDevModbusRTU.h
 *
 *  Created on: Jan 27, 2024
 *      Author: hjx
 *
 * 每个设备有自己独立的时间片，一共4个函数
 * 初始化Init
 * 时间片开始时调用Start，结束时调用End
 * 中间调用Update
 *
 * 请在 F_Device_Type函数中 初始化设备
 *
 */

#ifndef CDEVMODBUSRTU_H_
#define CDEVMODBUSRTU_H_

#include <stdint.h>

#include <CSensor.h>

#include "eos_buffer.h"
#include "eob_date.h"

// 设备类型
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

// 设备名称
#define DEVICE_NONE				0
#define DEVICE_SYS_SWITCH		1
#define DEVICE_SYS_ADC			2
#define DEVICE_MODBUSRTU		10
#define DEVICE_TEMPHUMI			101

// 每个设备自身的寄存器数据最大长度
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

	// 索引
	uint8_t id;

	// 设备类型
	uint8_t device;

	// 端口
	uint8_t type;

	// 状态
	uint8_t status;

	// 更新标识，如果是modbus，分片执行，避免冲突
	uint8_t update_flag;

	uint8_t r3;

	// 参数长度
	uint8_t param_count;
	// 数据长度
	uint8_t data_count;

	// 数据
	uint8_t data[DEV_DATA_MAX];
	// 参数
	uint8_t* param;
}
TDevInfo;



// IO

#define CTRL_SET 		0x1 // 高电平
#define CTRL_RESET 		0x0 // 低电平
#define CTRL_NONE 		0xFF // 未知
typedef void (*FuncCtrlChange)(TDevInfo* pDevInfo, uint8_t nCtrlSet);

void F_Device_SysSwitch_Init(TDevInfo* pDevInfo, char* sType, char* sParams);
void F_SysSwitch_ChangeEvent_Add(FuncCtrlChange tCallbackChange);
void F_SysSwitch_Output_Set(uint8_t nInId, uint8_t nSet);

// ADC

void F_Device_SysADC_Init(TDevInfo* pDevInfo, char* sType, char* sParams);

// UART Modbus

// 设备是否正在处理数据
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
