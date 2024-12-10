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

#include "eob_util.h"
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

#define VERSION_BIN_SIZE 1000


static char s_STCode[SIZE_8];
static char s_MNValue[SIZE_32];
static char s_KeyList[MAX_SENSOR][SIZE_32];

#define HJ212_DATA_CN_COUNT			4
static char s_CNList[HJ212_DATA_CN_COUNT][SIZE_8];

// ������ʱ��
#define TIMER_ID_HJ212_SYSTEMRESET 	(TIMER_ID_APP+11)
static EOTTimer s_TimerSystmReset;


static uint8_t s_UpdateChannelId = 0xFF;
// �����ļ���С
// ͬʱ��Ϊ���
// ��s_UpdateFileTotal>0ʱ�����������ļ�
// ��s_UpdateFileTotal<=0ʱ������HJ212����
static int s_UpdateFileTotal = 0;
static int s_UpdateFilePos = 0;
static int s_UpdateBufferSize = 0;
// ������ǳ�ʱ
static int s_UpdateTickLast = 0;

static char s_UpdateCrc[SIZE_40] = {0};
static char s_UpdateType[SIZE_32] = {0};
static char s_UpdateVersion[SIZE_32] = {0};

//
// ÿ��ͨ�����Լ�����������
//
typedef struct _stTHJ212Data
{
	// ��������Ӧ���Ӷ��� EOTConnect.id
	uint8_t id;
	uint8_t r2;
	uint8_t r3;
	uint8_t r4;

	int last_minute;
	uint64_t tick_connect;
}
THJ212Data;
static THJ212Data s_ListHJ212Data[MAX_TCP_CONNECT];


static void GetCrcBuffer(uint8_t* pCrc, char* sVal)
{
	int nLen = strnlen(sVal, SIZE_32);
	_T("CRCУ���� Length=%d", nLen);
	if (nLen != SIZE_32)
	{
		return;
	}

	uint8_t d1, d2;
	int i;
	for (i=0; i<SIZE_16; i++)
	{
		d1 = sVal[i * 2];
		d2 = sVal[i * 2 + 1];
		CHAR_HEX(d1);
		CHAR_HEX(d2);

		pCrc[i] =  d1 << 8 | d2;
	}
}


/**
 * ��ʱ����
 */
static void OnTimer_SystmReset(EOTTimer* tpTimer)
{
	NVIC_SystemReset();
}

#include "eob_w25q.h"

#include "IAPConfig.h"

static uint32_t s_W25Q_Address = 0x1FFFFFFF;
static int s_W25Q_Length = 0;


static void ResetVersionUpdate(void)
{
	// ����
	s_UpdateChannelId = 0xFF;
	s_UpdateFileTotal = 0;
	s_UpdateFilePos = 0;
	s_UpdateBufferSize = 0;

	s_UpdateTickLast = 0;
}

/**
 * �汾�ļ���ʼ
 * �汾�ļ�д�������һ�汾���飬д��֮����ĵ�ǰ�汾��������
 */
static void TcpHJ212BinBegin()
{
	TIAPConfig* tIAPConfig = F_IAPConfig_Get();

	// ����д
	// �����ǰ�汾��0��д������2������д������1
	if (tIAPConfig->region == 0)
	{
		_T("�汾������ʼ �汾1");
		s_W25Q_Address = W25Q_SECTOR_BIN_2;
	}
	else
	{
		_T("�汾������ʼ �汾0");
		s_W25Q_Address = W25Q_SECTOR_BIN_1;
	}

	int i;
	int nAddress = s_W25Q_Address;
	for (i=0; i<W25Q_BIN_BLOCK_COUNT; i++)
	{
		EOB_W25Q_EraseBlock(nAddress);
		nAddress += W25Q_BLOCK_SIZE;
	}

	// �����ļ���С
	s_W25Q_Length = 0;
}

/**
 * �ļ�����
 */
static void TcpHJ212BinEnd()
{
	TIAPConfig* tIAPConfig = F_IAPConfig_Get();
	//
	// �����ļ���С�����л��汾
	if (tIAPConfig->region == 0)
	{
		_T("�汾�������� �汾1");
		tIAPConfig->bin2_length = s_W25Q_Length;
	}
	else
	{
		_T("�汾�������� �汾0");
		tIAPConfig->bin1_length = s_W25Q_Length;
	}

	// ���ı�APP��ʶ
	F_IAPConfig_Save(CONFIG_FLAG_NONE);
}


