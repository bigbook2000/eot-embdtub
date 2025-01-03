/*
 * CSensor.c
 *
 *  Created on: Jan 20, 2024
 *      Author: hjx
 */

#include "CSensor.h"

#include <string.h>
#include <stdlib.h>

#include "cmsis_os.h"

#include "eos_inc.h"
#include "eob_debug.h"
#include "eob_date.h"

#include "Global.h"
#include "AppSetting.h"

// ͳ������ʱ����ǰ��(��)
#define SENSOR_TIME_LEAD 	15

// ��̬����
static TSensor* s_SensorList = NULL;
static uint8_t s_SensorCount = 0;

TSensor* F_Sensor_List(void)
{
	return s_SensorList;
}
uint8_t F_Sensor_Count(void)
{
	return s_SensorCount;
}
TSensor* F_Sensor_Get(uint8_t nSensorId)
{
	if (nSensorId > s_SensorCount)
	{
		_T("*** �������������: %d/%d", nSensorId, s_SensorCount);
		return NULL;
	}
	return &s_SensorList[nSensorId];
}

TSensor* F_Sensor_Init(uint8_t nCount)
{
	int nSize = sizeof(TSensor) * nCount;
	s_SensorList = (TSensor*)pvPortMalloc(nSize);
	if (s_SensorList == NULL)
	{
		_D("*** xAlloc NULL %d", nSize);
		return NULL;
	}

	s_SensorCount = nCount;

	return s_SensorList;
}


void F_Sensor_Type(TSensor* pSensor, char* sName, char* sDevice, char* sTick, char* sData)
{
	char s[VALUE_SIZE];

	int i;
	strncpy(pSensor->name, sName, SENSOR_NAME_SIZE);
	pSensor->name[SENSOR_NAME_SIZE-1] = '\0';

	_T("��ʼ��������: %s", pSensor->name);

	pSensor->device_id = 0;

	pSensor->length = 0;
	pSensor->offset = 0;
	pSensor->data_type = DATA_TYPE_NONE;
	pSensor->endian = DATA_TYPE_LITTLE;

	memset(&pSensor->data_rt, 0, sizeof(TRegisterData));
	for (i=0; i<DATA_COUNT_MAX; i++)
	{
		memset(&pSensor->data_h[i], 0, sizeof(TRegisterData));
		pSensor->data_h[i].id = i;
		pSensor->data_h[i].min = VALUE_MAX;
		pSensor->data_h[i].max = VALUE_MIN;
	}

	char* ss[8];
	uint8_t cnt;
	// �豸�Ĵ������� �豸��ţ��Ĵ�����ַ���������ͣ��ֽ�˳��
	// F_ConfigAdd("sensor1.device", "1,00,uint16,little");

	cnt = 8; // ���������������
	strcpy(s, sDevice); // �����ƻ��Էָ�
	if (EOG_SplitString(s, -1, ',', (char**)ss, &cnt) != 4)
	{
		_T("*** �豸�������ô��� %d", cnt);
		return;
	}

	uint32_t dAddr;
	uint8_t dType;
	uint8_t dLen;

	dType = F_DataType(ss[2]);
	dLen = DATA_TYPE_LENGTH(dType);

	pSensor->device_id = atoi(ss[0]) - 1; // ����
	pSensor->offset = EOG_String2Hex(ss[1], 2); // ��ַƫ��
	pSensor->data_type = dType;	// ����
	pSensor->length = dLen; // ����

	if (strcmp(ss[2], "big") == 0)
		pSensor->endian = DATA_TYPE_BIG;
	else
		pSensor->endian = DATA_TYPE_LITTLE;

	// ��Ĵ����еĲ���
	// F_ConfigAdd("sensor1.data", "0,float");
	cnt = 8; // ���������������
	strcpy(s, sData); // �����ƻ��Էָ�
	if (EOG_SplitString(s, -1, ',', (char**)ss, &cnt) != 2)
	{
		_T("*** Data �������ô��� %d", cnt);
		return;
	}

	dAddr = atoi(ss[0]);
	dType = F_DataType(ss[1]);
	dLen = DATA_TYPE_LENGTH(dType);

	pSensor->data_rt.address = dAddr;
	pSensor->data_rt.length = dLen;
	pSensor->data_rt.type = dLen;

	// �����������0
	// F_ConfigAdd("sensor1.tick", "1,10,60,1440");
	cnt = 8; // ���������������
	strcpy(s, sTick); // �����ƻ��Էָ�
	if (EOG_SplitString(s, -1, ',', (char**)ss, &cnt) != DATA_COUNT_MAX)
	{
		_T("*** Tick �������ô��� %d", cnt);
		return;
	}
	for (i=0; i<DATA_COUNT_MAX; i++)
	{
		pSensor->data_h[i].address = dAddr;
		pSensor->data_h[i].length = dLen;
		pSensor->data_h[i].type = dLen;
		pSensor->data_h[i].tick_delay = atoi(ss[i]);
	}
}


