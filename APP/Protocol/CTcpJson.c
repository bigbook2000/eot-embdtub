/*
 * CTcpJson.c
 *
 *  Created on: Feb 1, 2024
 *      Author: hjx
 *
 * JSON��ʽ
 *
 */


#include "CProtocol.h"

#include "cmsis_os.h"

#include <string.h>
#include <stdio.h>

#include "eos_inc.h"
#include "eos_timer.h"
#include "eob_debug.h"
#include "eob_date.h"
#include "eob_tick.h"
#include "eob_w25q.h"

#include "eon_ec800.h"
#include "eon_tcp.h"
#include "eon_http.h"
#include "eon_mqtt.h"

#include "Global.h"
#include "AppSetting.h"
#include "CSensor.h"


static char s_STCode[SIZE_8];
static char s_MNValue[SIZE_32];
static char s_KeyList[MAX_SENSOR][SIZE_32];

static void OnTcpOpen(EOTConnect *connect, int code)
{
}
static int OnTcpRecv(EOTConnect *connect, unsigned char* pData, int nLength)
{
	return 0;
}
static void OnTcpClose(EOTConnect *connect, int code)
{
}

static int TcpJson_SendBegin(char* pSendBuffer, EOTDate* pDate, int nDataTick)
{
	char sDateTime[32];
	sprintf(sDateTime, "%04d%02d%02d%02d%02d%02d",
			YEAR_ZERO + pDate->year, pDate->month, pDate->date,
			pDate->hour, pDate->minute, pDate->second);

	int len = 0;
	len = sprintf(&pSendBuffer[len],
			"{\"DeviceId\":\"%s\",\"DataTime\":\"%s\",\"Tick\":%d,\"Data\":{",
			s_MNValue, sDateTime, nDataTick);

	return len;
}

static void TcpJson_SendDataHis(char* pSendBuffer,
		uint8_t nChannel, EOTDate* pDate, int nDataTick, uint8_t nDataId)
{
	int i;

	int len = TcpJson_SendBegin(pSendBuffer, pDate, nDataTick);

	int n = 0;
	char* k;
	TRegisterData* pRegHis;
	TSensor* pSensor;

	TSensor* pSensorList = F_Sensor_List();
	int nSensorCount = F_Sensor_Count();
	for (i=0; i<nSensorCount; i++)
	{
		// �첽���ݣ����̰߳�ȫ
		pSensor = &pSensorList[i];

		k = s_KeyList[i];
		if (*k == '\0') continue;

		pRegHis = &pSensor->data_h[nDataId];

		len += sprintf(&pSendBuffer[len],
				"\"%s\":{\"Avg\"=%.1f,\"Min\"=%.1f,\"Max\"=%.1f},",
				k, pRegHis->data, pRegHis->min, pRegHis->max);
		++n;
	}

	if (n > 0) len--;

	// ���һ������ȥ��
	sprintf(&pSendBuffer[len], "}}\r\n");
	len += 5; // ����һ��������

	_T("����[%d]: [%d]%s", nChannel, len, (const char*)pSendBuffer);
	EON_Tcp_SendData(nChannel, (uint8_t*)pSendBuffer, len);
}

static void TcpJson_SendDataRt(char* pSendBuffer, uint8_t nChannel, EOTDate* pDate)
{
	int i;

	char sDateTime[32];
	sprintf(sDateTime, "%04d%02d%02d%02d%02d%02d",
			YEAR_ZERO + pDate->year, pDate->month, pDate->date,
			pDate->hour, pDate->minute, pDate->second);

	int len = 0;
	len = sprintf(&pSendBuffer[len],
			"{\"DeviceId\":\"%s\",\"DataTime\":\"%s\",\"Tick\":%d,\"Data\":{",
			s_MNValue, sDateTime, 0);

	int n = 0;
	char* k;
	TSensor* pSensor;
	TSensor* pSensorList = F_Sensor_List();;
	int nSensorCount = F_Sensor_Count();
	for (i=0; i<nSensorCount; i++)
	{
		// �첽���ݣ����̰߳�ȫ
		pSensor = &pSensorList[i];

		k = s_KeyList[i];
		if (*k == '\0') continue;

		len += sprintf(&pSendBuffer[len],
				"\"%s\":{\"Rtd\":%.1f},", k, pSensor->data_rt.data);
		++n;
	}

	if (n > 0) len--;

	// ���һ������ȥ��
	sprintf(&pSendBuffer[len], "}}\r\n");
	len += 5; // ����һ��������

	_T("����[%d]: [%d]%s", nChannel, len, (const char*)pSendBuffer);
	EON_Tcp_SendData(nChannel, (uint8_t*)pSendBuffer, len);
}

