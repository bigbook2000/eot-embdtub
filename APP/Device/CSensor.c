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

// 统计数据时间提前量(秒)
#define SENSOR_TIME_LEAD 	15

// 动态分配
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
		_T("*** 传感器索引溢出: %d/%d", nSensorId, s_SensorCount);
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

	_T("初始化传感器: %s", pSensor->name);

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
	// 设备寄存器参数 设备编号，寄存器地址，数据类型，字节顺序
	// F_ConfigAdd("sensor1.device", "1,00,uint16,little");

	cnt = 8; // 必须输入最大限制
	strcpy(s, sDevice); // 避免破坏性分割
	if (EOG_SplitString(s, -1, ',', (char**)ss, &cnt) != 4)
	{
		_T("*** 设备参数设置错误 %d", cnt);
		return;
	}

	uint32_t dAddr;
	uint8_t dType;
	uint8_t dLen;

	dType = F_DataType(ss[2]);
	dLen = DATA_TYPE_LENGTH(dType);

	pSensor->device_id = atoi(ss[0]) - 1; // 索引
	pSensor->offset = EOG_String2Hex(ss[1], 2); // 地址偏移
	pSensor->data_type = dType;	// 类型
	pSensor->length = dLen; // 长度

	if (strcmp(ss[2], "big") == 0)
		pSensor->endian = DATA_TYPE_BIG;
	else
		pSensor->endian = DATA_TYPE_LITTLE;

	// 大寄存器中的参数
	// F_ConfigAdd("sensor1.data", "0,float");
	cnt = 8; // 必须输入最大限制
	strcpy(s, sData); // 避免破坏性分割
	if (EOG_SplitString(s, -1, ',', (char**)ss, &cnt) != 2)
	{
		_T("*** Data 参数设置错误 %d", cnt);
		return;
	}

	dAddr = atoi(ss[0]);
	dType = F_DataType(ss[1]);
	dLen = DATA_TYPE_LENGTH(dType);

	pSensor->data_rt.address = dAddr;
	pSensor->data_rt.length = dLen;
	pSensor->data_rt.type = dLen;

	// 如果无则输入0
	// F_ConfigAdd("sensor1.tick", "1,10,60,1440");
	cnt = 8; // 必须输入最大限制
	strcpy(s, sTick); // 避免破坏性分割
	if (EOG_SplitString(s, -1, ',', (char**)ss, &cnt) != DATA_COUNT_MAX)
	{
		_T("*** Tick 参数设置错误 %d", cnt);
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
 * 重置统计数据
 * 按tick配置分钟间隔，以每天0点为基准点计算
 */
static void Sensor_DataReset(TSensor* pSensor, int nMinute)
{
	TRegisterData* pRegHis;

	int dp;
	int i;
	for (i=0; i<DATA_COUNT_MAX; i++)
	{
		pRegHis = &pSensor->data_h[i];

		// 0表示不统计
		if (pRegHis->tick_delay <= 0) continue;
		// 没数据aa
		if (pRegHis->count <= 0) continue;

		dp = nMinute / pRegHis->tick_delay;
		if (pRegHis->tick_last != dp)
		{
			// 计算上一周期的数据
			pRegHis->data = pRegHis->sum / pRegHis->count;
			pRegHis->tick_last = dp;

			// 重置
			_T("传感器统计重置 [%d] [%d: %d/%d] = (%d) %f, %f (%f, %f)",
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
	// 为了防止数据调用失败，设定一定的提前量
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
 * 计算统计数据
 */
void F_Sensor_DataCount(TSensor* pSensor, double d)
{
	TRegisterData* pRegHis;

	int i;
	for (i=0; i<DATA_COUNT_MAX; i++)
	{
		pRegHis = &pSensor->data_h[i];

		// 0表示不统计
		if (pRegHis->tick_delay <= 0) continue;

		pRegHis->sum += d;
		pRegHis->count++;

		if (pRegHis->min > d) pRegHis->min = d;
		if (pRegHis->max < d) pRegHis->max = d;
	}
}


/**
 * 判断所有统计数据是否已经存在
 * @param nDataTick 以分钟计数，0点为基点的累计数
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
			_T("传感器数据未统计 %s: %d, %d",
					pSensor->name, nDataDelay, nDataTick);
			return EO_FALSE;
		}
	}

	return EO_TRUE;
}

/**
 * 将设备采集的数据对应到sensor上
 * 一个设备对应多个Sensor
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

		// 计算统计值
		F_Sensor_DataCount(pSensor, d);
	}
}
