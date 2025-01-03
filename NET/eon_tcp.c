/*
 * eon_tcp.c
 *
 *  Created on: Jun 26, 2023
 *      Author: hjx
 */

#include "eon_tcp.h"

#include <string.h>
#include <stdlib.h>

#include "cmsis_os.h"

#include "eos_inc.h"
#include "eos_list.h"
#include "eob_debug.h"
#include "eob_tick.h"

#include "eon_ec800.h"


// 每次最大读取1K
#define TCP_RECV_MAX 	1024

// 字节数据太长返回失败，将大包拆小包分批发送
// 最大发送包
#define TCP_SEND_MAX1 	1600
#define TCP_SEND_MAX2 	2400


// 发送数据队列
static EOTList s_ListSendData;
static uint32_t s_SendDataId = 0;

EOTConnect s_TcpConnectList[MAX_TCP_CONNECT];

uint8_t EON_Tcp_IsConnect(uint8_t nChannel)
{
	return (s_TcpConnectList[nChannel].status == STATUS_OPENED);
}

EOTConnect* EON_Tcp_GetConnect(uint8_t nChannel)
{
	return &s_TcpConnectList[nChannel];
}

/**
 * 设置连接参数
 */
EOTConnect* EON_Tcp_SetConnect(uint8_t nChannel,
		char* sHost, uint16_t nPort,
		EOFuncNetOpen tCallbackOpen,
		EOFuncNetRecv tCallbackRecv,
		EOFuncNetClose tCallbackClose)
{
	EOTConnect* hConnect = &s_TcpConnectList[nChannel];

	strncpy(hConnect->host, sHost, HOST_LENGTH);
	hConnect->port = nPort;

	// 设置回调
	hConnect->open_cb = tCallbackOpen;
	hConnect->recv_cb = tCallbackRecv;
	hConnect->close_cb = tCallbackClose;

	return hConnect;
}

// 释放回调，清除挂载数据
static void TcpSendDataFree(EOTList* tpList, EOTListItem* tpItem)
{
	char *pSendData = tpItem->data;
	if (pSendData != NULL)
	{
		//_D("[xFree:%08X]\r\n", (uint32_t)cmd);
		vPortFree(pSendData);
	}
}

// TCP连接
static int OnTcpCmd_QIOPEN(EOTGprsCmd* pCmd, EOTBuffer* tBuffer)
{
	// QIOPEN指令先返回OK，后返回结果

	//+QIOPEN: 0,563
	char sCmdText[GPRS_CMD_SIZE];
	if (NCHECK_STR(sCmdText, "+QIOPEN: ", NULL))
		return tBuffer->length;

	int nChannel = -1;
	int nError = 0;
	char* ppArray[8];
	uint8_t nCount = 0;

	if (EOG_SplitString(sCmdText, -1, ',', ppArray, &nCount) == 2)
	{
		nChannel = atoi(ppArray[0]);
		nError = atoi(ppArray[1]);

		_T("TCP连接响应[%d]: %d", nChannel, nError);
		if (nChannel < 0 || nChannel >= MAX_TCP_CONNECT)
		{
			_T("*** 错误的连接通道[%d]", nChannel);
			return 0;
		}

		// Socket标识被占用
		if (nError == 563)
		{
			_T("*** 连接通道被占用[%d]", nChannel);
			EON_Tcp_Close(nChannel);
		}

		EOTConnect* hConnect = &s_TcpConnectList[nChannel];
		if (nError == 0) hConnect->status = STATUS_OPENED;

		hConnect->open_cb(hConnect, nError);
	}

	return 0;
}

// 处理关闭命令（手动）
static int OnTcpCmd_QICLOSE(EOTGprsCmd* pCmd, EOTBuffer* tBuffer)
{
	EOTConnect* hConnect;
	if (CHECK_STR("OK"))
	{
		if (pCmd->channel >= MAX_TCP_CONNECT)
		{
			_T("*** 错误的连接通道[%d]", pCmd->channel);
			return 0;
		}

		hConnect = &s_TcpConnectList[pCmd->channel];

		if (hConnect->status != STATUS_NONE)
		{
			// 回调，注意不要循环调用
			hConnect->close_cb(hConnect, 0);
		}

		// 处理状态
		hConnect->status = STATUS_NONE;

		return 0;
	}

	return tBuffer->length;
}