static void TcpJson_Start(TNetChannel* pNetChannel)
{
	EON_Tcp_SetConnect(
			pNetChannel->gprs_id,
			F_SettingGetString("svr%d.host", pNetChannel->cfg_id),
			F_SettingGetInt32("svr%d.port", pNetChannel->cfg_id),
			(EOFuncNetOpen)OnTcpOpen,
			(EOFuncNetRecv)OnTcpRecv,
			(EOFuncNetClose)OnTcpClose);
}

static int s_LastMinute = INT_MAX;
static uint64_t s_TickConnect = 0L;
static void TcpJson_Update(TNetChannel* pNetChannel, uint64_t tick, EOTDate* pDate)
{
	int dm, dp, dy, td;
	int i;

	// ��0��Ϊ����
	dm = pDate->hour * 60 + pDate->minute;

	if (s_LastMinute == dm) return;
	s_LastMinute = dm;

	//_T("F_Protocol_TcpJson_Update = %d", s_LastMinute);

	char* pSendBuffer = F_Protocol_SendBuffer();
	if (EON_Tcp_IsConnect(pNetChannel->gprs_id))
	{
		// �ȷ���ʵʱ����
		TcpJson_SendDataRt(pSendBuffer, pNetChannel->gprs_id, pDate);

		// ����ͳ������
		for (i=0; i<TICK_UPDATE_MAX; i++)
		{
			td = pNetChannel->tick_delay[i];
			if (td <= 0) continue;

			dy = dm % td;
			dp = dm / td;
			if (dy == 0)
			{
				// ����ʱ��ʼ��ڵ�
				pNetChannel->tick_last[i] = dp;
			}

			// ͬ�࿪ʼ����
			if (pNetChannel->tick_last[i] == dp)
			{
				// ��Ϊ���ͽڵ�ʹ������ڵ���ܲ�һ��
				// ��Ҫ�ȴ��������ڵ�ͬ��

				_T("��鷢��ʱ��ڵ�[%d]: (%d) %d, %d, %d", i, td, dm, dy, dp);
				if (F_Sensor_DataReady(td, dp))
				{
					// ��ͬ��ʱ��̶�ͳ�����ݶ�Ӧ��ͨ������
					TcpJson_SendDataHis(pSendBuffer, pNetChannel->gprs_id, pDate, td, i);
					// ���ͳɹ�����ת�ڵ�
					pNetChannel->tick_last[i] = INT_MAX;
				}
			}
		}
	}
	else
	{
		// ��ʱ����Ƿ�����
		if (EOG_Check_Delay(&s_TickConnect, tick, 100000))
		{
			EON_Tcp_Open(pNetChannel->gprs_id);
		}
	}
}

/**
 * Э���ʼ������
 * �ú���������CProtocol.c F_Protocol_Type ������
 */
void F_Protocol_TcpJson_Init(TNetChannel* pNetChannel, char* sType)
{
	if (strcmp(sType, "TcpJson") != 0) return;

	pNetChannel->protocol = TcpJson;
	pNetChannel->start_cb = TcpJson_Start;
	pNetChannel->update_cb = TcpJson_Update;

	_T("����Э��[%d]: %s", pNetChannel->cfg_id, sType);

	int i;
	s_STCode[0] = '\0';
	strcpy(s_MNValue, "__U%NONE__");
	for (i=0; i<MAX_SENSOR; i++)
	{
		s_KeyList[i][0] = '\0';
	}

	char s[VALUE_SIZE];
	char* ss[32];
	uint8_t cnt;

	char* sForm = F_SettingGetString("svr%d.form", pNetChannel->cfg_id);

	// �ж��ٸ����������ݶ�Ӧ���ٸ�����
	cnt = MAX_SENSOR;
	strcpy(s, sForm); // �����ƻ��Էָ�
	if (EOG_SplitString(s, -1, ',', (char**)ss, &cnt) <= 0)
	{
		_T("*** ����������ô��� %d", cnt);
		return;
	}

	//F_ConfigAdd("svr2.form", "deviceId:TEST2021-01,temp:1,humi:2");
	char sKey[SIZE_32];
	char sVal[SIZE_128];
	int nSensorId;
	for (i=0; i<cnt; i++)
	{
		EOG_KeyValue(ss[i], -1, ':', '\0', sKey, SIZE_32, sVal, SIZE_128);

		if (strcmpi(sKey, "DeviceId") == 0)
		{
			strcpy(s_MNValue, sVal);
		}
		else
		{
			nSensorId = atoi(sVal); // ��1��ʼ
			strcpy(s_KeyList[nSensorId-1], sKey);

			_T("��Ӧ����: %s = %d", s_KeyList[nSensorId-1], nSensorId);
		}
	}

	_T("��Ӧ����: DeviceId = %s", s_MNValue, s_STCode);
}
