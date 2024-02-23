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


// ÿ������ȡ1K
#define TCP_RECV_MAX 	1024


// �������ݶ���
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
 * �������Ӳ���
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

	// ���ûص�
	hConnect->open_cb = tCallbackOpen;
	hConnect->recv_cb = tCallbackRecv;
	hConnect->close_cb = tCallbackClose;

	return hConnect;
}

// �ͷŻص��������������
static void TcpSendDataFree(EOTList* tpList, EOTListItem* tpItem)
{
	char *pSendData = tpItem->data;
	if (pSendData != NULL)
	{
		//_D("[xFree:%08X]\r\n", (uint32_t)cmd);
		vPortFree(pSendData);
	}
}

// TCP����
static int OnTcpCmd_QIOPEN(EOTBuffer* tBuffer)
{
	// QIOPENָ���ȷ���OK���󷵻ؽ��

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

		_T("TCP������Ӧ[%d]: %d", nChannel, nError);
		if (nChannel < 0 || nChannel >= MAX_TCP_CONNECT)
		{
			_T("*** ���������ͨ��[%d]", nChannel);
			return 0;
		}

		// Socket��ʶ��ռ��
		if (nError == 563)
		{
			_T("*** ����ͨ����ռ��[%d]", nChannel);
			EON_Tcp_Close(nChannel);
		}

		EOTConnect* hConnect = &s_TcpConnectList[nChannel];
		if (nError == 0) hConnect->status = STATUS_OPENED;

		hConnect->open_cb(hConnect, nError);
	}

	return 0;
}

// ������������
static int OnTcpCmd_QISEND(EOTGprsCmd* pCmd, EOTBuffer* tBuffer)
{
	if (CHECK_STR(">"))
	{
		// �رն��з�
		EON_Gprs_CheckCmdFlag("", 0);

		// ʵ�ʴ�������
		EOTListItem* item = EOS_List_Pop(&s_ListSendData);
		if (item == NULL)
		{
			_D("δ�ҵ���Ӧ������");
			return tBuffer->length;
		}

		_T("OnTcpCmd_QISEND[%d]: %d/%d", s_ListSendData.count, (int)pCmd->tag, (int)item->length);

		// ������������
		// ���͵�ʱ����DATA��������OK
		EON_Gprs_DataPut(item->data, item->length);

		// �ȷ������֮�󣬱����ͷ�ԭʼ����
		EOS_List_ItemFree(&s_ListSendData, item, (EOFuncListItemFree)TcpSendDataFree);
	}
	else if (CHECK_STR("SEND OK"))
	{
		return 0;
	}

	return tBuffer->length;
}


/**
 * ����ģʽ
 * ���յ����ݳ��ȣ����������ȡ����
 * --- ����
 */
static void TcpCmd_QIRD(uint8_t nChannel)
{
	// ����ģʽ
	// AT+QIRD=<connectID>[,<read_length>]

	char sCmdText[GPRS_CMD_SIZE];
	sprintf(sCmdText, "AT+QIRD=%d,%d\r\n", nChannel, TCP_RECV_MAX);

	EOTGprsCmd* pCmd = EON_Gprs_SendCmdPut(
		EOE_GprsCmd_QIRD,
		(uint8_t*)sCmdText, -1,
		GPRS_MODE_AT, GPRS_MODE_DATA,
		GPRS_TIME_LONG, GPRS_TRY_ONCE, GPRS_CMD_NORMAL);

	// ��¼��һ�ν��յ�ͨ��
	pCmd->tag = nChannel;
}