/**
 * ���ÿ�ʼ
 * ����д�������������
 */
static void TcpHJ212DatBegin()
{
	TIAPConfig* tIAPConfig = F_IAPConfig_Get();

	//EOB_W25Q_EraseAll();
	if (tIAPConfig->region == 0)
	{
		_T("����������ʼ �汾1");
		s_W25Q_Address = W25Q_SECTOR_DAT_2;
	}
	else
	{
		_T("����������ʼ �汾0");
		s_W25Q_Address = W25Q_SECTOR_DAT_1;
	}

	// ����ֻռ1������
	EOB_W25Q_EraseBlock(s_W25Q_Address);

	// �������ô�С
	s_W25Q_Length = 0;
}

/**
 * ���ý���
 */
static void TcpHJ212DatEnd()
{
	TIAPConfig* tIAPConfig = F_IAPConfig_Get();

	// �������ô�С�������汾��������
	if (tIAPConfig->region == 0)
	{
		tIAPConfig->dat2_length = s_W25Q_Length;
		tIAPConfig->region = 1;
	}
	else
	{
		tIAPConfig->dat1_length = s_W25Q_Length;
		tIAPConfig->region = 0;
	}

	_T("ִ������: ���ý���[�汾%d]", tIAPConfig->region);

	// ִ��ˢд
	F_IAPConfig_Save(CONFIG_FLAG_FLASH);
}

/**
 * ����CP=&&DataTime ������;
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
		// �����Ѿ�����
		_T("*** �����������");
		return;
	}

	pSendBuffer[len+0] = '&';
	pSendBuffer[len+1] = '&';
	len += 2;

	int dn = len - 6;
	// Ԥ������λ��10�����ַ���ֱ�Ӹ�ֵЧ�ʸ��ߣ���������ʼ��##��4λ����
	pSendBuffer[2] = '0' + dn % 10000 / 1000;
	pSendBuffer[3] = '0' + dn % 1000 / 100;
	pSendBuffer[4] = '0' + dn % 100 / 10;
	pSendBuffer[5] = '0' + dn % 10;

	uint16_t nCrc = F_Protocol_HJ212CRC16((uint8_t*)&pSendBuffer[6], dn);
	sprintf(&pSendBuffer[len], "%04X\r\n", nCrc);

	_T("����[%d]: [%d]%s", nChannel, len, (const char*)pSendBuffer);
	// ����crc \r\n
	EON_Tcp_SendData(nChannel, (uint8_t*)pSendBuffer, len+6);
}

/**
 * ����λ���������ò���
 */
static void TcpHJ212_Send3020(EOTConnect *tConnect, char* pSendBuffer)
{
	int len = 0;
	uint8_t nChannel = tConnect->channel;

	EOTDate tDate = {0};
	EOB_Date_Get(&tDate);

	TIAPConfig* tIAPConfig = F_IAPConfig_Get();

	len = TcpHJ212SendBegin(pSendBuffer, &tDate, "3020");
	len += sprintf(&pSendBuffer[len],
			";DKey=%s;DType=%s;DVersion=%s;",
			tIAPConfig->device_key,
			__APP_BIN_TYPE,
			__APP_BIN_VERSION);

	len += F_AppConfigString(&pSendBuffer[len], SEND_MAX - len);

	// ȥ�����һ��;
	--len;

	TcpHJ212SendEnd(pSendBuffer, nChannel, len);
}

/**
 * ��ȡָ���汾���ļ���
 */
static void TcpHJ212_Send3111(EOTConnect *tConnect, char* pSendBuffer)
{
	uint8_t nChannel = tConnect->channel;

	EOTDate tDate = {0};
	EOB_Date_Get(&tDate);

	// ���ý���
	s_UpdateBufferSize = 0;

	int len = TcpHJ212SendBegin(pSendBuffer, &tDate, "3111");
	len += sprintf(&pSendBuffer[len],
			";UCrc=%s;UPos=%d;USize=%d",
			s_UpdateCrc, s_UpdateFilePos,
			VERSION_BIN_SIZE);
	TcpHJ212SendEnd(pSendBuffer, nChannel, len);
}

