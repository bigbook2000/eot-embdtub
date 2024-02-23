/*
 * eon_ec800.c
 *
 *  Created on: Jan 18, 2024
 *      Author: hjx
 * 模块使用移远4G模组芯片进行网络通讯，通过EC800测试
 * 所有命令采用异步模式，在独立的线程下运行
 * 为避免中断当前命令，URC上报命令并不立即处理，而是先缓存到队列之后统一处理
 *
 */

#include "eon_ec800.h"

#include <string.h>
#include <stdlib.h>

#include "cmsis_os.h"

#include "stm32f4xx_ll_utils.h"
#include "stm32f4xx_ll_usart.h"
#include "stm32f4xx_ll_dma.h"
#include "stm32f4xx_ll_gpio.h"

#include "eos_inc.h"
#include "eos_buffer.h"
#include "eos_timer.h"
#include "eos_list.h"

#include "eob_debug.h"
#include "eob_date.h"
#include "eob_dma.h"


#define USART_GPRS				USART6
#define DMA_GPRS				DMA2
#define DMA_STREAM_GPRS			LL_DMA_STREAM_1

#define GPIO_EC_RST 			GPIOD
#define PIN_EC_RST 				LL_GPIO_PIN_14
#define GPIO_EC_DTR 			GPIOD
#define PIN_EC_DTR 				LL_GPIO_PIN_15


// 记录模块最后一次消息时间戳，避免卡死
#define ACTIVE_TICK 			90000
static uint64_t s_LastActiveTick = 0L;

// DMA信息
static EOTDMAInfo s_GprsDMAInfo;

// 有效数据缓存，避免交换超过1000的数据
#define GPRS_DATA_SIZE	1024
D_STATIC_BUFFER_DECLARE(s_GprsDMABuffer, GPRS_DATA_SIZE)
// 二级缓存，用于检查首尾
D_STATIC_BUFFER_DECLARE(s_GprsRecvBuffer, GPRS_DATA_SIZE)
// 返回收到命令
D_STATIC_BUFFER_DECLARE(s_GprsRecvData, GPRS_DATA_SIZE)


// GPRS运行状态
#define GPRS_STATUS_NONE 				0
#define GPRS_STATUS_POWER 				1 // 正在上电
#define GPRS_STATUS_READY 				2 // 正在初始化
#define GPRS_STATUS_RUN 				5 // 正在运行
#define GPRS_STATUS_RESET 				9 // 准备重置


static uint8_t s_GprsStatus = GPRS_STATUS_NONE;
uint8_t EOB_Gprs_IsReady(void)
{
	return (s_GprsStatus == GPRS_STATUS_RUN);
}

// 消息队列
static EOTList s_ListCmdSend;

// 接收的数据长度，同时作为标识
static uint8_t s_CmdRecvChannel = 0xFF;
static int s_CmdRecvCount = 0;

// 用来处理特殊断行
static uint8_t s_CheckCmdLength = 0;
static char s_CheckCmdFlag[16];

// 串行命令，标记最后一次执行的命令
static EOTListItem* s_SemdLastItem = NULL;
EOTGprsCmd* EON_Gprs_LastCmd(void)
{
	if (s_SemdLastItem == NULL) return NULL;
	return s_SemdLastItem->data;
}

#define TIMER_ID_STATUS 14
#define TIMER_ID_CMD 	19

// 重启定时器
static EOTTimer s_TimerGprsStatus;
// 命令超时
static EOTTimer s_TimerCmdTimeout;
static void OnTimer_SystmReset(EOTTimer* tpTimer);
static void OnTimer_CmdTimeout(EOTTimer* tpTimer);


// 运营商
#define ISP_CODE_CM 1 // 移动 China Mobile
#define ISP_CODE_CU 2 // 联通 China Unicom
#define ISP_CODE_CT 3 // 电信 China Telecom
static uint8_t s_ISPCode = ISP_CODE_CM;

// 信号
static uint8_t s_CSQ = 0;
unsigned char EON_Gprs_CSQ(void)
{
	return s_CSQ;
}



// 向串口写数据
static int USARTSendData(uint8_t* pData, int nLength)
{
	int i;
	for (i=0; i<nLength; i++)
	{
		while (LL_USART_IsActiveFlag_TC(USART_GPRS) == RESET) { }
		LL_USART_TransmitData8(USART_GPRS, (uint8_t)pData[i]);
	}

	//LL_USART_ClearFlag_TC(USART_GPRS);

	return nLength;
}

// 设置回调列表
static EOTList s_ListGprsCallbackReady;
static EOTList s_ListGprsCallbackReset;
static EOTList s_ListGprsCallbackError;
static EOTList s_ListGprsCallbackCommandBefore;
static EOTList s_ListGprsCallbackCommand;
static EOTList s_ListGprsCallbackData;
static EOTList s_ListGprsCallbackEvent;

