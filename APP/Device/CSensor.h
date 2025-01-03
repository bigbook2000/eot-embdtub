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
 * 数据寄存器
 */
typedef struct _stRegisterData
{
	// 索引
	uint8_t id;

	// 地址
	uint8_t address;
	// 数据长度
	uint8_t length;
	// 数据类型
	uint8_t type;

	uint32_t count;
	// 数据
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
	// 索引
	uint8_t id;

	uint8_t device_id;

	// 设备数据类型
	uint8_t data_type;
	// 设备数据地址
	uint16_t offset;
	// 设备数据长度
	uint8_t length;
	// 字节顺序
	uint8_t endian;

	// 统计数据数量
	uint8_t data_count;

	// 实时数据
	TRegisterData data_rt;
	// 统计数据
	TRegisterData data_h[DATA_COUNT_MAX];

	char name[SENSOR_NAME_SIZE];
}
TSensor;

// 一个设备对应多个Sensor
// 转换函数
typedef double (*EOFuncSensorData)(TSensor* pSensor, void* pTag, uint8_t* pData, int nLength);
// 将设备采集的数据对应到sensor上
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
