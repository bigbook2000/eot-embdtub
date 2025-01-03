/*
 * CSensor.h
 *
 *  Created on: Jan 20, 2024
 *      Author: hjx
 */

#ifndef CSENSOR_H_
#define CSENSOR_H_

#include <stdint.h>

#include "eob_date.h"

#include "Global.h"

#define MAX_SENSOR 			128

#define SENSOR_NAME_SIZE 	16
#define DATA_COUNT_MAX		4

/**
 * ���ݼĴ���
 */
typedef struct _stRegisterData
{
	// ����
	uint8_t id;

	// ��ַ
	uint8_t address;
	// ���ݳ���
	uint8_t length;
	// ��������
	uint8_t type;

	uint32_t count;
	// ����
	double data;
	double min;
	double max;
	double sum;

	int tick_delay;
	int tick_last;
}
TRegisterData;


typedef struct _stSensor
{
	// ����
	uint8_t id;

	uint8_t device_id;

	// �豸��������
	uint8_t data_type;
	// �豸���ݵ�ַ
	uint16_t offset;
	// �豸���ݳ���
	uint8_t length;
	// �ֽ�˳��
	uint8_t endian;

	// ͳ����������
	uint8_t data_count;

	// ʵʱ����
	TRegisterData data_rt;
	// ͳ������
	TRegisterData data_h[DATA_COUNT_MAX];

	char name[SENSOR_NAME_SIZE];
}
TSensor;

// һ���豸��Ӧ���Sensor
// ת������
typedef double (*EOFuncSensorData)(TSensor* pSensor, void* pTag, uint8_t* pData, int nLength);
// ���豸�ɼ������ݶ�Ӧ��sensor��
void F_Sensor_DataSet(uint8_t nDeviceId, EOFuncSensorData tCallbackData, void* pTag, uint8_t* pData, int nLength);

void F_Sensor_Type(TSensor* pSensor, char* sName, char* sDevice, char* sTick, char* sData);

TSensor* F_Sensor_Init(uint8_t nCount);
void F_Sensor_Update(uint64_t tick, EOTDate* pDate);
TSensor* F_Sensor_List(void);
uint8_t F_Sensor_Count(void);
TSensor* F_Sensor_Get(uint8_t nSensorId);

void F_Sensor_DataCount(TSensor* pSensor, double d);
uint8_t F_Sensor_DataReady(int nDataDelay, int nDataTick);

#endif /* CSENSOR_H_ */
