/*
 * CTcpHJ212.c
 *
 * 212协议
 * 数据命令2011,2051,2061,2031
 *
 * 3020 上位机获取配置参数
 * 3021 上位机设置配置参数
 *
 * 31xx自定义命令
 * 3111 升级版本，如果版本文件长度Total大于0则升级新版本，如果长度等于0则复制当前版本，仅更新配置
 *
 * 3123 上位机发送控制命令
 * 3124 向上位机传送控制状态
 *
 * 3199 重启设备
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
#include "AppSetting.h"
#include "CDevice.h"
#include "CSensor.h"

#define BASE_DATA_SIZE 768
static uint8_t s_BaseBuffer[SIZE_1K];


static char s_STCode[SIZE_8];
static char s_MNValue[SIZE_32];
static char s_KeyList[MAX_SENSOR][SIZE_32];

#define HJ212_DATA_CN_COUNT			4
static char s_CNList[HJ212_DATA_CN_COUNT][SIZE_8];

// 重启定时器
#define TIMER_ID_HJ212_SYSTEMRESET 	(TIMER_ID_APP+11)
static EOTTimer s_TimerSystmReset;


static uint8_t s_UpdateChannelId = 0xFF;
// 升级文件大小
// 同时作为标记
// 当s_UpdateFileTotal>0时，接收升级文件
// 当s_UpdateFileTotal<=0时，接收HJ212命令
static int s_UpdateTotal = 0;
static int s_UpdatePos = 0;
// 用来标记超时
static int s_UpdateTickLast = 0;

static char s_UpdateCrc[SIZE_40] = {0};


// 使用宏定义，简化代码阅读，处理每个参数
#define HJ212_ARGS_BEGIN() 	uint8_t i;char* ppArray[SIZE_32];uint8_t nCount = SIZE_32;\
							if (EOG_SplitString(pCPStr, -1, ';', ppArray, &nCount) <= 0){_T("HJ212错误的命令: %s", pCPStr);return;}\
							char* sKey;char* sVal;\
							for (i=0; i<nCount; i++) { EOG_KeyValueChar(ppArray[i], '=', &sKey, &sVal);	if (sKey == NULL || sVal == NULL) continue

#define HJ212_ARGS_DATA(n, v) 		if (strcmp(sKey, (n))==0)v=sVal
#define HJ212_ARGS_STRING(n, v) 	if (strcmp(sKey, (n))==0)strcpy((v), sVal)
#define HJ212_ARGS_INT32(n, v)		if (strcmp(sKey, (n))==0)(v)=atoi(sVal)
#define HJ212_ARGS_DOUBLE(n, v)		if (strcmp(sKey, (n))==0)(v)=atof(sVal)

#define HJ212_ARGS_END()	}


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


//static void GetCrcBuffer(uint8_t* pCrc, char* sVal)
//{
//	int nLen = strnlen(sVal, SIZE_32);
//	_T("CRC校验码 Length=%d", nLen);
//	if (nLen != SIZE_32)
//	{
//		return;
//	}
//
//	uint8_t d1, d2;
//	int i;
//	for (i=0; i<SIZE_16; i++)
//	{
//		d1 = sVal[i * 2];
//		d2 = sVal[i * 2 + 1];
//		CHAR_HEX(d1);
//		CHAR_HEX(d2);
//
//		pCrc[i] =  d1 << 8 | d2;
//	}
//}


/**
 * 延时重启
 */
static void OnTimer_SystmReset(EOTTimer* tpTimer)
{
	NVIC_SystemReset();
}

#include "eob_w25q.h"

#include "IAPConfig.h"

static uint32_t s_W25Q_Address = 0x1FFFFFFF;

/**
 * 重置升级状态
 */
static void ResetVersionUpdate(void)
{
	// 重置
	s_UpdateChannelId = 0xFF;
	s_UpdateTotal = 0;
	s_UpdatePos = 0;

	s_UpdateTickLast = 0;
}


/**
 * 版本文件开始
 * 版本文件写入的是下一版本区块，写入之后更改当前版本区块索引
 * 第一个命令只发送总长度和文件标识
 */