/**
 * ��ȡָ���汾������
 */
static void TcpHJ212_Send3112(EOTConnect *tConnect, char* pSendBuffer)
{
	uint8_t nChannel = tConnect->channel;

	EOTDate tDate = {0};
	EOB_Date_Get(&tDate);

	// ���ý���
	s_UpdateBufferSize = 0;

	int len = TcpHJ212SendBegin(pSendBuffer, &tDate, "3112");
	len += sprintf(&pSendBuffer[len], ";UCrc=%s", s_UpdateCrc);
	TcpHJ212SendEnd(pSendBuffer, nChannel, len);
}

/**
 * ����λ�������ǰ����
 */
static void OnTcpHJ212_3021(EOTConnect *tConnect, char* pCPStr, char* pSendBuffer)
{
	// ֱ�ӽ��յ���CP���浽������
	F_SaveAppConfigString(pCPStr, -1);

	// �����Ϣ���ն˷�����λ����Ҫ�ظ�
	uint8_t nChannel = tConnect->channel;

	EOTDate tDate = {0};
	EOB_Date_Get(&tDate);

	int len = TcpHJ212SendBegin(pSendBuffer, &tDate, "3021");
	TcpHJ212SendEnd(pSendBuffer, nChannel, len);

	// �ӳ�����
	EOS_Timer_Start(&s_TimerSystmReset);
}


/**
 * ���°汾
 */
static void OnTcpHJ212_3111(EOTConnect *tConnect, char* pCPStr, char* pSendBuffer)
{
	uint8_t i;
	char* ppArray[SIZE_128];
	uint8_t nCount = SIZE_128;
	if (EOG_SplitString(pCPStr, -1, ';', ppArray, &nCount) <= 0)
	{
		_T("HJ212���������3111[%d]: %s", tConnect->channel, pCPStr);
		return;
	}

	char* sKey;
	char* sVal;
	for (i=0; i<nCount; i++)
	{
		EOG_KeyValueChar(ppArray[i], '=', &sKey, &sVal);
		if (sKey != NULL && sVal != NULL)
		{
			if (strcmp(sKey, "UType") == 0)
			{
				strcpy(s_UpdateType, sVal);
			}
			else if (strcmp(sKey, "UVersion") == 0)
			{
				strcpy(s_UpdateVersion, sVal);
			}
			else if (strcmp(sKey, "UTotal") == 0)
			{
				s_UpdateFileTotal = atoi(sVal);
			}
			else if (strcmp(sKey, "UCrc") == 0)
			{
				strcpy(s_UpdateCrc, sVal);
			}
		}
	}

	// ����ȫ��ƫ����
	s_UpdateChannelId = tConnect->channel;
	s_UpdateFilePos = 0;
	_T("�豸����: Type=%s,Version=%s,Total=%d,Crc=%s",
			s_UpdateType, s_UpdateVersion, s_UpdateFileTotal, s_UpdateCrc);

	// ׼������
	TcpHJ212BinBegin();

	TcpHJ212_Send3111(tConnect, pSendBuffer);
}


/**
 * ��������
 */
static void OnTcpHJ212_3112(EOTConnect *tConnect, char* pCPStr, char* pSendBuffer)
{
	int nLength = strlen(pCPStr);

	// �����汾������
	TcpHJ212DatBegin();

	// д��W25Q
	EOB_W25Q_WriteData(s_W25Q_Address, pCPStr, nLength);
	s_W25Q_Address += nLength;
	s_W25Q_Length += nLength;

	// �л��汾
	TcpHJ212DatEnd();

	// �����Ϣ���ն˷��𣬲�Ҫ�ظ�

	// �ӳ�����
	EOS_Timer_Start(&s_TimerSystmReset);
}