// 发送数据命令
static int OnTcpCmd_QISEND(EOTGprsCmd* pCmd, EOTBuffer* tBuffer)
{
	if (CHECK_STR(">"))
	{
		// 关闭断行符
		EON_Gprs_CheckCmdFlag("", 0);

		// 实际传输数据
		EOTListItem* item = EOS_List_Pop(&s_ListSendData);
		if (item == NULL)
		{
			_D("未找到对应的数据");
			return tBuffer->length;
		}

		_T("OnTcpCmd_QISEND[%d]: SEND %d/%d", s_ListSendData.count, (int)pCmd->tag, (int)item->length);

		// 立即传输数据
		// 发送的时候是DATA，回来是OK
		EON_Gprs_DataPut(item->data, item->length);

		// 等发送完成之后，必须释放原始数据
		EOS_List_ItemFree(&s_ListSendData, item, (EOFuncListItemFree)TcpSendDataFree);
	}
	else if (CHECK_STR("SEND OK"))
	{
		// 返回0结束命令
		return 0;
	}
	else if (CHECK_STR("ERROR"))
	{
		// ERROR 发送错误
		// 关闭断行符
		EON_Gprs_CheckCmdFlag("", 0);

		// 实际传输数据
		EOTListItem* item = EOS_List_Pop(&s_ListSendData);
		if (item == NULL)
		{
			_D("未找到对应的数据");
			return tBuffer->length;
		}

		_T("OnTcpCmd_QISEND[%d]: EORROR %d/%d, Length = %d",
				pCmd->channel, (int)pCmd->tag, (int)item->length,
				s_ListSendData.count);

		// 必须释放原始数据
		EOS_List_ItemFree(&s_ListSendData, item, (EOFuncListItemFree)TcpSendDataFree);

		// 关闭连接
		EON_Tcp_Close(pCmd->channel);

		// 返回0结束命令
		return 0;
	}

	return tBuffer->length;
}


/**
 * 缓存模式
 * 当收到数据长度，发送命令读取数据
 * --- 保留
 */
static void TcpCmd_QIRD(uint8_t nChannel)
{
	// 缓存模式
	// AT+QIRD=<connectID>[,<read_length>]

	char sCmdText[GPRS_CMD_SIZE];
	sprintf(sCmdText, "AT+QIRD=%d,%d\r\n", nChannel, TCP_RECV_MAX);

	EOTGprsCmd* pCmd = EON_Gprs_SendCmdPut(
		EOE_GprsCmd_QIRD,
		(uint8_t*)sCmdText, -1,
		GPRS_MODE_AT, GPRS_MODE_DATA,
		GPRS_TIME_LONG, GPRS_TRY_ONCE, GPRS_CMD_NORMAL);
	pCmd->tag = nChannel;
}

// 接收数据
static int OnTcpCmd_QIRD(EOTGprsCmd* pCmd, EOTBuffer* tBuffer)
{
	// 命令指向的通道
	uint8_t nChannel = (uint8_t)pCmd->tag;

	// 如果接收数据包含二进制，则不能使用字符串操作
	//+QIRD: <read_actual_length><CR><LF><data>
	//OK

	char ret[32];
	char* p = EOG_FindString((char*)tBuffer->buffer, tBuffer->length, ret, "+QIRD: ", NULL);
	if (p == NULL)
	{
		_D("*** 错误的返回");
		return 0;
	}

	int len;
	EOTConnect* hConnect = &s_TcpConnectList[nChannel];

	int nRead = atoi(ret);
	_T("读取数据: %d", nRead);
	if (nRead <= 0)
	{
		// 直到缓存为0，进行处理
		if (hConnect->recv_cb != NULL)
		{
			len = hConnect->recv_cb(hConnect, hConnect->recv_buffer.buffer,  hConnect->recv_buffer.length);
		}
		return 0;
	}

	// p为换行符所在位置
	p += 2;
	len = (int)((uint32_t)p - (uint32_t)(tBuffer->buffer));
	// 校验长度
	if ((tBuffer->length - len) < nRead)
	{
		_D("*** TCP接收数据长度错误 : %d, %d, %d", tBuffer->length, len, nRead);
		return 0;
	}

	// 加入缓存
	EOS_Buffer_Push(&(hConnect->recv_buffer), p, nRead);

	// 继续读取
	TcpCmd_QIRD(nChannel);

	return 0;
}
/**
 * 实际发送命令之前响应
 */