static void TcpHJ212BinWrite(int nSize, char* sData)
{
	int nAddress;
	TIAPConfig* tIAPConfig = F_IAPConfig_Get();

	// 交叉写
	// 如果当前版本是0，写入区块2，否则写入区块1
	if (nSize > 0 && sData != NULL)
	{
		int nWrite = EOG_Base64Decode(sData, strlen(sData), s_BaseBuffer, BASE64_CHAR_END);

		// 写入W25Q
		nAddress = s_W25Q_Address + W25Q_PAGE_SIZE + s_UpdatePos;
		EOB_W25Q_WriteData(nAddress, s_BaseBuffer, nWrite);

		_T("收到版本数据[%08lX]: %d", nAddress, nWrite);
		_Tmb(s_BaseBuffer, nWrite);

		s_UpdatePos += nWrite;
	}
	else
	{
		if (tIAPConfig->region == 0)
		{
			s_W25Q_Address = W25Q_SECTOR_BIN_1;
			_T("版本升级开始 版本1, [%08lX], %d", s_W25Q_Address, s_UpdateTotal);
		}
		else
		{
			s_W25Q_Address = W25Q_SECTOR_BIN_0;
			_T("版本升级开始 版本0, [%08lX], %d", s_W25Q_Address, s_UpdateTotal);
		}

		// 第一个命令只发送总长度和文件标识
		int i;
		nAddress = s_W25Q_Address;
		for (i=0; i<W25Q_BIN_BLOCK_COUNT; i++)
		{
			EOB_W25Q_EraseBlock(nAddress);
			nAddress += W25Q_BLOCK_SIZE;
		}

		// 写入大小
		WRITE_HEAD_LENGTH(s_W25Q_Address, s_BaseBuffer, s_UpdateTotal);
	}
}


/**
 * 复制版本
 */
static void TcpHJ212BinCopy()
{
	TIAPConfig* tIAPConfig = F_IAPConfig_Get();

	uint32_t nAddrSrc = 0;
	uint32_t nAddrDst = 0;

	int nLength = 0;
	uint8_t pBuffer[W25Q_PAGE_SIZE];
	if (tIAPConfig->region == 0)
	{
		nAddrSrc = W25Q_SECTOR_BIN_0;
		nAddrDst = W25Q_SECTOR_BIN_1;
		READ_HEAD_LENGTH(nAddrSrc, pBuffer, nLength);
		_T("版本复制开始 版本 0 -> 1, %d", nLength);
	}
	else
	{
		nAddrSrc = W25Q_SECTOR_BIN_1;
		nAddrDst = W25Q_SECTOR_BIN_0;
		READ_HEAD_LENGTH(nAddrSrc, pBuffer, nLength);
		_T("版本复制开始 版本 1 -> 0, %d", nLength);
	}

	int i;
	// 先擦除
	int nAddress = nAddrDst;
	for (i=0; i<W25Q_BIN_BLOCK_COUNT; i++)
	{
		EOB_W25Q_EraseBlock(nAddress);
		nAddress += W25Q_BLOCK_SIZE;
	}

	// +1连头一起复制
	int cnt = nLength / W25Q_PAGE_SIZE + 1;
	if ((nLength % W25Q_PAGE_SIZE) != 0) cnt++;

	for (i=0; i<cnt; i++)
	{
		_T("版本复制: [%08lX] -> [%08lX]", nAddrSrc, nAddrDst);

		// 复制
		EOB_W25Q_ReadDirect(nAddrSrc, pBuffer, W25Q_PAGE_SIZE);

		EOB_W25Q_WriteDirect(nAddrDst, pBuffer, W25Q_PAGE_SIZE);

		nAddrSrc += W25Q_PAGE_SIZE;
		nAddrDst += W25Q_PAGE_SIZE;
	}

	// 不改变APP标识
	F_IAPConfig_Save(CONFIG_FLAG_NONE);

	_T("版本复制完成 Length = %d", nLength);
}



/**
 * 配置开始
 * 配置写入的是新配置区
 */