// 添加回调列表
void GprsCallbackAdd(EOTList* tpList, void* pCallback)
{
	EOTListItem* item;
	int flag = 0;
	item = tpList->_head;
	while (item != NULL)
	{
		if (item->data == pCallback)
		{
			flag = 1;
			break;
		}
	}

	if (flag == 0)
	{
		EOS_List_Push(tpList, 0, pCallback, 0);
	}
}
void GprsCallbackReady()
{
	EOTListItem* item = s_ListGprsCallbackReady._head;
	while (item != NULL)
	{
		((EOFuncGprsReady)(item->data))();
		item = item->_next;
	}
}
void GprsCallbackError(EOTGprsCmd* pCmd, int nError)
{
	EOTListItem* item = s_ListGprsCallbackError._head;
	while (item != NULL)
	{
		((EOFuncGprsError)(item->data))(pCmd, nError);
		item = item->_next;
	}
}
void GprsCallbackReset(EOTGprsCmd* pCmd)
{
	EOTListItem* item = s_ListGprsCallbackReset._head;
	while (item != NULL)
	{
		((EOFuncGprsReset)(item->data))(pCmd);
		item = item->_next;
	}
}
void GprsCallbackCommandBefore(EOTGprsCmd* pCmd)
{
	EOTListItem* item = s_ListGprsCallbackCommandBefore._head;
	while (item != NULL)
	{
		((EOFuncGprsCommandBefore)(item->data))(pCmd);
		item = item->_next;
	}
}
int GprsCallbackCommand(EOTGprsCmd* pCmd, EOTBuffer* tBuffer)
{
	int ret = tBuffer->length;

	EOTListItem* item = s_ListGprsCallbackCommand._head;
	while (item != NULL)
	{
		// 命令只能单一处理
		ret = ((EOFuncGprsCommand)(item->data))(pCmd, tBuffer);
		item = item->_next;
	}

	return ret;
}
void GprsCallbackData(uint8_t nChannel, EOTBuffer* tBuffer)
{
	EOTListItem* item = s_ListGprsCallbackData._head;
	while (item != NULL)
	{
		((EOFuncGprsData)(item->data))(nChannel, tBuffer);
		item = item->_next;
	}
}
int GprsCallbackEvent(EOTBuffer* tBuffer)
{
	int ret = tBuffer->length;

	EOTListItem* item = s_ListGprsCallbackEvent._head;
	while (item != NULL)
	{
		ret = ((EOFuncGprsEvent)(item->data))(tBuffer);
		item = item->_next;
	}

	return ret;
}


void EON_Gprs_Callback(
		EOFuncGprsReady tCallbackReady,
		EOFuncGprsReset tCallbackReset,
		EOFuncGprsError tCallbackError,
		EOFuncGprsCommandBefore tCallbackCommandBefore,
		EOFuncGprsCommand tCallbackCommand,
		EOFuncGprsData tCallbackData,
		EOFuncGprsEvent tCallbackEvent)
{
	if (tCallbackReady != NULL)
		GprsCallbackAdd(&s_ListGprsCallbackReady, (void*)tCallbackReady);

	if (tCallbackReset != NULL)
		GprsCallbackAdd(&s_ListGprsCallbackReset, (void*)tCallbackReset);

	if (tCallbackError != NULL)
		GprsCallbackAdd(&s_ListGprsCallbackError, (void*)tCallbackError);

	if (tCallbackCommandBefore != NULL)
		GprsCallbackAdd(&s_ListGprsCallbackCommandBefore, (void*)tCallbackCommandBefore);

	if (tCallbackCommand != NULL)
		GprsCallbackAdd(&s_ListGprsCallbackCommand, (void*)tCallbackCommand);

	if (tCallbackData != NULL)
		GprsCallbackAdd(&s_ListGprsCallbackData, (void*)tCallbackData);

	if (tCallbackEvent != NULL)
		GprsCallbackAdd(&s_ListGprsCallbackEvent, (void*)tCallbackEvent);
}


// 获取信号
void EON_Gprs_Cmd_CSQ(void)
{
	EON_Gprs_SendCmdPut(EOE_GprsCmd_CSQ,
			(uint8_t*)"AT+CSQ\r\n", -1,
			GPRS_MODE_AT, GPRS_MODE_AT,
			GPRS_TIME_SHORT, GPRS_TRY_ONCE, GPRS_CMD_NORMAL);
}

// 获取时钟
void EON_Gprs_Cmd_QLTS(void)
{
	EON_Gprs_SendCmdPut(EOE_GprsCmd_QLTS,
			(uint8_t*)"AT+QLTS=1\r\n", -1,
			GPRS_MODE_AT, GPRS_MODE_AT,
			GPRS_TIME_SHORT, GPRS_TRY_ONCE, GPRS_CMD_NORMAL);
}


// 直接调用串口写入数据
int EON_Gprs_DataPut(uint8_t* pData, int nLength)
{
	return USARTSendData(pData, nLength);
}

void EON_Gprs_DataGet(int nChannel, int nCount)
{
	// 直接获取指定长度的数据
	s_CmdRecvChannel = nChannel;
	s_CmdRecvCount = nCount;
}

/**
 * EC的命令都以\r\n换行符结束
 * 有时候我们需要在收到一些特殊字符的时候进行处理
 * 使用此函数可以设定断行处理的字符
 * @param sFlag 特殊字符串
 * @param nLength 特殊字符串长度，如果是0表示关闭监测
 */
void EON_Gprs_CheckCmdFlag(char* sFlag, int nLength)
{
	s_CheckCmdLength = nLength;
	if (s_CheckCmdLength > 0)
	{
		strncpy(s_CheckCmdFlag, sFlag, nLength);
		s_CheckCmdFlag[nLength] = '\0';
	}
}


