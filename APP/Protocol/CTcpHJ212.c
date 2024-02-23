/*
 * CTcpHJ212.c
 *
 *  Created on: Jan 20, 2024
 *      Author: hjx
 */

#include "CProtocol.h"

#include "cmsis_os.h"

#include <string.h>
#include <stdio.h>

#include "cmsis_os.h"

#include "stm32f4xx_ll_utils.h"

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
#include "Config.h"
#include "CSensor.h"


static char s_STCode[SIZE_8];
static char s_MNValue[SIZE_32];
static char s_KeyList[MAX_SENSOR][SIZE_32];

#define HJ212_DATA_CN_COUNT			4
static char s_CNList[HJ212_DATA_CN_COUNT][SIZE_8];

// 重启定时器
#define TIMER_ID_HJ212_SYSTEMRESET 	(TIMER_ID_APP+11)
static EOTTimer s_TimerSystmReset;

//
// 每个通道有自己独立的数据
//
typedef struct _stTHJ212Data
{
	// 索引，对应连接对象 EOTConnect.id
	uint8_t id;
	uint8_t r2;
	uint8_t r3;
	uint8_t r4;

	int last_minute;
	uint64_t tick_connect;
}
THJ212Data;
static THJ212Data s_ListHJ212Data[MAX_TCP_CONNECT];

/**
 * 延时重启
 */
static void OnTimer_SystmReset(EOTTimer* tpTimer)
{
	_T("\r\n\r\n---------------- 系统重启 ----------------\r\n\r\n\r\n");
	LL_mDelay(3000);
	NVIC_SystemReset();

	while (1) {}
}


/**
 * 包含CP=&&DataTime 结束无;
 */
static int TcpHJ212SendBegin(char* pSendBuffer, EOTDate* pDate, char* sCNCode)
{
	char sQnTime[16];
	char sDateTime[16];
	sprintf(sQnTime, "%04d%02d%02d%02d%02d%02d",
			YEAR_ZERO + pDate->year, pDate->month, pDate->date,
			pDate->hour, pDate->minute, pDate->second);

	sprintf(sDateTime, "%04d%02d%02d%02d%02d%02d",
			YEAR_ZERO + pDate->year, pDate->month, pDate->date,
			pDate->hour, pDate->minute, 0);

	int len = 0;
	len = sprintf(&pSendBuffer[len],
			"##0000QN=%s000;ST=%s;CN=%s;PW=123456;MN=%s;Flag=5;CP=&&DataTime=%s",
			sQnTime, s_STCode, sCNCode, s_MNValue, sDateTime);

	return len;
}

static void TcpHJ212SendEnd(char* pSendBuffer, uint8_t nChannel, int len)
{
	if (pSendBuffer == NULL || len >= (SEND_MAX - 6))
	{
		// 程序已经崩溃
		_T("*** 发送数据溢出");
		return;
	}

	pSendBuffer[len+0] = '&';
	pSendBuffer[len+1] = '&';
	len += 2;

	int dn = len - 6;
	// 预留长度位，10进制字符串直接赋值效率更高，不包含开始的##和4位长度
	pSendBuffer[2] = '0' + dn % 10000 / 1000;
	pSendBuffer[3] = '0' + dn % 1000 / 100;
	pSendBuffer[4] = '0' + dn % 100 / 10;
	pSendBuffer[5] = '0' + dn % 10;

	uint16_t nCrc = F_Protocol_HJ212CRC16((uint8_t*)&pSendBuffer[6], dn);
	sprintf(&pSendBuffer[len], "%04X\r\n", nCrc);

	_T("数据[%d]: [%d]%s", nChannel, len, (const char*)pSendBuffer);
	// 包含crc \r\n
	EON_Tcp_SendData(nChannel, (uint8_t*)pSendBuffer, len+6);
}


static void TcpHJ212_Send3101(EOTConnect *tConnect, char* pSendBuffer)
{
	int len = 0;
	uint8_t nChannel = tConnect->channel;

	EOTDate tDate = {0};
	EOB_Date_Get(&tDate);

	TIAPConfig* tIAPConfig = F_IAPConfig_Get();

	len = TcpHJ212SendBegin(pSendBuffer, &tDate, "3101");
	len += sprintf(&pSendBuffer[len],
			";DKey=%s;DType=%s;DVersion=%s;",
			tIAPConfig->device_key,
			__APP_BIN_TYPE,
			__APP_BIN_VERSION);

	len += F_AppConfigString(&pSendBuffer[len], SEND_MAX - len);

	// 去掉最后一个;
	--len;

	TcpHJ212SendEnd(pSendBuffer, nChannel, len);
}