// ��������
static void OnTcpCmd_QIRD(EOTGprsCmd* pCmd, EOTBuffer* tBuffer)
{
	// ����ָ���ͨ��
	uint8_t nChannel = (uint8_t)pCmd->tag;

	// ����������ݰ��������ƣ�����ʹ���ַ�������
	//+QIRD: <read_actual_length><CR><LF><data>
	//OK

	char ret[32];
	char* p = EOG_FindString((char*)tBuffer->buffer, tBuffer->length, ret, "+QIRD: ", NULL);
	if (p == NULL)
	{
		_D("*** ����ķ���");
		return;
	}

	int len;
	EOTConnect* hConnect = &s_TcpConnectList[nChannel];

	int nRead = atoi(ret);
	_T("��ȡ����: %d", nRead);
	if (nRead <= 0)
	{
		// ֱ������Ϊ0�����д���
		if (hConnect->recv_cb != NULL)
		{
			len = hConnect->recv_cb(hConnect, hConnect->recv_buffer.buffer,  hConnect->recv_buffer.length);
		}
		return;
	}

	// pΪ���з�����λ��
	p += 2;
	len = (int)((uint32_t)p - (uint32_t)(tBuffer->buffer));
	// У�鳤��
	if ((tBuffer->length - len) < nRead)
	{
		_D("*** TCP�������ݳ��ȴ��� : %d, %d, %d", tBuffer->length, len, nRead);
		return;
	}

	// ���뻺��
	EOS_Buffer_Push(&(hConnect->recv_buffer), p, nRead);

	// ������ȡ
	TcpCmd_QIRD(nChannel);
}
/**
 * ʵ�ʷ�������֮ǰ��Ӧ
 */
static void OnTcpCommandBefore(EOTGprsCmd* pCmd)
{
	if (pCmd == NULL) return;

	if (pCmd->id == EOE_GprsCmd_QISEND)
	{
		// ����һ�����з�
		EON_Gprs_CheckCmdFlag("> ", 2);
	}
}
static int OnTcpCommand(EOTGprsCmd* pCmd, EOTBuffer* tBuffer)
{
	if (pCmd == NULL) return -1;
	int nCmdId = pCmd->id;

	switch (nCmdId)
	{
	// TCP����
	case EOE_GprsCmd_QIOPEN:
		return OnTcpCmd_QIOPEN(tBuffer);
	// ��������
	case EOE_GprsCmd_QISEND:
		return OnTcpCmd_QISEND(pCmd, tBuffer);
	// ��������
	case EOE_GprsCmd_QIRD:
		OnTcpCmd_QIRD(pCmd, tBuffer);
	}

	return 0;
}
// ���ݷ���
static void OnTcpData(uint8_t nChannel, EOTBuffer* tBuffer)
{
	_T("TCP�������ݷ���[%d]: %d", nChannel, tBuffer->length);

	if (nChannel < 0 || nChannel >= MAX_TCP_CONNECT) return;

	EOTConnect* hConnect = &s_TcpConnectList[nChannel];

	// ���ý��ջص�
	if (hConnect->recv_cb != NULL)
	{
		hConnect->recv_cb(hConnect, tBuffer->buffer, tBuffer->length);
	}
}
static void OnTcpError(EOTGprsCmd* pCmd, int nError)
{
	// ������ִ��������
}

static int OnTcpEvent(EOTBuffer* tBuffer)
{
	// +QIURC: "closed",<connectID><CR><LF>
	// +QIURC: "recv",<connectID>,<currentrecvlength><CR><LF><data>

	char qiurcStr[GPRS_CMD_SIZE];
	if (NCHECK_STR(qiurcStr, "+QIURC: ", NULL)) return -1;
	//_T("TCP�¼�: %s", qiurcStr);

	char* sCmd;
	int nChannel = -1;

	char* ppArray[8];
	uint8_t nCount = 0;
	if (EOG_SplitString(qiurcStr, -1, ',', ppArray, &nCount) < 2)
	{
		_T("*** TCP�¼���������: %s", qiurcStr);
		return 0;
	}
	sCmd = ppArray[0];
	nChannel = atoi(ppArray[1]);

	_T("TCP�¼���Ӧ[%d]: %s", nChannel, sCmd);

	if (nChannel >= 0 && nChannel < MAX_TCP_CONNECT)
	{
		if (strstr(sCmd, "recv") != NULL)
		{
			// ֱ��ģʽ
			int nRead = atoi(ppArray[2]);
			EON_Gprs_DataGet(nChannel, nRead);

			// ������ȡ����
			return tBuffer->length;
		}
		else if (strstr(sCmd, "closed") != NULL)
		{
			EON_Tcp_Close(nChannel);
		}
	}

	return 0;
}

int EON_Tcp_Init(void)
{
	// ��ȫ��ʼ��
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

	// ע��ص�
	EON_Gprs_Callback(NULL, NULL,
			(EOFuncGprsError)OnTcpError,
			(EOFuncGprsCommandBefore)OnTcpCommandBefore,
			(EOFuncGprsCommand)OnTcpCommand,
			(EOFuncGprsData)OnTcpData,
			(EOFuncGprsEvent)OnTcpEvent);

	// �����������ݶ���
	EOS_List_Create(&s_ListSendData);

	return 0;
}
void EON_Tcp_Update(uint64_t tick)
{
}


