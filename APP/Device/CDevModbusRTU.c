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
#include "AppSetting.h"
#include "CSensor.h"

/**
 * ת��ʵ���յ������ݴ�device��sensor
 */
static double DoSensorDataRt(TSensor* pSensor, void* pTag, uint8_t* pData, int nLength)
{
	TDevInfo* pDevInfo = (TDevInfo*)pTag;

	int retain;
	uint8_t* p;
	TRegisterData* pRegRt;

	pRegRt = &(pSensor->data_rt);

	retain = pDevInfo->data_count - pSensor->offset;
	if (retain < pSensor->length)
	{
		_T("*** ���������ݵ�ַ��� [%d:%d] %d",
				pSensor->offset, pSensor->length, pDevInfo->data_count);
		return 0.0;
	}

	////////////////////////////////////////////////////////////////
	//
	// ������Խ�����ֵת������Modbus������ת����ʵ����Ҫ������
	//

	// �������豸�Ĵ���
	p = &(pDevInfo->data[pSensor->offset]);
	//_T("Address = %d, Length = %d", pSensor->offset, pSensor->length);
	uint16_t v = 0;
	DATA_TYPE_SET(&v, pSensor->data_type, p);
	if (pSensor->endian == DATA_TYPE_LITTLE) v = ENDIAN_CHANGE_16(v);

	double d = v;

	//
	//
	////////////////////////////////////////////////////////////////

	// ���Ƶ���Ĵ������ڲ�ʼ��Ϊdouble
	DATA_TYPE_SET(&(pRegRt->data), DATA_TYPE_DOUBLE, &d);

	_T("������ʵʱ���� %s = (%f, %d) %f",
			pSensor->name, d, v, pRegRt->data);

	return d;
}

void DeviceModbusRTU_Start(TDevInfo* pDevInfo, uint64_t tick, EOTDate* pDate)
{

}
void DeviceModbusRTU_End(TDevInfo* pDevInfo, uint64_t tick, EOTDate* pDate)
{

}
void DeviceModbusRTU_Update(uint8_t nActiveId, TDevInfo* pDevInfo, uint64_t tick, EOTDate* pDate)
{
	F_UART_DMA_Send(pDevInfo);

	// ���յ�����
	EOTBuffer* pRecvBuffer = F_UART_DMA_Buffer(pDevInfo);
	if (pRecvBuffer->length <= 0) return;

	// Debug
	_Tmb(pRecvBuffer->buffer, pRecvBuffer->length);

	int len = F_CheckModbusRTU(pDevInfo, pRecvBuffer->buffer, pRecvBuffer->length);
	if (len <= 0) return;

	//
	// ���豸�ɼ������ݶ�Ӧ��sensor��
	//
	F_Sensor_DataSet(pDevInfo->id, (EOFuncSensorData)DoSensorDataRt, pDevInfo, NULL, 0);

	EOS_Buffer_Pop(pRecvBuffer, NULL, len);
	_T("RecvBuffer: %d/%d", len, pRecvBuffer->length);
}

void F_Device_ModbusRTU_Init(TDevInfo* pDevInfo, char* sType, char* sParams)
{
	if (strcmp(sType, "ModbusRTU") != 0) return;

	pDevInfo->device = DEVICE_MODBUSRTU;

	F_SetModbusRTU(pDevInfo, sParams);

	// ע���¼�
	pDevInfo->start_cb = (FuncDeviceStart)DeviceModbusRTU_Start;
	pDevInfo->end_cb = (FuncDeviceEnd)DeviceModbusRTU_End;
	pDevInfo->update_cb = (FuncDeviceUpdate)DeviceModbusRTU_Update;
}