// 释放回调，清除挂载数据
static void GprsCmdFree(EOTList* tpList, EOTListItem* tpItem)
{
	EOTGprsCmd *cmd = tpItem->data;
	if (cmd->send_mode == GPRS_MODE_AT)
	{
		// 如果是复制的命令，清除
		uint8_t* pCmdTextAlloc = cmd->cmd_data;
		if (pCmdTextAlloc != NULL)
		{
			//_D("[xFree:%08X]\r\n", (uint32_t)pCmdTextAlloc);
			vPortFree(pCmdTextAlloc);
		}
	}

	if (cmd != NULL)
	{
		//_D("[xFree:%08X]\r\n", (uint32_t)cmd);
		vPortFree(cmd);
	}
}

/**
 * 插入命令
 * @param nCmdId 命令标识
 *
 * @param sCmdData 如果命令模式是AT，先拷贝命令，如果是DATA，直接赋值
 * @param nCmdLen 如果命令长度，如果命令模式是AT，重新计算
 *
 * @param nSendMode 命令模式，如果是AT，则表示是字符串，需要拷贝命令，如果是DATA，直接赋值
 * @param nRecvMode 接收命令返回，如果是AT命令，需要校验是否返回OK
 *
 * @param nTimeout 重试间隔，等待时长，超时则认为失败
 * @param nTryCount 重试次数，如果失败，则重新发送 *
 * @param nReset 失败之后是否重启模块
 *
 */
EOTGprsCmd* EON_Gprs_SendCmdPut(
		EOEGprsCmd nCmdId,
		uint8_t* pCmdData, int16_t nCmdLen,
		int8_t nSendMode, int8_t nRecvMode,
		int16_t nTimeout, int8_t nTryCount, int8_t nReset)
{
	_T("HeapSize = %u", xPortGetFreeHeapSize());

	// 从堆上分配空间
	int len = nCmdLen;

	if (nSendMode == GPRS_MODE_AT)
	{
		// 先拷贝命令
		const char* sCmdText = (const char*)pCmdData;
		len = strlen(sCmdText) + 1;
		char* pCmdTextAlloc = pvPortMalloc(len);
		if (pCmdTextAlloc == NULL)
		{
			_D("*** xAlloc NULL");
			return NULL;
		}

		strcpy((char*)pCmdTextAlloc, sCmdText);
		pCmdTextAlloc[len - 1] = '\0';

		pCmdData = (uint8_t*)pCmdTextAlloc;
	}

	EOTGprsCmd *cmd = pvPortMalloc(sizeof(EOTGprsCmd));
	if (cmd == NULL)
	{
		_D("*** xAlloc NULL");
		return NULL;
	}

	cmd->id = nCmdId;
	cmd->tag = 0;
	cmd->send_mode = nSendMode;
	cmd->recv_mode = nRecvMode;
	cmd->reset = nReset;

	cmd->delay_max = nTimeout;
	cmd->try_max = nTryCount;

	cmd->cmd_data = pCmdData;
	cmd->cmd_len = len;

	// osWaitForever
	//osMessagePut(s_MessageGprsCmdHandle, (uint32_t)&cmd, GPRS_TIME_CMD);
	EOS_List_Push(&s_ListCmdSend, nCmdId, cmd, len);

	return cmd;
}

/**
 * 清除命令
 * 处理完成之后必须调用清理
 */
void EON_Gprs_SendCmdPop(void)
{
	if (s_SemdLastItem == NULL) return;

	// 释放
	EOS_List_ItemFree(&s_ListCmdSend, s_SemdLastItem, (EOFuncListItemFree)GprsCmdFree);

	// 重置
	s_SemdLastItem = NULL;
}

// 配置APN
void EON_Gprs_Cmd_QICSGP(char* sApn, char* sUser, char* sPassword)
{
	// APN
	char sCmdText[GPRS_CMD_SIZE];
	sprintf(sCmdText, "AT+QICSGP=1,1,\"%s\",\"%s\",\"%s\",1\r\n", sUser, sUser, sPassword);

	EON_Gprs_SendCmdPut(
			EOE_GprsCmd_QICSGP,
			(uint8_t*)sCmdText, -1,
			GPRS_MODE_AT, GPRS_MODE_AT,
			GPRS_TIME_LONG, GPRS_TRY_MORE, GPRS_CMD_RESET);
}


// 重置GPRS模块
// 拉低有效
void EON_Gprs_Power(void)
{
	_T("Reset GPRS");

	s_LastActiveTick = 0L;

	// 清除接收缓存
	EOS_Buffer_Clear(&s_GprsDMABuffer);
	EOS_Buffer_Clear(&s_GprsRecvData);

	// 命令标识复位
	EON_Gprs_SendCmdPop();

	EOS_List_Clear(&s_ListCmdSend, (EOFuncListItemFree)GprsCmdFree);

	osDelay(500);

	LL_GPIO_ResetOutputPin(GPIO_EC_RST, PIN_EC_RST);

	// 拉低2s
	uint8_t i;
	for (i=0; i<4; i++)
	{
		_T("..");
		osDelay(500);
	}
	LL_GPIO_SetOutputPin(GPIO_EC_RST, PIN_EC_RST);

	_T("OK");

	for (i=0; i<4; i++)
	{
		_T("..");
		osDelay(500);
	}
	_T("OK");
	//LL_mDelay(500);

	s_GprsStatus = GPRS_STATUS_READY;
}