static void TcpHJ212DatWrite(int nSize, char* sData)
{
	if (nSize <= 0 || sData == NULL)
	{
		_T("*** 未收到配置数据");
		return;
	}

	uint8_t pBuffer[W25Q_PAGE_SIZE];

	if (s_UpdatePos == 0)
	{
		TIAPConfig* tIAPConfig = F_IAPConfig_Get();
		if (tIAPConfig->region == 0)
		{
			_T("配置升级开始 版本1, Size = %d", s_UpdateTotal);
			s_W25Q_Address = W25Q_SECTOR_DAT_1;
		}
		else
		{
			_T("配置升级开始 版本0, Size = %d", s_UpdateTotal);
			s_W25Q_Address = W25Q_SECTOR_DAT_0;
		}

		// 配置只占1个区块
		EOB_W25Q_EraseBlock(s_W25Q_Address);

		// 写入长度
		// 第一页为预留配置, 头4个字节为长度
		WRITE_HEAD_LENGTH(s_W25Q_Address, pBuffer, s_UpdateTotal);
	}

	int nWrite = EOG_Base64Decode(sData, strlen(sData), s_BaseBuffer, BASE64_CHAR_END);

	// 写入W25Q
	int nAddress = s_W25Q_Address + W25Q_PAGE_SIZE + s_UpdatePos;
	EOB_W25Q_WriteData(nAddress, s_BaseBuffer, nWrite);

	_T("收到配置数据[%08lX]: %d", nAddress, nWrite);
	_Tmb(s_BaseBuffer, nWrite);


	s_UpdatePos += nSize;
}

/**
 * 升级结束
 */
static void TcpHJ212UpdateEnd()
{
	TIAPConfig* tIAPConfig = F_IAPConfig_Get();

	// 更新配置大小，交换版本，并重启
	if (tIAPConfig->region == 0)
	{
		tIAPConfig->region = 1;
	}
	else
	{
		tIAPConfig->region = 0;
	}

	_T("执行命令: 配置结束[版本%d]", tIAPConfig->region);

	// 执行刷写
	F_IAPConfig_Save(CONFIG_FLAG_FLASH);

	// 延迟重启
	EOS_Timer_Start(&s_TimerSystmReset);
}

/**
 * 包含CP=&&DataTime 结束无;
 */