/**
 * ����ͳ������
 * ��tick���÷��Ӽ������ÿ��0��Ϊ��׼�����
 */
static void Sensor_DataReset(TSensor* pSensor, int nMinute)
{
	TRegisterData* pRegHis;

	int dp;
	int i;
	for (i=0; i<DATA_COUNT_MAX; i++)
	{
		pRegHis = &pSensor->data_h[i];

		// 0��ʾ��ͳ��
		if (pRegHis->tick_delay <= 0) continue;
		// û����aa
		if (pRegHis->count <= 0) continue;

		dp = nMinute / pRegHis->tick_delay;
		if (pRegHis->tick_last != dp)
		{
			// ������һ���ڵ�����
			pRegHis->data = pRegHis->sum / pRegHis->count;
			pRegHis->tick_last = dp;

			// ����
			_T("������ͳ������ [%d] [%d: %d/%d] = (%d) %f, %f (%f, %f)",
					i, nMinute, dp, pRegHis->tick_last,
					pRegHis->count, pRegHis->data,
					pRegHis->sum, pRegHis->min, pRegHis->max);

			pRegHis->sum = 0.0;
			pRegHis->count = 0;
			pRegHis->min = VALUE_MAX;
			pRegHis->max = VALUE_MIN;
		}
	}
}

void F_Sensor_Update(uint64_t tick, EOTDate* pDate)
{
	// Ϊ�˷�ֹ���ݵ���ʧ�ܣ��趨һ������ǰ��
	int dm = (pDate->hour * 3600 + pDate->minute * 60 + pDate->second + SENSOR_TIME_LEAD) / 60;

	TSensor* pSensor;
	int i;
	for (i=0; i<s_SensorCount; i++)
	{
		pSensor = &s_SensorList[i];
		Sensor_DataReset(pSensor, dm);
	}
}

/**
 * ����ͳ������
 */
void F_Sensor_DataCount(TSensor* pSensor, double d)
{
	TRegisterData* pRegHis;

	int i;
	for (i=0; i<DATA_COUNT_MAX; i++)
	{
		pRegHis = &pSensor->data_h[i];

		// 0��ʾ��ͳ��
		if (pRegHis->tick_delay <= 0) continue;

		pRegHis->sum += d;
		pRegHis->count++;

		if (pRegHis->min > d) pRegHis->min = d;
		if (pRegHis->max < d) pRegHis->max = d;
	}
}


/**
 * �ж�����ͳ�������Ƿ��Ѿ�����
 * @param nDataTick �Է��Ӽ�����0��Ϊ������ۼ���
 */
uint8_t F_Sensor_DataReady(int nDataDelay, int nDataTick)
{
	int i, j;
	uint8_t b;

	TSensor* pSensor;
	TRegisterData* pRegHis;
	for (i=0; i<s_SensorCount; i++)
	{
		pSensor = &s_SensorList[i];

		b = EO_FALSE;
		for (j=0; j<DATA_COUNT_MAX; j++)
		{
			pRegHis = &pSensor->data_h[j];
			if (pRegHis->tick_delay != nDataDelay) continue;

			if (pRegHis->tick_last == nDataTick)
			{
				b = EO_TRUE;
				break;
			}
		}

		if (!b)
		{
			_T("����������δͳ�� %s: %d, %d",
					pSensor->name, nDataDelay, nDataTick);
			return EO_FALSE;
		}
	}

	return EO_TRUE;
}

/**
 * ���豸�ɼ������ݶ�Ӧ��sensor��
 * һ���豸��Ӧ���Sensor
 */
void F_Sensor_DataSet(uint8_t nDeviceId, EOFuncSensorData tCallbackData,
		void* pTag, uint8_t* pData, int nLength)
{
	int i;

	double d;
	TSensor* pSensor;
	for (i=0; i<s_SensorCount; i++)
	{
		pSensor = &s_SensorList[i];
		if (pSensor->device_id != nDeviceId) continue;

		d = tCallbackData(pSensor, pTag, pData, nLength);

		// ����ͳ��ֵ
		F_Sensor_DataCount(pSensor, d);
	}
}