// 重启整个系统
void EON_Gprs_Reset(void)
{
	if (s_GprsStatus == GPRS_STATUS_RESET) return;
	s_GprsStatus = GPRS_STATUS_RESET;

	_T("系统重置开始\r\n");

	EOTGprsCmd* pCmd = NULL;
	if (s_SemdLastItem != NULL) pCmd = s_SemdLastItem->data;
	GprsCallbackReset(pCmd);

	EOS_Timer_StartCall(&s_TimerGprsStatus, 5000, NULL, OnTimer_SystmReset);
}

static int OnGprsCmd_AT(EOTBuffer* tBuffer)
{
	if (CHECK_STR("OK"))
	{
		// 关闭回显
		EON_Gprs_SendCmdPut(EOE_GprsCmd_ATE0, (uint8_t*)"ATE0\r\n",
				-1, GPRS_MODE_AT, GPRS_MODE_AT, GPRS_TIME_NORMAL, GPRS_TRY_NORMAL, GPRS_CMD_RESET);

		return 0;
	}

	return tBuffer->length;
}

static int OnGprsCmd_ATE0(EOTBuffer* tBuffer)
{
	if (CHECK_STR("ATE0"))
	{
		// 获取SIM卡状态
		EON_Gprs_SendCmdPut(EOE_GprsCmd_CPIN, (uint8_t*)"AT+CPIN?\r\n", -1,
				GPRS_MODE_AT, GPRS_MODE_AT, GPRS_TIME_LONG, GPRS_TRY_MORE, GPRS_CMD_RESET);
		return 0;
	}

	return tBuffer->length;
}

// 获取SIM卡状态
static int OnGprsCmd_CPIN(EOTBuffer* tBuffer)
{
	if (CHECK_STR("OK"))
	{
		// 用于返回 ME 的国际移动设备识别码（IMEI 号）
		EON_Gprs_SendCmdPut(EOE_GprsCmd_CGSN, (uint8_t*)"AT+CGSN\r\n", -1,
				GPRS_MODE_AT, GPRS_MODE_AT, GPRS_TIME_NORMAL, GPRS_TRY_MORE, GPRS_CMD_RESET);

		return 0;
	}

	return tBuffer->length;
}

// SIM卡IMEI
static int OnGprsCmd_CGSN(EOTBuffer* tBuffer)
{
	if (CHECK_STR("OK"))
	{
		// 查询ICCID
		EON_Gprs_SendCmdPut(EOE_GprsCmd_CCID, (uint8_t*)"AT+CCID\r\n", -1,
				GPRS_MODE_AT, GPRS_MODE_AT, GPRS_TIME_NORMAL, GPRS_TRY_NORMAL, GPRS_CMD_RESET);
		return 0;
	}

	// 865447060395941
	if (tBuffer->length < 15) return tBuffer->length;

	char* s = (char*)tBuffer->buffer;
	if (EOG_CheckNumber(s, tBuffer->length))
	{
		_T("SIM IMEI: %s", (const char*)s);
//
//		// 查询ICCID
//		EOB_Gprs_SendCmdPut(EOE_GprsCmd_CCID, (uint8_t*)"AT+CCID\r\n", -1,
//				GPRS_MODE_AT, GPRS_MODE_AT, GPRS_TIME_NORMAL, GPRS_TRY_NORMAL, GPRS_CMD_RESET);
//		return 0;
	}

	return tBuffer->length;
}

// SIM卡ICCID
static int OnGprsCmd_CCID(EOTBuffer* tBuffer)
{
	if (CHECK_STR("OK"))
	{
		// 全功能模式
		// 0 最小功能模式
		// 1 全功能模式
		// 3 禁用 ME 接收射频信号
		// 4 禁用 ME 发送和接收射频信号功能
		// 5 禁用(U)SIM
		EON_Gprs_SendCmdPut(EOE_GprsCmd_CFUN_1, (uint8_t*)"AT+CFUN=1\r\n", -1,
				GPRS_MODE_AT, GPRS_MODE_AT, GPRS_TIME_NORMAL, GPRS_TRY_NORMAL, GPRS_CMD_RESET);

		return 0;
	}

	char iccidStr[GPRS_CMD_SIZE];
	if (NCHECK_STR(iccidStr, "+CCID: ", NULL))
		return tBuffer->length;

	_T("SIM ICCID: %s\r\n", iccidStr);

	return tBuffer->length;
}

// 关闭GSM
static int OnGprsCmd_CFUN_0(EOTBuffer* tBuffer)
{
	return 0;
}

// 打开GSM
static int OnGprsCmd_CFUN_1(EOTBuffer* tBuffer)
{
	if (CHECK_STR("OK"))
	{
		// 查询运营商
		EON_Gprs_SendCmdPut(EOE_GprsCmd_COPS, (uint8_t*)"AT+COPS?\r\n", -1,
				GPRS_MODE_AT, GPRS_MODE_AT, GPRS_TIME_LONG, GPRS_TRY_NORMAL, GPRS_CMD_RESET);

		return 0;
	}

	return tBuffer->length;
}