static int TcpHJ212SendBegin(char* pSendBuffer, EOTDate* pDate, char* sCNCode)
{
	char sQnTime[32];
	char sDateTime[32];
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


/**
 * 向上位机发送配置参数
 */
static void TcpHJ212_Send3020(EOTConnect *tConnect, char* pSendBuffer,
		int nPos, int nSize, int nTotal)
{
	int nAddress, nDataLength;
	TIAPConfig* tIAPConfig = F_IAPConfig_Get();

	W25Q_ADDRESS_DAT(nAddress, tIAPConfig->region);
	if (nPos == 0)
	{
		// 第一页为预留配置, 头4个字节为长度
		READ_HEAD_LENGTH(nAddress, s_BaseBuffer, nDataLength);
		nTotal = nDataLength;
	}

	nAddress += W25Q_PAGE_SIZE;
	nAddress += nPos;

	nSize = BASE_DATA_SIZE;
	if (nSize > (nTotal - nPos)) nSize = (nTotal - nPos);

	//_T("读取配置 %08lX, %d, %d, %d", nAddress, nPos, nSize, nTotal);
	EOB_W25Q_ReadData(nAddress, s_BaseBuffer, nSize);

	_Tmb(s_BaseBuffer, nSize);

	int len = 0;
	uint8_t nChannel = tConnect->channel;

	EOTDate tDate = {0};
	EOB_Date_Get(&tDate);

	// 含_的标识特殊处理
	len = TcpHJ212SendBegin(pSendBuffer, &tDate, "3020");

	len += sprintf(&pSendBuffer[len],
			";_DKey=%s;_DType=%s;_DVersion=%s",
			tIAPConfig->device_key,
			__APP_BIN_TYPE,
			__APP_BIN_VERSION);

	len += sprintf(&pSendBuffer[len],
			";_UPos=%d;_USize=%d;_UTotal=%d;_Data=",
			nPos, nSize, nTotal);

	len += EOG_Base64Encode(s_BaseBuffer, nSize, &pSendBuffer[len], BASE64_CHAR_END);

	//_T("Base64 = %s", &pSendBuffer[len]);

	TcpHJ212SendEnd(pSendBuffer, nChannel, len);
}

/**
 * 请求获得配置
 * @param tConnect
 * @param pSendBuffer
 * @param sCrc 如果是新版本则输入版本标识，如果仅仅更新配置则输入空字符串
 * @param nPos
 * @param nSize
 * @param nTotal
 */
static void TcpHJ212_Send3021(EOTConnect *tConnect, char* pSendBuffer,
		char* sCrc, int nPos, int nSize, int nTotal)
{
	uint8_t nChannel = tConnect->channel;

	EOTDate tDate = {0};
	EOB_Date_Get(&tDate);

	int len = TcpHJ212SendBegin(pSendBuffer, &tDate, "3021");
	len += sprintf(&pSendBuffer[len],
			";_UCrc=%s;_UPos=%d;_USize=%d;_UTotal=%d",
			sCrc,
			nPos,
			nSize,
			nTotal);
	TcpHJ212SendEnd(pSendBuffer, nChannel, len);
}

/**
 * 获取指定版本的文件流
 */
static void TcpHJ212_Send3111(EOTConnect *tConnect, char* pSendBuffer,
		char* sCrc, int nPos, int nSize, int nTotal)
{
	uint8_t nChannel = tConnect->channel;

	EOTDate tDate = {0};
	EOB_Date_Get(&tDate);

	int len = TcpHJ212SendBegin(pSendBuffer, &tDate, "3111");
	len += sprintf(&pSendBuffer[len],
			";_UCrc=%s;_UPos=%d;_USize=%d;_UTotal=%d",
			sCrc,
			nPos,
			nSize,
			nTotal);
	TcpHJ212SendEnd(pSendBuffer, nChannel, len);
}

/**
 * 上位机获取配置
 */
static void OnTcpHJ212_3020(EOTConnect *tConnect, char* pCPStr, char* pSendBuffer)
{
	int nPos, nSize, nTotal;

	nPos = 0;
	nSize = 0;
	nTotal = 0;

	// 解析参数开始
	HJ212_ARGS_BEGIN();

	HJ212_ARGS_INT32("_UPos", nPos);
	HJ212_ARGS_INT32("_USize", nSize);
	HJ212_ARGS_INT32("_UTotal", nTotal);

	HJ212_ARGS_END();
	// 解析参数结束

	TcpHJ212_Send3020(tConnect, pSendBuffer, nPos, nSize, nTotal);
}

/**
 * 从上位机变更当前配置
 */
static void OnTcpHJ212_3021(EOTConnect *tConnect, char* pCPStr, char* pSendBuffer)
{
	int nSize = 0;
	char* sData = NULL;

	// 解析参数开始
	HJ212_ARGS_BEGIN();

	HJ212_ARGS_STRING("_UCrc", s_UpdateCrc);
	HJ212_ARGS_INT32("_UPos", s_UpdatePos);
	HJ212_ARGS_INT32("_USize", nSize);
	HJ212_ARGS_INT32("_UTotal", s_UpdateTotal);
	HJ212_ARGS_DATA("_Data", sData);

	HJ212_ARGS_END();
	// 解析参数结束

	if (s_UpdatePos > s_UpdateTotal)
	{
		_T("**** **** 设备配置错误: [%d/%d] %d %s",
				s_UpdatePos, s_UpdateTotal, nSize, s_UpdateCrc);
		return;
	}
	else
	{
		// 写入配置
		TcpHJ212DatWrite(nSize, sData);

		if (s_UpdatePos == s_UpdateTotal)
		{
			_T("**** **** 设备配置完成: [%d/%d] %d %s",
					s_UpdatePos, s_UpdateTotal, nSize, s_UpdateCrc);

			// 升级结束
			TcpHJ212UpdateEnd();
		}
		else
		{
			// 继续获取配置
			TcpHJ212_Send3021(tConnect, pSendBuffer,
					s_UpdateCrc, s_UpdatePos, BASE_DATA_SIZE, s_UpdateTotal);
		}
	}
}


/**
 * 更新版本
 * 当终端收到3111指令后，表示升级版本Bin文件
 * 如果UTotal/s_UpdateFileTotal大于0，返回3111接收指定长度的字节数据，接收到指定长度的字节数之后继续发送3111，
 * 接收完成之后返回3112获取配置
 * 如果UTotal/s_UpdateFileTotal小于等于0，跳过接收，复制当前版本返回3112获取配置
 *
 */
static void OnTcpHJ212_3111(EOTConnect *tConnect, char* pCPStr, char* pSendBuffer)
{
	int nSize = 0;
	char* sData = NULL;

	// 解析参数开始
	HJ212_ARGS_BEGIN();

	HJ212_ARGS_STRING("_UCrc", s_UpdateCrc);
	HJ212_ARGS_INT32("_UPos", s_UpdatePos);
	HJ212_ARGS_INT32("_USize", nSize);
	HJ212_ARGS_INT32("_UTotal", s_UpdateTotal);
	HJ212_ARGS_DATA("_Data", sData);

	HJ212_ARGS_END();
	// 解析参数结束

	// 如果文件长度大于0，则发送新的版本
	if (s_UpdateTotal > 0)
	{
		s_UpdateChannelId = tConnect->channel;
		_T("**** **** 设备升级: [%d/%d] %d %s",
				s_UpdatePos, s_UpdateTotal, nSize, s_UpdateCrc);

		// 准备更新
		TcpHJ212BinWrite(nSize, sData);

		if (s_UpdatePos > s_UpdateTotal)
		{
			_T("**** **** 设备升级错误: [%d/%d] %d %s",
					s_UpdatePos, s_UpdateTotal, nSize, s_UpdateCrc);
			return;
		}
		else if (s_UpdatePos == s_UpdateTotal)
		{
			_T("**** **** 设备升级完成: [%d/%d] %d %s",
					s_UpdatePos, s_UpdateTotal, nSize, s_UpdateCrc);

			ResetVersionUpdate();

			// 获取配置
			TcpHJ212_Send3021(tConnect, pSendBuffer,
					s_UpdateCrc, s_UpdatePos, BASE_DATA_SIZE, s_UpdateTotal);
		}
		else
		{
			// 接收文件
			TcpHJ212_Send3111(tConnect, pSendBuffer,
					s_UpdateCrc, s_UpdatePos, BASE_DATA_SIZE, s_UpdateTotal);
		}
	}
	// 如果长度小于0，则复制当前版本
	else
	{
		_T("**** **** 设备配置: ");

		// 版本复制
		TcpHJ212BinCopy();

		ResetVersionUpdate();

		// 获取配置
		TcpHJ212_Send3021(tConnect, pSendBuffer,
				"", s_UpdatePos, BASE_DATA_SIZE, s_UpdateTotal);
	}
}




/**
 * 接收上位机控制命令
 * 主要用于开关量，TGPOut+序号，从1开始
 */
static void OnTcpHJ212_3104(EOTConnect *tConnect, char* pCPStr, char* pSendBuffer)
{
	uint8_t i;
	char* ppArray[SIZE_128];
	uint8_t nCount = SIZE_128;
	if (EOG_SplitString(pCPStr, -1, ';', ppArray, &nCount) <= 0)
	{
		_T("HJ212错误的命令3104[%d]: %s", tConnect->channel, pCPStr);
		return;
	}

	EOTDate tDate = {0};
	EOB_Date_Get(&tDate);

	int len = TcpHJ212SendBegin(pSendBuffer, &tDate, "3104");

	int nId;
	int nSet;
	char* sKey;
	char* sVal;
	for (i=0; i<nCount; i++)
	{
		EOG_KeyValueChar(ppArray[i], '=', &sKey, &sVal);
		if (sKey != NULL && sVal != NULL)
		{
			_T("Key = %s, Value = %s", sKey, sVal);
			if (!(sKey[0] == 'T'
				&& sKey[1] == 'G'
				&& sKey[2] == 'P'
				&& sKey[3] == 'O'
				&& sKey[4] == 'u'
				&& sKey[5] == 't')) continue;

			nId = atoi(&sKey[6]);
			nSet = (int)(atof(sVal));

			nSet = F_SysSwitch_Output_Set((uint8_t)nId, (uint8_t)nSet);
			len += sprintf(&pSendBuffer[len], ";TGPOut%d=%d", nId, nSet);
		}
	}

	TcpHJ212SendEnd(pSendBuffer, tConnect->channel, len);
}


/**
 * 实时处理控制输入
 * 可能存在线程安全性问题，使用一个队列来处理
 */
static TCtrlInfo s_InputEventList[CTRL_IO_COUNT];

/**
 * 控制回调
 */
static void OnCtrlChange_Input(TDevInfo* pDevInfo, TCtrlInfo* pCtrlInfo, uint8_t nCtrlSet)
{
	int i;
	TCtrlInfo* pEvent;
	for (i=0; i<CTRL_IO_COUNT; i++)
	{
		pEvent = &s_InputEventList[i];

		// 标记事件
		if (pEvent->flag != EO_TRUE)
		{
			pEvent->flag = EO_TRUE;
			pEvent->id = pCtrlInfo->id;
			pEvent->set = nCtrlSet;

			break;
		}
	}
}


/**
 * 下发开关输入状态
 */
static void TcpHJ212_Send3103(uint8_t nChannel)
{
	uint8_t flag = EO_FALSE;
	int i;
	TCtrlInfo* pEvent;
	for (i=0; i<CTRL_IO_COUNT; i++)
	{
		pEvent = &s_InputEventList[i];
		if (pEvent->flag == EO_TRUE)
		{
			flag = EO_TRUE;
			break;
		}
	}

	// 没有事件不发送
	if (flag != EO_TRUE) return;

	char* pSendBuffer = F_Protocol_SendBuffer();

	EOTDate tDate = {0};
	EOB_Date_Get(&tDate);

	int len = TcpHJ212SendBegin(pSendBuffer, &tDate, "3103");

	for (i=0; i<CTRL_IO_COUNT; i++)
	{
		pEvent = &s_InputEventList[i];
		if (pEvent->flag == EO_TRUE)
		{
			pEvent->flag = EO_FALSE; // 翻转事件
			len += sprintf(&pSendBuffer[len], ";TGPIn%d=%d", (pEvent->id+1), pEvent->set);
		}
	}

	TcpHJ212SendEnd(pSendBuffer, nChannel, len);
}

/**
 * 处理212命令
 * s
 */
static uint8_t OnTcpHJ212(EOTConnect* tConnect, uint8_t* pData, int nLength)
{
	// ##0000CP=&&&&XXXX\r\d
	if (nLength < 20)
	{
		_T("非HJ212数据返回[%d]: <%d>", tConnect->channel, nLength);
		return EO_FALSE;
	}
	// HJ212以\r\n返回
	// 将换行符替换成结束符方便字符串操作

	if (pData[0] != '#' || pData[1] != '#')
	{
		_T("非HJ212数据返回[%d]: <%d>", tConnect->channel, nLength);
		return EO_FALSE;
	}

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
		return EO_FALSE;
	}
	char* pCPStr = p1 + 5;
	p2 = strnstr(pCPStr, "&&", nLength - 5);
	if (p2 == NULL)
	{
		_T("未找到CP结束节点");
		return EO_FALSE;
	}
	*p2 = '\0';

	// 再查找CN
	p1 = strnstr(sData, "CN=", nLength);
	if (p1 == NULL)
	{
		_T("未找到CN起始节点");
		return EO_FALSE;
	}
	char* pCNStr = p1 + 3;
	p2 = strnstr(pCNStr, ";", nLength - 3);
	if (p2 == NULL)
	{
		_T("未找到CN结束节点");
		return EO_FALSE;
	}
	*p2 = '\0';

	_T("CN: %s", pCNStr);
	//_T("CP: %s", pCPStr);

	char* pSendBuffer = F_Protocol_SendBuffer();
	if (strcmp(pCNStr, "3020") == 0)
	{
		// 配置状态
		OnTcpHJ212_3020(tConnect, pCPStr, pSendBuffer);
	}
	else if (strcmp(pCNStr, "3021") == 0)
	{
		// 配置信息
		OnTcpHJ212_3021(tConnect, pCPStr, pSendBuffer);
	}
	// 31xx自定义命令
	else if (strcmp(pCNStr, "3111") == 0)
	{
		// 升级命令
		OnTcpHJ212_3111(tConnect, pCPStr, pSendBuffer);
	}
	else if (strcmp(pCNStr, "3104") == 0)
	{
		// 控制命令
		OnTcpHJ212_3104(tConnect, pCPStr, pSendBuffer);
	}
	else if (strcmp(pCNStr, "3199") == 0)
	{
		// 重启
		EOB_SystemReset();
	}

	return EO_TRUE;
}