// ��TCP����
// AT+QIOPEN=<contextID>,<connectID>,<service_type>,"<IP_address>/<domain_name>",<remote_port>[,<local_port>[,<access_mode>]]
void EON_Tcp_Open(uint8_t nChannel)
{
	_T("����TCP");

	// service_type
//	"TCP" �ͻ��˽��� TCP ����
//	"UDP" �ͻ��˽��� UDP ����
//	"TCP LISTENER" ���� TCP ���������� TCP ����
//	"UDP SERVICE" ���� UDP ����

	// access_mode
//	 0 ����ģʽ
//	 1 ֱ��ģʽ
//	 2 ͸��ģʽ

	EOTConnect* hConnect = &s_TcpConnectList[nChannel];

	// ����ģʽ��Ҫ��ǰ������ջ���
	// ֱ��ģʽ����Ҫ
//	if (hConnect->recv_buffer.size <= 0 && hConnect->recv_buffer.buffer == NULL)
//	{
//		EOS_Buffer_Create(&(hConnect->recv_buffer), MAX_TCP_RECV);
//	}

	// ʹ��ֱ��ģʽ
	char sCmdText[GPRS_CMD_SIZE];
	sprintf(sCmdText, "AT+QIOPEN=1,%d,\"TCP\",\"%s\",%d,0,1\r\n",
			hConnect->channel, (const char*)hConnect->host, hConnect->port);

	// ����ֻ����1�Σ����ⲿ�߼�������������
	EON_Gprs_SendCmdPut(
			EOE_GprsCmd_QIOPEN,
			(uint8_t*)sCmdText, -1,
			GPRS_MODE_AT, GPRS_MODE_AT,
			GPRS_TIME_LONG, GPRS_TRY_ONCE, GPRS_CMD_NORMAL);
}

// ������������
void EON_Tcp_SendData(uint8_t nChannel, uint8_t* pData, int nLength)
{
	// �����Ȼ���
	EOTConnect* hConnect = &s_TcpConnectList[nChannel];

	// �Ӷ��Ϸ���ռ�
	uint8_t* pDataAlloc = (uint8_t*)pvPortMalloc(nLength);
	if (pDataAlloc == NULL)
	{
		_D("*** xAlloc NULL %d", nLength);
		return;
	}
	//_D("[xAlloc:%08X]\r\n", (uint32_t)pCmdTextAlloc);

	memcpy(pDataAlloc, pData, nLength);

	++s_SendDataId;
	EOS_List_Push(&s_ListSendData, s_SendDataId, pDataAlloc, nLength);
	_T("*** EOS_List_Push[%d]: %d/%d", s_ListSendData.count, (int)s_SendDataId, nLength);

	char sCmdText[GPRS_CMD_SIZE];
	sprintf(sCmdText, "AT+QISEND=%d,%d\r\n", hConnect->channel, nLength);

	EOTGprsCmd* pCmd = EON_Gprs_SendCmdPut(
			EOE_GprsCmd_QISEND,
			(uint8_t*)sCmdText, -1,
			GPRS_MODE_AT, GPRS_MODE_AT,
			GPRS_TIME_LONG, GPRS_TRY_ONCE, GPRS_CMD_NORMAL); // ������OK

	// ��ʶ��У��
	pCmd->tag = s_SendDataId;
}

// �ر�����
void EON_Tcp_Close(uint8_t nChannel)
{
	// ���͹ر�
	char sCmdText[GPRS_CMD_SIZE];
	sprintf(sCmdText, "AT+QICLOSE=%d\r\n", nChannel);

	EON_Gprs_SendCmdPut(
		EOE_GprsCmd_QICLOSE,
		(uint8_t*)sCmdText, -1,
		GPRS_MODE_AT, GPRS_MODE_AT,
		GPRS_TIME_LONG, GPRS_TRY_NORMAL, GPRS_CMD_NORMAL);

	// �ص���ע�ⲻҪѭ������
	EOTConnect* hConnect = &s_TcpConnectList[nChannel];
	hConnect->close_cb(hConnect, 0);

	// ����״̬
	hConnect->status = STATUS_NONE;
}