// 运营商
static int OnGprsCmd_COPS(EOTBuffer* tBuffer)
{
	char ispStr[GPRS_CMD_SIZE];
	if (NCHECK_STR(ispStr, "\"", "\""))
		return tBuffer->length;

	// if (strcasecmp(copStr, "CHINA MOBILE") == 0)

	if (strcasecmp(ispStr, "CHINA UNICOM") == 0)
		s_ISPCode = ISP_CODE_CU;
	else if (strcasecmp(ispStr, "CHINA TELECOM") == 0)
		s_ISPCode = ISP_CODE_CT;
	else
		s_ISPCode = ISP_CODE_CM;

	_T("Cops: %s [%d]\r\n", ispStr, s_ISPCode);

	// 注册PS业务
	EON_Gprs_SendCmdPut(EOE_GprsCmd_CGREG, (uint8_t*)"AT+CGREG?\r\n", -1,
			GPRS_MODE_AT, GPRS_MODE_AT, GPRS_TIME_LONG, GPRS_TRY_MORE, GPRS_CMD_RESET);

	return 0;
}

// GPRS注册状态
// 0 未注册；ME 当前未搜索要注册的运营商
// 1 已注册，归属地网络
// 2 未注册，ME 正在搜索要注册的运营商
// 3 注册被拒绝
// 4 未知状态
// 5 已注册，漫游网络
static int OnGprsCmd_CGREG(EOTBuffer* tBuffer)
{
	if (CHECK_STR("OK"))
	{
		// 配置APN接入点
		EON_Gprs_Cmd_QICSGP("CMNET", "", "");

		return 0;
	}

	//+CGREG: 0,1
	char ret[8];
	if (NCHECK_STR(ret, ",", NULL))
		return tBuffer->length;

	uint8_t val = (ret[0] - '0');
	if (val == 1 || val == 5)
	{
		_T("Register GPRS\r\n");
	}

	return tBuffer->length;
}

static int OnGprsCmd_QICSGP(EOTBuffer* tBuffer)
{
	if (CHECK_STR("OK"))
	{
		// 激活PDP场景
		EON_Gprs_SendCmdPut(EOE_GprsCmd_QIACT, (uint8_t*)"AT+QIACT=1\r\n", -1,
				GPRS_MODE_AT, GPRS_MODE_AT, GPRS_TIME_LONG, GPRS_TRY_NORMAL, GPRS_CMD_RESET);

		return 0;
	}

	return tBuffer->length;
}

static int OnGprsCmd_QIACT(EOTBuffer* tBuffer)
{
	if (CHECK_STR("OK"))
	{
		// 获取IP地址
		EON_Gprs_SendCmdPut(EOE_GprsCmd_CGPADDR, (uint8_t*)"AT+CGPADDR=1\r\n", -1,
				GPRS_MODE_AT, GPRS_MODE_AT, GPRS_TIME_NORMAL, GPRS_TRY_NORMAL, GPRS_CMD_RESET);

		return 0;
	}

	return tBuffer->length;
}

// IP地址
static int OnGprsCmd_CGPADDR(EOTBuffer* tBuffer)
{
	// +CGPADDR: 1,"10.35.162.137"

	char ipStr[32];
	if (NCHECK_STR(ipStr, "\"", "\""))
		return tBuffer->length;

	_T("IP Address: %s\r\n", ipStr);

	// 获取IP是初始化最后一步
	// 转入数据传输阶段
	// 网络运行正常
	s_GprsStatus = GPRS_STATUS_RUN;

	GprsCallbackReady();

	// 初始信号
	EON_Gprs_Cmd_CSQ();

	// 提取时间
	EON_Gprs_Cmd_QLTS();

	return 0;
}

static int OnGprsCmd_QIDEACT(EOTBuffer* tBuffer)
{
	// 重新走流程

	if (CHECK_STR("OK"))
	{
		// 获取SIM卡状态
		EON_Gprs_SendCmdPut(EOE_GprsCmd_CPIN, (uint8_t*)"AT+CPIN?\r\n", -1,
				GPRS_MODE_AT, GPRS_MODE_AT, GPRS_TIME_LONG, GPRS_TRY_MORE, GPRS_CMD_RESET);

		return 0;
	}

	return tBuffer->length;
}

/**
 * 初始化运行
 * 如果命令已处理，返回0，否则返回-1
 */
static int OnGprsStatus_Ready(EOTGprsCmd* pCmd, EOTBuffer* tBuffer)
{
	int nCmdId = pCmd->id;
	// 状态机
	switch (nCmdId)
	{
		case EOE_GprsCmd_AT:
			return OnGprsCmd_AT(tBuffer);
		case EOE_GprsCmd_ATE0:
			return OnGprsCmd_ATE0(tBuffer);

		// SIM卡状态
		case EOE_GprsCmd_CPIN:
			return OnGprsCmd_CPIN(tBuffer);
		// SIM卡IMEI
		case EOE_GprsCmd_CGSN:
			return OnGprsCmd_CGSN(tBuffer);
		// SIM卡ICCID
		case EOE_GprsCmd_CCID:
			return OnGprsCmd_CCID(tBuffer);
		// 关闭GSM
		case EOE_GprsCmd_CFUN_0:
			return OnGprsCmd_CFUN_0(tBuffer);
		// 打开GSM
		case EOE_GprsCmd_CFUN_1:
			return OnGprsCmd_CFUN_1(tBuffer);

		// 运营商
		case EOE_GprsCmd_COPS:
			return OnGprsCmd_COPS(tBuffer);
		// GPRS注册状态
		case EOE_GprsCmd_CGREG:
			return OnGprsCmd_CGREG(tBuffer);

		// 移动APN接入点
		case EOE_GprsCmd_QICSGP:
			return OnGprsCmd_QICSGP(tBuffer); // 激活PDP场景
		// 激活PDP
		case EOE_GprsCmd_QIACT:
			return OnGprsCmd_QIACT(tBuffer);
		// 取消激活PDP
		case EOE_GprsCmd_QIDEACT:
			return OnGprsCmd_QIDEACT(tBuffer);
		// IP地址
		case EOE_GprsCmd_CGPADDR:
			return OnGprsCmd_CGPADDR(tBuffer);
	}

	return -1;
}