static void OnTcpOpen(EOTConnect *connect, int code)
{
	// 重置
	ResetVersionUpdate();
}
static int OnTcpRecv(EOTConnect* tConnect, uint8_t* pData, int nLength)
{
	OnTcpHJ212(tConnect, pData, nLength);
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

	uint8_t csq = EON_Gprs_CSQ();
	len += sprintf(&pSendBuffer[len], ";CSQ=%d", csq);

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
	_T("重置 TcpHJ212");
	EON_Tcp_SetConnect(
			pNetChannel->gprs_id,
			F_SettingGetString("svr%d.host", pNetChannel->cfg_id),
			F_SettingGetInt32("svr%d.port", pNetChannel->cfg_id),
			(EOFuncNetOpen)OnTcpOpen,
			(EOFuncNetRecv)OnTcpRecv,
			(EOFuncNetClose)OnTcpClose);

	THJ212Data* pData = &s_ListHJ212Data[pNetChannel->id];
	pData->last_minute = INT_MAX;
	pData->tick_connect = 0L;

	// 重置
	ResetVersionUpdate();
}

/**
 * 主动发送HJ212数据
 */
static void TcpHJ212_SendData(TNetChannel* pNetChannel, uint64_t tick, EOTDate* pDate, int dm)
{
	int i;
	int dp, dy, td;

	// 升级期间不发送数据
	if (s_UpdateTotal > 0)
	{
		return;
	}

	char* pSendBuffer = F_Protocol_SendBuffer();

	// 先发送实时数据
	TcpHJ212_SendDataRt(pSendBuffer, pNetChannel->gprs_id, pDate);

	// 发送统计数据
	for (i=0; i<TICK_UPDATE_MAX; i++)
	{
		td = pNetChannel->tick_delay[i];
		if (td <= 0) continue;

		dy = dm % td;
		dp = dm / td;

		_T("检查发送时间节点[%d]: (%d) %d, %d, %d", i, td, dm, dy, dp);

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

/**
 * 轮询主函数
 */
static void TcpHJ212_Update(TNetChannel* pNetChannel, uint64_t tick, EOTDate* pDate)
{
	int dm;

	EOS_Timer_Update(&s_TimerSystmReset, tick);

	// 提高控制响应时间
	if (EON_Tcp_IsConnect(pNetChannel->gprs_id))
	{
		TcpHJ212_Send3103(pNetChannel->gprs_id);
	};

	// 以0点为基点
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

	int i;
	// 重置事件
	TCtrlInfo* pCtrlInfo;
	for (i=0; i<CTRL_IO_COUNT; i++)
	{
		pCtrlInfo = &s_InputEventList[i];
		pCtrlInfo->flag = EO_FALSE;
		pCtrlInfo->id = 0x0;
		pCtrlInfo->set = CTRL_NONE;
	}
	F_SysSwitch_ChangeEvent_Add((FuncCtrlChange)OnCtrlChange_Input);

	// 请在Update中调用 EOS_Timer_Update
	EOS_Timer_Init(&s_TimerSystmReset,
			TIMER_ID_HJ212_SYSTEMRESET, 3000, TIMER_LOOP_ONCE, NULL,
			OnTimer_SystmReset);

	pNetChannel->protocol = TcpHJ212;
	pNetChannel->start_cb = TcpHJ212_Start;
	pNetChannel->update_cb = TcpHJ212_Update;

	_T("配置协议[%d]: %s", pNetChannel->cfg_id, sType);


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

	char* sForm = F_SettingGetString("svr%d.form", pNetChannel->cfg_id);
	cnt = MAX_SENSOR;
	strcpy(s, sForm); // 避免破坏性分割
	if (EOG_SplitString(s, -1, ',', (char**)ss, &cnt) <= 0)
	{
		_T("*** 网络参数设置错误 %d", cnt);
		return;
	}

	//F_ConfigAdd("svr1.form", "ST:39,a01001:1,a01002:2");
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