static void OnTcpHJ212_3102(EOTConnect *tConnect, char* pCPStr, char* pSendBuffer)
{
	// 直接将收到的CP保存到配置区
	F_SaveAppConfigString(pCPStr, -1);

	// 延迟重启
	EOS_Timer_Start(&s_TimerSystmReset);
}

static void OnTcpOpen(EOTConnect *connect, int code)
{
}
static int OnTcpRecv(EOTConnect* tConnect, uint8_t* pData, int nLength)
{
	// HJ212以\r\n返回
	// 将换行符替换成结束符方便字符串操作

	pData[nLength - 1] = '\0';
	const char* sData = (const char*)pData;
	_T("HJ212接收数据返回[%d]: <%d> %s", tConnect->channel, nLength, sData);

	// 进行命令解析
	char* p1;
	char* p2;

	p1 = strnstr(sData, "CP=&&", nLength);
	if (p1 == NULL)
	{
		_T("未找到CP起始节点");
		return 0;
	}
	char* pCPStr = p1 + 5;
	p2 = strnstr(pCPStr, "&&", nLength - 5);
	if (p2 == NULL)
	{
		_T("未找到CP结束节点");
		return 0;
	}
	*p2 = '\0';

	// 再查找CN
	p1 = strnstr(sData, "CN=", nLength);
	if (p1 == NULL)
	{
		_T("未找到CN起始节点");
		return 0;
	}
	char* pCNStr = p1 + 3;
	p2 = strnstr(pCNStr, ";", nLength - 3);
	if (p2 == NULL)
	{
		_T("未找到CN结束节点");
		return 0;
	}
	*p2 = '\0';

	_T("CN: %s", pCNStr);
	_T("CP: %s", pCPStr);

	char* pSendBuffer = F_Protocol_SendBuffer();
	if (strcmp(pCNStr, "3101") == 0)
	{
		TcpHJ212_Send3101(tConnect, pSendBuffer);
	}
	else if (strcmp(pCNStr, "3102") == 0)
	{
		OnTcpHJ212_3102(tConnect, pCPStr, pSendBuffer);
	}

	return 0;
}
static void OnTcpClose(EOTConnect* tConnect, int code)
{
}

static void TcpHJ212_SendDataHis(char* pSendBuffer, uint8_t nChannel, EOTDate* pDate, char* sCNCode, uint8_t nDataId)
{
	int i;

	int len = 0;
	len = TcpHJ212SendBegin(pSendBuffer, pDate, sCNCode);

	char* k;
	TRegisterData* pRegHis;
	TSensor* pSensor;

	TSensor* pSensorList = F_Sensor_List();
	int nSensorCount = F_Sensor_Count();
	for (i=0; i<nSensorCount; i++)
	{
		// 异步数据，非线程安全
		pSensor = &pSensorList[i];

		k = s_KeyList[i];
		if (*k == '\0') continue;

		pRegHis = &pSensor->data_h[nDataId];

		len += sprintf(&pSendBuffer[len],
				";%s-Avg=%.1f,%s-Min=%.1f,%s-Max=%.1f,%s-Flag=N",
				k, pRegHis->data,
				k, pRegHis->min,
				k, pRegHis->max,
				k);
	}

	TcpHJ212SendEnd(pSendBuffer, nChannel, len);
}

static void TcpHJ212_SendDataRt(char* pSendBuffer, uint8_t nChannel, EOTDate* pDate)
{
	int i;

	int len = 0;
	len = TcpHJ212SendBegin(pSendBuffer, pDate, "2011");

	char* k;
	TSensor* pSensor;
	TSensor* pSensorList = F_Sensor_List();;
	int nSensorCount = F_Sensor_Count();
	for (i=0; i<nSensorCount; i++)
	{
		// 异步数据，非线程安全
		pSensor = &pSensorList[i];

		k = s_KeyList[i];
		if (*k == '\0') continue;

		len += sprintf(&pSendBuffer[len],
				";%s-Rtd=%.1f,%s-Flag=N",
				k, pSensor->data_rt.data, k);
	}

	TcpHJ212SendEnd(pSendBuffer, nChannel, len);
}

static void TcpHJ212_Start(TNetChannel* pNetChannel)
{
	EON_Tcp_SetConnect(
			pNetChannel->gprs_id,
			F_ConfigGetString("server%d.host", pNetChannel->cfg_id),
			F_ConfigGetInt32("server%d.port", pNetChannel->cfg_id),
			(EOFuncNetOpen)OnTcpOpen,
			(EOFuncNetRecv)OnTcpRecv,
			(EOFuncNetClose)OnTcpClose);

	THJ212Data* pData = &s_ListHJ212Data[pNetChannel->id];
	pData->last_minute = INT_MAX;
	pData->tick_connect = 0L;
}