// 网络链接状态
static int OnGprsCmd_CIPSTATUS(EOTBuffer* tBuffer)
{
	return 0;
}

// DNS
static int OnGprsCmd_CDNSGIP(EOTBuffer* tBuffer)
{
	return 0;
}


// 时钟
static int OnGprsCmd_QLTS(EOTBuffer* tBuffer)
{
	// +QLTS: "2022/10/28,03:17:06+32,0"

	char dateStr[64];
	if (NCHECK_STR(dateStr, "\"", "\"")) return tBuffer->length;

	uint8_t len = strlen(dateStr);
	if (len < 24) return tBuffer->length;

	char tzf = dateStr[19];

	dateStr[4] = '\0';
	dateStr[7] = '\0';
	dateStr[10] = '\0';
	dateStr[13] = '\0';
	dateStr[16] = '\0';
	dateStr[19] = '\0';
	dateStr[22] = '\0';

	EOTDate tDate;
	tDate.year = atoi(&dateStr[0]) - YEAR_ZERO;
	tDate.month = atoi(&dateStr[5]);
	tDate.date = atoi(&dateStr[8]);

	tDate.hour = atoi(&dateStr[11]);
	tDate.minute = atoi(&dateStr[14]);
	tDate.second = atoi(&dateStr[17]);

	int8_t tz = atoi(&dateStr[20]) >> 2;
	if (tzf == '-') tz = -tz;

	EOB_Date_AddTime(&tDate, tz, 0, 0);

	// 设置RTC
	EOB_Date_Set(&tDate);

	EOB_Date_Get(&tDate);
	printf("DateTime: %04d-%02d-%02d %02d:%02d:%02d\r\n",
			YEAR_ZERO + tDate.year, tDate.month, tDate.date, tDate.hour, tDate.minute, tDate.second);

	return 0;
}

// 信号
static int OnGprsCmd_CSQ(EOTBuffer* tBuffer)
{
	// +CSQ: 16,99
	char ret[8];
	if (NCHECK_STR(ret, "+CSQ: ", ",")) return tBuffer->length;

	s_CSQ = atoi(ret);
	_T("信号强度: %d", s_CSQ);

	return 0;
}

// 处理事件
static int GprsEventDispacher(EOTBuffer* tBuffer)
{
	// +QIURC: "closed",<connectID><CR><LF>
	// +QIURC: "recv",<connectID>,<currentrecvlength><CR><LF><data>
	// +QIURC: "pdpdeact",<contextID><CR><LF>

	char qiurcStr[GPRS_CMD_SIZE];
	if (NCHECK_STR(qiurcStr, "+QIURC: ", NULL)) return -1;

	_T("GPRS事件: %s", qiurcStr);

	int ret = 0;
	if (strstr(qiurcStr, "pdpdeact"))
	{
		// 反激活PDP场景
		EON_Gprs_SendCmdPut(EOE_GprsCmd_QIDEACT, (uint8_t*)"AT+QIDEACT=1\r\n", -1,
				GPRS_MODE_AT, GPRS_MODE_AT, GPRS_TIME_LONG, GPRS_TRY_NORMAL, GPRS_CMD_RESET);
	}
	else
	{
		ret = GprsCallbackEvent(tBuffer);
	}

	return ret;
}

/**
 * 网路运行
 * 命令正常处理返回0，否则返回-1
 */
static int OnGprsStatus_Run(EOTGprsCmd* pCmd, EOTBuffer* tBuffer)
{
	if (pCmd == NULL) return 0;
	int nCmdId = pCmd->id;

	// 状态机
	switch (nCmdId)
	{
		// 网络链接状态
		case EOE_GprsCmd_CIPSTATUS:
			return OnGprsCmd_CIPSTATUS(tBuffer);

		// DNS
		case EOE_GprsCmd_CDNSGIP:
			return OnGprsCmd_CDNSGIP(tBuffer);

		// 时钟
		case EOE_GprsCmd_QLTS:
			return OnGprsCmd_QLTS(tBuffer);

		// 信号
		case EOE_GprsCmd_CSQ:
			return OnGprsCmd_CSQ(tBuffer);
	}

	return GprsCallbackCommand(pCmd, tBuffer);
}

/**
 * 处理命令
 */