static void OnTcpCommandBefore(EOTGprsCmd* pCmd)
{
	if (pCmd == NULL) return;

	if (pCmd->id == EOE_GprsCmd_QISEND)
	{
		// 设置一个断行符
		EON_Gprs_CheckCmdFlag("> ", 2);
	}
}
static int OnTcpCommand(EOTGprsCmd* pCmd, EOTBuffer* tBuffer)
{
	if (pCmd == NULL) return -1;
	int nCmdId = pCmd->id;

	switch (nCmdId)
	{
	// TCP连接
	case EOE_GprsCmd_QIOPEN:
		return OnTcpCmd_QIOPEN(pCmd, tBuffer);
	// TCP关闭
	case EOE_GprsCmd_QICLOSE:
		return OnTcpCmd_QICLOSE(pCmd, tBuffer);
	// 发送数据
	case EOE_GprsCmd_QISEND:
		return OnTcpCmd_QISEND(pCmd, tBuffer);
	// 接收数据
	case EOE_GprsCmd_QIRD:
		return OnTcpCmd_QIRD(pCmd, tBuffer);
	default:
		return -1;
	}

	return 0;
}
// 数据返回
static int OnTcpData(uint8_t nChannel, EOTBuffer* tBuffer)
{
	_T("TCP接收数据返回[%d]: %d", nChannel, tBuffer->length);

	if (nChannel < 0 || nChannel >= MAX_TCP_CONNECT) return 0;

	EOTConnect* hConnect = &s_TcpConnectList[nChannel];

	// 调用接收回调
	if (hConnect->recv_cb != NULL)
	{
		hConnect->recv_cb(hConnect, tBuffer->buffer, tBuffer->length);
	}

	return 0;
}
static void OnTcpError(EOTGprsCmd* pCmd, int nError)
{
	// 如果出现错误，则完成
	_T("*** TCP错误事件: [%d]%d", pCmd->id, nError);
}

static int OnTcpEvent(EOTBuffer* tBuffer)
{
	// +QIURC: "closed",<connectID><CR><LF>
	// +QIURC: "recv",<connectID>,<currentrecvlength><CR><LF><data>

	char qiurcStr[GPRS_CMD_SIZE];
	if (NCHECK_STR(qiurcStr, "+QIURC: ", NULL)) return -1;
	//_T("TCP事件: %s", qiurcStr);

	char* sCmd;
	int nChannel = -1;

	char* ppArray[8];
	uint8_t nCount = 0;
	if (EOG_SplitString(qiurcStr, -1, ',', ppArray, &nCount) < 2)
	{
		_T("*** TCP事件参数错误: %s", qiurcStr);
		return 0;
	}
	sCmd = ppArray[0];
	nChannel = atoi(ppArray[1]);

	_T("TCP事件响应[%d]: %s", nChannel, sCmd);
	if (nChannel < 0 || nChannel >= MAX_TCP_CONNECT) return 0;

	if (strstr(sCmd, "recv") != NULL)
	{
		// 直吐模式
		int nRead = atoi(ppArray[2]);
		EON_Gprs_DataGet(nChannel, nRead);

		// 继续读取数据
		return tBuffer->length;
	}
	else if (strstr(sCmd, "closed") != NULL)
	{
		EON_Tcp_Close(nChannel);
	}

	return 0;
}

int EON_Tcp_Init(void)
{
	// 安全初始化
	int i;
	for (i=0; i<MAX_TCP_CONNECT; i++)
	{
		s_TcpConnectList[i].channel = i;
		s_TcpConnectList[i].host[0] = '\0';
		s_TcpConnectList[i].port = 0;
		s_TcpConnectList[i].status = STATUS_NONE;

		s_TcpConnectList[i].last_tick = 0;

		s_TcpConnectList[i].recv_buffer.buffer = NULL;
		s_TcpConnectList[i].recv_buffer.length = 0;
		s_TcpConnectList[i].recv_buffer.size = 0;
		s_TcpConnectList[i].recv_buffer.pos = 0;
		s_TcpConnectList[i].recv_buffer.tag = 0;

		s_TcpConnectList[i].open_cb = NULL;
		s_TcpConnectList[i].recv_cb = NULL;
		s_TcpConnectList[i].close_cb = NULL;
	}

	// 注入回调
	EON_Gprs_Callback(NULL, NULL,
			(EOFuncGprsError)OnTcpError,
			(EOFuncGprsCommandBefore)OnTcpCommandBefore,
			(EOFuncGprsCommand)OnTcpCommand,
			(EOFuncGprsData)OnTcpData,
			(EOFuncGprsEvent)OnTcpEvent);

	// 创建发送数据队列
	EOS_List_Create(&s_ListSendData);

	return 0;
}
void EON_Tcp_Update(uint64_t tick)
{
}


