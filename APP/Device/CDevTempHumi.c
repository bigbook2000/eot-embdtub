/*
 * CDevTempHumi.c
 *
 *  Created on: Jan 30, 2024
 *      Author: hjx
 */


#include "CDevice.h"

#include <string.h>
#include <stdlib.h>

#include "cmsis_os.h"
#include "crc.h"

#include "eos_inc.h"
#include "eob_debug.h"
#include "eob_tick.h"

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

	// ��ʵ�����ݸ��Ƶ���Ĵ�����
	// ���������ֲᣬ������Ϊ�Ŵ�10����������������1λС��
	// ������10000ʱ��Ϊ����

	double d = v / 10.0;
	if (v > 10000) d = (10000 - v) / 10;

	//
	//
	////////////////////////////////////////////////////////////////

	// ���Ƶ���Ĵ������ڲ�ʼ��Ϊdouble
	DATA_TYPE_SET(&(pRegRt->data), DATA_TYPE_DOUBLE, &d);

	_T("������ʵʱ���� %s = (%f, %d) %f",
			pSensor->name, d, v, pRegRt->data);

	return d;
}


void DeviceTempHumi_Start(TDevInfo* pDevInfo, uint64_t tick, EOTDate* pDate)
{
	// ����ָ��
	F_UART_DMA_Send(pDevInfo);
}
void DeviceTempHumi_End(TDevInfo* pDevInfo, uint64_t tick, EOTDate* pDate)
{

}

void DeviceTempHumi_Update(uint8_t nActiveId, TDevInfo* pDevInfo, uint64_t tick, EOTDate* pDate)
{
	// �����Լ���ʱ��Ƭ�����Բ�����
	if (nActiveId != pDevInfo->id) return;

	// ���յ�����
	EOTBuffer* pRecvBuffer = F_UART_DMA_Buffer(pDevInfo);
	if (pRecvBuffer->length <= 0) return;

	// Debug
	//_Tmb(pRecvBuffer->buffer, pRecvBuffer->length);

	int nDataLen = F_CheckModbusRTU(pDevInfo, pRecvBuffer->buffer, pRecvBuffer->length);
	if (nDataLen <= 0) return;

	//
	// ���豸�ɼ������ݶ�Ӧ��sensor��
	//
	F_Sensor_DataSet(pDevInfo->id, (EOFuncSensorData)DoSensorDataRt, pDevInfo, NULL, 0);

	EOS_Buffer_Pop(pRecvBuffer, NULL, nDataLen);
	_T("RecvBuffer: %d/%d", nDataLen, pRecvBuffer->length);
}

void F_Device_TempHumi_Init(TDevInfo* pDevInfo, char* sType, char* sParams)
{
	if (strcmp(sType, "TempHumi") != 0) return;

	pDevInfo->device = DEVICE_TEMPHUMI;

	F_SetModbusRTU(pDevInfo, sParams);

	// ע���¼�
	pDevInfo->start_cb = (FuncDeviceStart)DeviceTempHumi_Start;
	pDevInfo->end_cb = (FuncDeviceEnd)DeviceTempHumi_End;
	pDevInfo->update_cb = (FuncDeviceUpdate)DeviceTempHumi_Update;
}