static int GprsParseCmd(EOTGprsCmd* pCmd, EOTBuffer* tBuffer)
{
	int ret = -1;

	// 处理事件
	// 如果非事件，返回-1
	ret = GprsEventDispacher(tBuffer);
	if (ret >= 0) return ret;

	// 跳过
	if (pCmd == NULL) return 0;

	switch (s_GprsStatus)
	{
		case GPRS_STATUS_READY:
			ret = OnGprsStatus_Ready(pCmd, tBuffer);
			break;
		case GPRS_STATUS_RUN:
			ret = OnGprsStatus_Run(pCmd, tBuffer);
			break;
	}

	// 如果命令返回-1则继续等待，0表示已命令已结束，大于0，表示需要清空
	if (ret == 0)
	{
		EON_Gprs_SendCmdPop();
	}

	return ret;
}

/**
 * 处理每一行命令
 */
static int GprsParseDataLine(int nLine, EOTGprsCmd* pCmd, EOTBuffer* tRecvBuffer)
{
	int ret = -1;

	// 直接接收数据模式
	if (s_CmdRecvCount > 0)
	{
		_Tmb(s_GprsRecvBuffer.buffer, s_GprsRecvBuffer.length);
		_T("接收数据: %d/%d", s_CmdRecvCount, tRecvBuffer->length);
		if (s_CmdRecvCount > tRecvBuffer->length)
		{
			// 数量不足，继续等待
			return -1;
		}

		// 取出指定长度的数据
		ret = EOS_Buffer_Pop(tRecvBuffer, (char*)s_GprsRecvData.buffer, s_CmdRecvCount);
		if (ret < 0) return -1;
		s_GprsRecvData.length = ret;

		GprsCallbackData(s_CmdRecvChannel, &s_GprsRecvData);

		// 数据接收完成，自动重置
		s_CmdRecvCount = 0;

		// EOS_Buffer_Pop已经取出数据，无需返回长度，继续下一行命令
		return 0;
	}
	else
	{
		// 查找换行标识，只有AT命令返回换行符
		if (s_GprsRecvBuffer.length < 2) return -1;

		_Tmb(s_GprsRecvBuffer.buffer, s_GprsRecvBuffer.length);
		// 每次读取1行数据，buffer不包含换行符，ret包含换行符
		ret = EOS_Buffer_PopReturn(&s_GprsRecvBuffer, (char*)s_GprsRecvData.buffer, s_GprsRecvData.size);
		if (ret < 0)
		{
			// 特别处理发送标识，不带换行符
			if (s_CheckCmdLength > 0)
			{
				_T("查找特殊断行符: [%d]%s", s_CheckCmdLength, s_CheckCmdFlag);
				ret = EOS_Buffer_PopFlag(&s_GprsRecvBuffer, NULL, s_CheckCmdFlag);
				if (ret < 0) return -1;

				strcpy((char *)s_GprsRecvData.buffer, s_CheckCmdFlag);
				s_GprsRecvData.length = s_CheckCmdLength;
			}
			else
			{
				return -1;
			}
		}
		else
		{
			// 计数包含换行符
			s_GprsRecvData.length = ret - 2;
		}

		_T("----<[%d/%d] %s", nLine, s_GprsRecvData.length, (const char*)s_GprsRecvData.buffer);

		// 空行不处理
		if (s_GprsRecvData.length <= 0) return 2;

		// 如果返回大于0，表示已处理，但不是结束，需要继续
		ret = GprsParseCmd(pCmd, &s_GprsRecvData);
		_T("处理命令结果[%d]: %d", nLine, ret);
	}

	return ret;
}

// 响应Gprs命令
static void GprsRecvProcess(uint64_t tick)
{
	int i;
	// 调用DMA
	// DMARecvBuffer();
	if (s_GprsDMABuffer.length >= GPRS_DATA_SIZE)
	{
		// 数据太多了
		EOS_Buffer_Clear(&s_GprsDMABuffer);
	}

	int ret = EOB_DMA_Recv(&s_GprsDMAInfo);
	if (ret < 0)
	{
		_T("**** DMA Error [%d] %d - %d",
				s_GprsDMAInfo.data->length, s_GprsDMAInfo.retain, s_GprsDMAInfo.retain_last);
	}

	if (ret == 0) return;

	// 标记时间戳
	s_LastActiveTick = tick;

	// 取出DMA缓存，放入二级命令缓存
	EOS_Buffer_PushBuffer(&s_GprsRecvBuffer, &s_GprsDMABuffer);

	// 检查命令行是否存在指定标识
	// OK、ERROR 或+CME ERROR: <err>
	// <CR><LF>OK<CR><LF>
	// 在返回标识之前会返回响应消息

	// 最后执行的命令
	EOTGprsCmd* pCmd = NULL;
	if (s_SemdLastItem != NULL)
	{
		pCmd = s_SemdLastItem->data;
	}

	// 限制每次最大循环次数
	for (i=0; i<16; i++)
	{
		// 如果大于0，清除当前行继续
		if (GprsParseDataLine(i, pCmd, &s_GprsRecvBuffer) <= 0) break;
	}
}

static void GprsSendData(EOTGprsCmd* pCmd)
{
	// 先清空命令接收数据
	EOS_Buffer_Clear(&s_GprsDMABuffer);
	EOS_Buffer_Clear(&s_GprsRecvData);

	if (pCmd->send_mode == GPRS_MODE_AT)
	{
		const char* sCmdText = (const char*)pCmd->cmd_data;
		_T("---->[%d]%s", pCmd->cmd_len, sCmdText);
	}
	else
	{
		_T("---->[%d] HEX", pCmd->cmd_len);
	}

	// 事件回调
	GprsCallbackCommandBefore(pCmd);

	// 向串口发送命令
	USARTSendData(pCmd->cmd_data, pCmd->cmd_len);
}