static void TcpHJ212_Update(TNetChannel* pNetChannel, uint64_t tick, EOTDate* pDate)
{
	int dm, dp, dy, td;
	int i;

	// 以0点为基点
	dm = pDate->hour * 60 + pDate->minute;

	THJ212Data* pData = &s_ListHJ212Data[pNetChannel->id];

	if (pData->last_minute == dm) return;
	pData->last_minute = dm;

	//_T("F_Protocol_TcpHJ212_Update = %d", s_LastMinute);

	char* pSendBuffer = F_Protocol_SendBuffer();

	if (EON_Tcp_IsConnect(pNetChannel->gprs_id))
	{
		// 先发送实时数据
		TcpHJ212_SendDataRt(pSendBuffer, pNetChannel->gprs_id, pDate);

		// 发送统计数据
		for (i=0; i<TICK_UPDATE_MAX; i++)
		{
			td = pNetChannel->tick_delay[i];
			if (td <= 0) continue;

			dy = dm % td;
			dp = dm / td;
			if (dy == 0)
			{
				// 整点时开始变节点
				pNetChannel->tick_last[i] = dp;
			}

			// 同相开始发送
			if (pNetChannel->tick_last[i] == dp)
			{
				// 因为发送节点和传感器节点可能不一致
				// 需要等待传感器节点同步

				_T("检查发送时间节点[%d]: (%d) %d, %d, %d", i, td, dm, dy, dp);
				if (F_Sensor_DataReady(td, dp))
				{
					// 不同的时间刻度统计数据对应不通的因子
					TcpHJ212_SendDataHis(pSendBuffer, pNetChannel->gprs_id, pDate, s_CNList[i+1], i);
					// 发送成功，翻转节点
					pNetChannel->tick_last[i] = INT_MAX;
				}
			}
		}
	}
	else
	{
		// 定时检查是否连接
		if (EOG_Check_Delay(&pData->tick_connect, tick, 45000))
		{
			EON_Tcp_Open(pNetChannel->gprs_id);
		}
	}
}

/**
 * 协议初始化函数
 * 该函数均需在CProtocol.c F_Protocol_Type 被调用
 */
void F_Protocol_TcpHJ212_Init(TNetChannel* pNetChannel, char* sType)
{
	if (strcmp(sType, "TcpHJ212") != 0) return;

	EOS_Timer_Init(&s_TimerSystmReset,
			TIMER_ID_HJ212_SYSTEMRESET, 5000, TIMER_LOOP_ONCE, NULL,
			OnTimer_SystmReset);

	pNetChannel->protocol = TcpHJ212;
	pNetChannel->start_cb = TcpHJ212_Start;
	pNetChannel->update_cb = TcpHJ212_Update;

	_T("配置协议[%d]: %s", pNetChannel->cfg_id, sType);

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

	// 有多少个传感器数据对应多少个因子

	char* sForm = F_ConfigGetString("server%d.form", pNetChannel->cfg_id);
	cnt = MAX_SENSOR;
	strcpy(s, sForm); // 避免破坏性分割
	if (EOG_SplitString(s, -1, ',', (char**)ss, &cnt) <= 0)
	{
		_T("*** 网络参数设置错误 %d", cnt);
		return;
	}

	//F_ConfigAdd("server1.form", "ST:39,a01001:1,a01002:2");
	char sKey[SIZE_32];
	char sVal[SIZE_128];
	int nSensorId;
	for (i=0; i<cnt; i++)
	{
		EOG_KeyValue(ss[i], -1, ':', '\0', sKey, SIZE_32, sVal, SIZE_128);

		if (strcmpi(sKey, "ST") == 0)
		{
			strcpy(s_STCode, sVal);
		}
		else if (strcmpi(sKey, "MN") == 0)
		{
			strcpy(s_MNValue, sVal);
		}
		else
		{
			nSensorId = atoi(sVal); // 从1开始
			strcpy(s_KeyList[nSensorId-1], sKey);

			_T("对应因子: %s = %d", s_KeyList[nSensorId-1], nSensorId);
		}
	}

	_T("对应参数: MN = %s, ST = %s", s_MNValue, s_STCode);

	strcpy(s_CNList[0], "2011");
	strcpy(s_CNList[1], "2051");
	strcpy(s_CNList[2], "2061");
	strcpy(s_CNList[3], "2031");

	// 重置
	memset(s_ListHJ212Data, 0, sizeof(THJ212Data) * MAX_TCP_CONNECT);
	for (i=0; i<MAX_TCP_CONNECT; i++)
	{
		s_ListHJ212Data[i].id = i;
	}
}

