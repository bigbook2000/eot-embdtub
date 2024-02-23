/*
 * CDevModbusRTU.c
 *
 *  Created on: Jan 27, 2024
 *      Author: hjx
 */

#include "CDevice.h"

#include <string.h>
#include <stdlib.h>

#include "cmsis_os.h"
#include "crc.h"

#include "stm32f4xx_ll_utils.h"
#include "stm32f4xx_ll_usart.h"
#include "stm32f4xx_ll_dma.h"
#include "stm32f4xx_ll_gpio.h"

#include "eos_inc.h"
#include "eos_timer.h"
#include "eob_dma.h"
#include "eob_debug.h"
#include "eob_tick.h"
#include "eob_uart.h"

#include "Global.h"
#include "Config.h"
#include "CSensor.h"


void DeviceModbusRTU_Start(TDevInfo* pDevInfo, uint64_t tick, EOTDate* pDate)
{

}
void DeviceModbusRTU_End(TDevInfo* pDevInfo, uint64_t tick, EOTDate* pDate)
{

}
void DeviceModbusRTU_Update(uint8_t nActiveId, TDevInfo* pDevInfo, uint64_t tick, EOTDate* pDate)
{
	int i;
	F_UART_DMA_Send(pDevInfo);

	// 接收的数据
	EOTBuffer* pRecvBuffer = F_UART_DMA_Buffer(pDevInfo);
	if (pRecvBuffer->length <= 0) return;

	// Debug
	_Tmb(pRecvBuffer->buffer, pRecvBuffer->length);

	int len = F_CheckModbusRTU(pDevInfo, pRecvBuffer->buffer, pRecvBuffer->length);
	if (len <= 0) return;

	TSensor* pSensor;

	// 处理接收数据
	TSensor* pSensorList = F_Sensor_List();
	int nCount = F_Sensor_Count();
	for (i=0; i<nCount; i++)
	{
		pSensor = &pSensorList[i];
		if (pSensor->device_id != pDevInfo->id) continue;

		// 在此处理代码
	}

	EOS_Buffer_Pop(pRecvBuffer, NULL, len);
	_T("RecvBuffer: %d/%d", len, pRecvBuffer->length);
}

void F_Device_ModbusRTU_Init(TDevInfo* pDevInfo, char* sType, char* sParams)
{
	if (strcmp(sType, "ModbusRTU") != 0) return;

	pDevInfo->device = DEVICE_MODBUSRTU;

	F_SetModbusRTU(pDevInfo, sParams);

	// 注册事件
	pDevInfo->start_cb = (FuncDeviceStart)DeviceModbusRTU_Start;
	pDevInfo->end_cb = (FuncDeviceEnd)DeviceModbusRTU_End;
	pDevInfo->update_cb = (FuncDeviceUpdate)DeviceModbusRTU_Update;
}