static uint8_t OnTcpHJ212(EOTConnect* tConnect, uint8_t* pData, int nLength)
{
	// ##0000CP=&&&&XXXX\r\d
	if (nLength < 20)
	{
		_T("��HJ212���ݷ���[%d]: <%d>", tConnect->channel, nLength);
		return EO_FALSE;
	}
	// HJ212��\r\n����
	// �����з��滻�ɽ����������ַ�������

	if (pData[0] != '#' || pData[1] != '#')
	{
		_T("��HJ212���ݷ���[%d]: <%d>", tConnect->channel, nLength);
		return EO_FALSE;
	}

	pData[nLength - 1] = '\0';
	const char* sData = (const char*)pData;
	_T("HJ212�������ݷ���[%d]: <%d> %s", tConnect->channel, nLength, sData);

	// �����������
	char* p1;
	char* p2;

	p1 = strnstr(sData, "CP=&&", nLength);
	if (p1 == NULL)
	{
		_T("δ�ҵ�CP��ʼ�ڵ�");
		return EO_FALSE;
	}
	char* pCPStr = p1 + 5;
	p2 = strnstr(pCPStr, "&&", nLength - 5);
	if (p2 == NULL)
	{
		_T("δ�ҵ�CP�����ڵ�");
		return EO_FALSE;
	}
	*p2 = '\0';

	// �ٲ���CN
	p1 = strnstr(sData, "CN=", nLength);
	if (p1 == NULL)
	{
		_T("δ�ҵ�CN��ʼ�ڵ�");
		return EO_FALSE;
	}
	char* pCNStr = p1 + 3;
	p2 = strnstr(pCNStr, ";", nLength - 3);
	if (p2 == NULL)
	{
		_T("δ�ҵ�CN�����ڵ�");
		return EO_FALSE;
	}
	*p2 = '\0';

	_T("CN: %s", pCNStr);
	_T("CP: %s", pCPStr);

	char* pSendBuffer = F_Protocol_SendBuffer();
	if (strcmp(pCNStr, "3020") == 0)
	{
		// ����״̬
		TcpHJ212_Send3020(tConnect, pSendBuffer);
	}
	else if (strcmp(pCNStr, "3021") == 0)
	{
		// ������Ϣ
		OnTcpHJ212_3021(tConnect, pCPStr, pSendBuffer);
	}
	else if (strcmp(pCNStr, "3111") == 0)
	{
		// ��������
		OnTcpHJ212_3111(tConnect, pCPStr, pSendBuffer);
	}
	else if (strcmp(pCNStr, "3112") == 0)
	{
		// ��������
		OnTcpHJ212_3112(tConnect, pCPStr, pSendBuffer);
	}
	else if (strcmp(pCNStr, "3199") == 0)
	{
		// ����
		EOB_SystemReset();
	}

	return EO_TRUE;
}


/**
 * �汾����
 */
static void OnTcpVersionUpdate(EOTConnect* tConnect, uint8_t* pData, int nLength)
{
	s_UpdateBufferSize += nLength;
	s_UpdateFilePos += nLength;

	_T("�յ��汾����: %d / %d -> %d / %d",
			nLength, s_UpdateBufferSize, s_UpdateFilePos, s_UpdateFileTotal);

	// д��W25Q
	EOB_W25Q_WriteData(s_W25Q_Address, pData, nLength);
	s_W25Q_Address += nLength;
	s_W25Q_Length += nLength;

	char* pSendBuffer = F_Protocol_SendBuffer();

	// ��������
	if (s_UpdateFilePos < s_UpdateFileTotal)
	{
		// �����Ѿ�ȫ����ȡ
		if (s_UpdateBufferSize >= VERSION_BIN_SIZE)
		{
			TcpHJ212_Send3111(tConnect, pSendBuffer);
		}
	}
	else
	{
		// �л���ʶ
		s_UpdateFileTotal = 0;

		// �Ȳ��л���ǰ�汾���ȴ���ȡ����
		TcpHJ212BinEnd();

		// ��ȡĬ������
		TcpHJ212_Send3112(tConnect, pSendBuffer);
	}
}