// 打开TCP连接
// AT+QIOPEN=<contextID>,<connectID>,<service_type>,"<IP_address>/<domain_name>",<remote_port>[,<local_port>[,<access_mode>]]
void EON_Tcp_Open(uint8_t nChannel)
{
	_T("连接TCP");

	// service_type
//	"TCP" 客户端建立 TCP 连接
//	"UDP" 客户端建立 UDP 连接
//	"TCP LISTENER" 建立 TCP 服务器监听 TCP 连接
//	"UDP SERVICE" 建立 UDP 服务

	// access_mode
//	 0 缓存模式
//	 1 直吐模式
//	 2 透传模式

	EOTConnect* hConnect = &s_TcpConnectList[nChannel];

	// 缓存模式需要提前分配接收缓存
	// 直吐模式不需要
//	if (hConnect->recv_buffer.size <= 0 && hConnect->recv_buffer.buffer == NULL)
//	{
//		EOS_Buffer_Create(&(hConnect->recv_buffer), MAX_TCP_RECV);
//	}

	// 使用直吐模式
	char sCmdText[GPRS_CMD_SIZE];
	sprintf(sCmdText, "AT+QIOPEN=1,%d,\"TCP\",\"%s\",%d,0,1\r\n",
			hConnect->channel, (const char*)hConnect->host, hConnect->port);

	// 命令只重试1次，由外部逻辑处理重连问题
	EOTGprsCmd* pCmd = EON_Gprs_SendCmdPut(
			EOE_GprsCmd_QIOPEN,
			(uint8_t*)sCmdText, -1,
			GPRS_MODE_AT, GPRS_MODE_AT,
			GPRS_TIME_LONG, GPRS_TRY_ONCE, GPRS_CMD_NORMAL);
	// 标记命令通道
	pCmd->channel = nChannel;
}

/**
 * 向gprs发送命令
 * 数据需要缓存
 */
void TcpPushSendData(uint8_t nChannel, uint8_t* pData, int nLength)
{
	// 数据先缓存
	// 从堆上分配空间
	uint8_t* pDataAlloc = (uint8_t*)pvPortMalloc(nLength);
	if (pDataAlloc == NULL)
	{
		_D("*** xAlloc NULL %d", nLength);
		return;
	}
	//_D("[xAlloc:%08X]\r\n", (uint32_t)pCmdTextAlloc);

	memcpy(pDataAlloc, pData, nLength);

	// 这个顺序号用于实际发送时取出对应的数据
	uint32_t nSendId = ++s_SendDataId;

	EOS_List_Push(&s_ListSendData, nSendId, pDataAlloc, nLength);
	_T("*** EOS_List_Push[%d]: %d/%d", s_ListSendData.count, (int)nSendId, nLength);

	char sCmdText[GPRS_CMD_SIZE];
	sprintf(sCmdText, "AT+QISEND=%d,%d\r\n", nChannel, nLength);

	EOTGprsCmd* pCmd = EON_Gprs_SendCmdPut(
			EOE_GprsCmd_QISEND,
			(uint8_t*)sCmdText, -1,
			GPRS_MODE_AT, GPRS_MODE_AT,
			GPRS_TIME_LONG, GPRS_TRY_ONCE, GPRS_CMD_NORMAL); // 不解析OK

	// 标记命令通道
	pCmd->channel = nChannel;
	// 标识并校验
	pCmd->tag = nSendId;
}

/**
 * 发送数据命令
 * 这里会创建一个数据副本，因此外部应用无需管理传输的数据，可以使用临时变量或立即复用
 */
void EON_Tcp_SendData(uint8_t nChannel, uint8_t* pData, int nLength)
{
	//EOTConnect* hConnect = &s_TcpConnectList[nChannel];

	int nPos = 0;
	int nCount = nLength;
	int i;

	// 计算分包
	// 不用while循环
	for (i=0; i<0xFF; i++)
	{
		if (nLength <= 0) break;

		if (nLength > TCP_SEND_MAX1)
		{
			if (nLength > TCP_SEND_MAX2)
			{
				// 包太大
				nCount = TCP_SEND_MAX1;
			}
			else
			{
				// 不大不小分一半，避免一个包大一个包小
				nCount = nLength / 2;
			}
		}
		else
		{
			// 包太小直接全部发送
			nCount = nLength;
		}

		TcpPushSendData(nChannel, &pData[nPos], nCount);

		nPos += nCount;
		nLength -= nCount;
	}
}

// 关闭连接
void EON_Tcp_Close(uint8_t nChannel)
{
	if (nChannel >= MAX_TCP_CONNECT)
	{
		_T("*** EON_Tcp_Close: %d/%d", nChannel, MAX_TCP_CONNECT);
		return;
	}

	// 发送关闭
	char sCmdText[GPRS_CMD_SIZE];
	sprintf(sCmdText, "AT+QICLOSE=%d\r\n", nChannel);

	EOTGprsCmd* pCmd = EON_Gprs_SendCmdPut(
		EOE_GprsCmd_QICLOSE,
		(uint8_t*)sCmdText, -1,
		GPRS_MODE_AT, GPRS_MODE_AT,
		GPRS_TIME_LONG, GPRS_TRY_NORMAL, GPRS_CMD_RESET);
	// 标记命令通道
	pCmd->channel = nChannel;
}