/**
 * 实际向串口发送命令
 */
static void GprsSendProcess(uint64_t tick)
{
	// 接收数据时不要发送
	if (s_CmdRecvCount > 0) return;

	// 超时0，立即返回
//	osEvent event = osMessageGet(s_MessageGprsCmdHandle, 0);
//	if (event.status != osEventMessage) return;
//	EOTGprsCmd *cmd = event.value.p;

	// 串行命令，命令未处理完成之前不能发送新的命令
	if (s_SemdLastItem != NULL) return;

	// 取出最先的新命令
	s_SemdLastItem = EOS_List_Pop(&s_ListCmdSend);
	if (s_SemdLastItem == NULL) return;

	EOTGprsCmd* pCmd = s_SemdLastItem->data;
	if (pCmd == NULL) return;

	// 重置计数器
	EOS_Timer_StartDelay(&s_TimerCmdTimeout, pCmd->delay_max, pCmd->try_max, NULL);

	GprsSendData(pCmd);
}


void EON_Gprs_Init(void)
{
	_T("GPRS初始化...");

	// 初始化定时器
	EOS_Timer_Init(&s_TimerGprsStatus, TIMER_ID_STATUS, 5000, TIMER_LOOP_ONCE, NULL, NULL);
	EOS_Timer_Init(&s_TimerCmdTimeout, TIMER_ID_CMD, 30000, TIMER_LOOP_ONCE, NULL, (EOFuncTimer)OnTimer_CmdTimeout);

	// 初始化缓存
	D_STATIC_BUFFER_INIT(s_GprsDMABuffer, GPRS_DATA_SIZE);
	D_STATIC_BUFFER_INIT(s_GprsRecvBuffer, GPRS_DATA_SIZE);
	D_STATIC_BUFFER_INIT(s_GprsRecvData, GPRS_DATA_SIZE);

	//osPoolDef(PoolGprsCmd, 32, EOTGprsCmd);
	//s_PoolGprsCmdHandle = osPoolCreate(osPool(PoolGprsCmd));
	_T("HeapSize = %u", xPortGetFreeHeapSize());

//	// 启动中断
//	NVIC_SetPriority(USART3_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0));
//	NVIC_EnableIRQ(USART3_IRQn);
//
//	// 手动启动RXNE
//	LL_USART_EnableIT_RXNE(USART_GPRS);

//	// 配置DMA
//	LL_DMA_SetMemoryAddress(DMA_GPRS, DMA_STREAM_GPRS, (uint32_t)s_GprsDMABufer);
//	LL_DMA_SetDataLength(DMA_GPRS, DMA_STREAM_GPRS, GPRS_DATA_SIZE);
//	LL_DMA_SetPeriphAddress(DMA_GPRS, DMA_STREAM_GPRS, (uint32_t)&(USART_GPRS->DR));
//	LL_DMA_EnableStream(DMA_GPRS, DMA_STREAM_GPRS);

	// 配置DMA
	EOB_DMA_Recv_Init(&s_GprsDMAInfo,
			DMA_GPRS, DMA_STREAM_GPRS, (uint32_t)&(USART_GPRS->DR),
			&s_GprsDMABuffer);

	// 串口接收DMA
	LL_USART_EnableDMAReq_RX(USART_GPRS);

	//printf("%u\r\n", xPortGetFreeHeapSize());

	// 创建命令队列
	EOS_List_Create(&s_ListCmdSend);

	_T("OK");

	osDelay(50);

	EON_Gprs_Power();

	// 第一步发送AT命令
	EON_Gprs_SendCmdPut(EOE_GprsCmd_AT, (uint8_t*)"AT\r\n", -1,
			GPRS_MODE_AT, GPRS_MODE_AT, GPRS_TIME_LONG, GPRS_TRY_MORE, GPRS_CMD_RESET);
}

// 独立GPRS线程处理
void EON_Gprs_Update(uint64_t tick)
{
	if (s_LastActiveTick > 0L && (tick - s_LastActiveTick) > ACTIVE_TICK)
	{
		_T("*** 模块长时间无响应");
		EON_Gprs_Power();
		return;
	}

	GprsRecvProcess(tick);
	GprsSendProcess(tick);

	EOS_Timer_Update(&s_TimerGprsStatus, tick);
	EOS_Timer_Update(&s_TimerCmdTimeout, tick);
}


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
 * 监测命令超时
 */
static void OnTimer_CmdTimeout(EOTTimer* tpTimer)
{
	if (s_SemdLastItem == NULL) return;
	EOTGprsCmd* pCmd = s_SemdLastItem->data;
	if (pCmd == NULL) return;

	if (EOS_Timer_IsLast(tpTimer))
	{
		// 关键命令重启
		if (pCmd->reset == GPRS_CMD_RESET)
		{
			EON_Gprs_Reset();
		}
		else
		{
			// 响应错误，只处理非关键命令
			GprsCallbackError(pCmd, 0);

			// 重置命令
			EON_Gprs_SendCmdPop();
		}
	}
	else
	{
		_T("命令重试\r\n");
		GprsSendData(pCmd);
	}
}