static void OnTcpOpen(EOTConnect *connect, int code)
{
	// ����
	ResetVersionUpdate();
}
static int OnTcpRecv(EOTConnect* tConnect, uint8_t* pData, int nLength)
{
	// ��ֹ�����ж�
	if (OnTcpHJ212(tConnect, pData, nLength) != EO_TRUE)
	{
		if (s_UpdateFileTotal > 0 && s_UpdateChannelId == tConnect->channel)
		{
			OnTcpVersionUpdate(tConnect, pData, nLength);
		}
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
		// �첽���ݣ����̰߳�ȫ
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
		// �첽���ݣ����̰߳�ȫ
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
	_T("���� TcpHJ212");
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

	// ����
	ResetVersionUpdate();
}

/**
 * ��������HJ212����
 */
static void TcpHJ212_SendData(TNetChannel* pNetChannel, uint64_t tick, EOTDate* pDate, int dm)
{
	int i;
	int dp, dy, td;

	// �����ڼ䲻��������
	if (s_UpdateFileTotal > 0)
	{
		return;
	}

	char* pSendBuffer = F_Protocol_SendBuffer();

	// �ȷ���ʵʱ����
	TcpHJ212_SendDataRt(pSendBuffer, pNetChannel->gprs_id, pDate);

	// ����ͳ������
	for (i=0; i<TICK_UPDATE_MAX; i++)
	{
		td = pNetChannel->tick_delay[i];
		if (td <= 0) continue;

		dy = dm % td;
		dp = dm / td;

		_T("��鷢��ʱ��ڵ�[%d]: (%d) %d, %d, %d", i, td, dm, dy, dp);

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
			if (F_Sensor_DataReady(td, dp))
			{
				// ��ͬ��ʱ��̶�ͳ�����ݶ�Ӧ��ͨ������
				TcpHJ212_SendDataHis(pSendBuffer, pNetChannel->gprs_id, pDate, s_CNList[i+1], i);
				// ���ͳɹ�����ת�ڵ�
				pNetChannel->tick_last[i] = INT_MAX;
			}
		}
	}
}

/**
 * ��ѯ������
 */
static void TcpHJ212_Update(TNetChannel* pNetChannel, uint64_t tick, EOTDate* pDate)
{
	int dm;

	EOS_Timer_Update(&s_TimerSystmReset, tick);

	// ��0��Ϊ����
	dm = pDate->hour * 60 + pDate->minute;

	THJ212Data* pData = &s_ListHJ212Data[pNetChannel->id];

	if (pData->last_minute == dm) return;
	pData->last_minute = dm;

	_T("TcpHJ212_Update = %d", dm);
	if (EON_Tcp_IsConnect(pNetChannel->gprs_id))
	{
		TcpHJ212_SendData(pNetChannel, tick, pDate, dm);
	}
	else
	{
		// ��ʱ����Ƿ�����
		if (EOG_Check_Delay(&pData->tick_connect, tick, 45000))
		{
			EON_Tcp_Open(pNetChannel->gprs_id);
		}
	}
}

/**
 * Э���ʼ������
 * �ú���������CProtocol.c F_Protocol_Type ������
 */
void F_Protocol_TcpHJ212_Init(TNetChannel* pNetChannel, char* sType)
{
	if (strcmp(sType, "TcpHJ212") != 0) return;

	// ����Update�е��� EOS_Timer_Update
	EOS_Timer_Init(&s_TimerSystmReset,
			TIMER_ID_HJ212_SYSTEMRESET, 3000, TIMER_LOOP_ONCE, NULL,
			OnTimer_SystmReset);

	pNetChannel->protocol = TcpHJ212;
	pNetChannel->start_cb = TcpHJ212_Start;
	pNetChannel->update_cb = TcpHJ212_Update;

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

	// �ж��ٸ����������ݶ�Ӧ���ٸ�����

	char* sForm = F_ConfigGetString("server%d.form", pNetChannel->cfg_id);
	cnt = MAX_SENSOR;
	strcpy(s, sForm); // �����ƻ��Էָ�
	if (EOG_SplitString(s, -1, ',', (char**)ss, &cnt) <= 0)
	{
		_T("*** ����������ô��� %d", cnt);
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
			nSensorId = atoi(sVal); // ��1��ʼ
			strcpy(s_KeyList[nSensorId-1], sKey);

			_T("��Ӧ����: %s = %d", s_KeyList[nSensorId-1], nSensorId);
		}
	}

	_T("��Ӧ����: MN = %s, ST = %s", s_MNValue, s_STCode);

	strcpy(s_CNList[0], "2011");
	strcpy(s_CNList[1], "2051");
	strcpy(s_CNList[2], "2061");
	strcpy(s_CNList[3], "2031");

	// ����
	memset(s_ListHJ212Data, 0, sizeof(THJ212Data) * MAX_TCP_CONNECT);
	for (i=0; i<MAX_TCP_CONNECT; i++)
	{
		s_ListHJ212Data[i].id = i;
	}
}

