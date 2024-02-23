/*
 * eon_ec800.c
 *
 *  Created on: Jan 18, 2024
 *      Author: hjx
 * ģ��ʹ����Զ4Gģ��оƬ��������ͨѶ��ͨ��EC800����
 * ������������첽ģʽ���ڶ������߳�������
 * Ϊ�����жϵ�ǰ���URC�ϱ�������������������Ȼ��浽����֮��ͳһ����
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


// ��¼ģ�����һ����Ϣʱ��������⿨��
#define ACTIVE_TICK 			90000
static uint64_t s_LastActiveTick = 0L;

// DMA��Ϣ
static EOTDMAInfo s_GprsDMAInfo;

// ��Ч���ݻ��棬���⽻������1000������
#define GPRS_DATA_SIZE	1024
D_STATIC_BUFFER_DECLARE(s_GprsDMABuffer, GPRS_DATA_SIZE)
// �������棬���ڼ����β
D_STATIC_BUFFER_DECLARE(s_GprsRecvBuffer, GPRS_DATA_SIZE)
// �����յ�����
D_STATIC_BUFFER_DECLARE(s_GprsRecvData, GPRS_DATA_SIZE)


// GPRS����״̬
#define GPRS_STATUS_NONE 				0
#define GPRS_STATUS_POWER 				1 // �����ϵ�
#define GPRS_STATUS_READY 				2 // ���ڳ�ʼ��
#define GPRS_STATUS_RUN 				5 // ��������
#define GPRS_STATUS_RESET 				9 // ׼������


static uint8_t s_GprsStatus = GPRS_STATUS_NONE;
uint8_t EOB_Gprs_IsReady(void)
{
	return (s_GprsStatus == GPRS_STATUS_RUN);
}

// ��Ϣ����
static EOTList s_ListCmdSend;

// ���յ����ݳ��ȣ�ͬʱ��Ϊ��ʶ
static uint8_t s_CmdRecvChannel = 0xFF;
static int s_CmdRecvCount = 0;

// ���������������
static uint8_t s_CheckCmdLength = 0;
static char s_CheckCmdFlag[16];

// �������������һ��ִ�е�����
static EOTListItem* s_SemdLastItem = NULL;
EOTGprsCmd* EON_Gprs_LastCmd(void)
{
	if (s_SemdLastItem == NULL) return NULL;
	return s_SemdLastItem->data;
}

#define TIMER_ID_STATUS 14
#define TIMER_ID_CMD 	19

// ������ʱ��
static EOTTimer s_TimerGprsStatus;
// ���ʱ
static EOTTimer s_TimerCmdTimeout;
static void OnTimer_SystmReset(EOTTimer* tpTimer);
static void OnTimer_CmdTimeout(EOTTimer* tpTimer);


// ��Ӫ��
#define ISP_CODE_CM 1 // �ƶ� China Mobile
#define ISP_CODE_CU 2 // ��ͨ China Unicom
#define ISP_CODE_CT 3 // ���� China Telecom
static uint8_t s_ISPCode = ISP_CODE_CM;

// �ź�
static uint8_t s_CSQ = 0;
unsigned char EON_Gprs_CSQ(void)
{
	return s_CSQ;
}



// �򴮿�д����
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

// ���ûص��б�
static EOTList s_ListGprsCallbackReady;
static EOTList s_ListGprsCallbackReset;
static EOTList s_ListGprsCallbackError;
static EOTList s_ListGprsCallbackCommandBefore;
static EOTList s_ListGprsCallbackCommand;
static EOTList s_ListGprsCallbackData;
static EOTList s_ListGprsCallbackEvent;

// ��ӻص��б�
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
		// ����ֻ�ܵ�һ����
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


// ��ȡ�ź�
void EON_Gprs_Cmd_CSQ(void)
{
	EON_Gprs_SendCmdPut(EOE_GprsCmd_CSQ,
			(uint8_t*)"AT+CSQ\r\n", -1,
			GPRS_MODE_AT, GPRS_MODE_AT,
			GPRS_TIME_SHORT, GPRS_TRY_ONCE, GPRS_CMD_NORMAL);
}

// ��ȡʱ��
void EON_Gprs_Cmd_QLTS(void)
{
	EON_Gprs_SendCmdPut(EOE_GprsCmd_QLTS,
			(uint8_t*)"AT+QLTS=1\r\n", -1,
			GPRS_MODE_AT, GPRS_MODE_AT,
			GPRS_TIME_SHORT, GPRS_TRY_ONCE, GPRS_CMD_NORMAL);
}


// ֱ�ӵ��ô���д������
int EON_Gprs_DataPut(uint8_t* pData, int nLength)
{
	return USARTSendData(pData, nLength);
}

void EON_Gprs_DataGet(int nChannel, int nCount)
{
	// ֱ�ӻ�ȡָ�����ȵ�����
	s_CmdRecvChannel = nChannel;
	s_CmdRecvCount = nCount;
}

/**
 * EC�������\r\n���з�����
 * ��ʱ��������Ҫ���յ�һЩ�����ַ���ʱ����д���
 * ʹ�ô˺��������趨���д�����ַ�
 * @param sFlag �����ַ���
 * @param nLength �����ַ������ȣ������0��ʾ�رռ��
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


// �ͷŻص��������������
static void GprsCmdFree(EOTList* tpList, EOTListItem* tpItem)
{
	EOTGprsCmd *cmd = tpItem->data;
	if (cmd->send_mode == GPRS_MODE_AT)
	{
		// ����Ǹ��Ƶ�������
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
 * ��������
 * @param nCmdId �����ʶ
 *
 * @param sCmdData �������ģʽ��AT���ȿ�����������DATA��ֱ�Ӹ�ֵ
 * @param nCmdLen �������ȣ��������ģʽ��AT�����¼���
 *
 * @param nSendMode ����ģʽ�������AT�����ʾ���ַ�������Ҫ������������DATA��ֱ�Ӹ�ֵ
 * @param nRecvMode ��������أ������AT�����ҪУ���Ƿ񷵻�OK
 *
 * @param nTimeout ���Լ�����ȴ�ʱ������ʱ����Ϊʧ��
 * @param nTryCount ���Դ��������ʧ�ܣ������·��� *
 * @param nReset ʧ��֮���Ƿ�����ģ��
 *
 */
EOTGprsCmd* EON_Gprs_SendCmdPut(
		EOEGprsCmd nCmdId,
		uint8_t* pCmdData, int16_t nCmdLen,
		int8_t nSendMode, int8_t nRecvMode,
		int16_t nTimeout, int8_t nTryCount, int8_t nReset)
{
	_T("HeapSize = %u", xPortGetFreeHeapSize());

	// �Ӷ��Ϸ���ռ�
	int len = nCmdLen;

	if (nSendMode == GPRS_MODE_AT)
	{
		// �ȿ�������
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
 * �������
 * �������֮������������
 */
void EON_Gprs_SendCmdPop(void)
{
	if (s_SemdLastItem == NULL) return;

	// �ͷ�
	EOS_List_ItemFree(&s_ListCmdSend, s_SemdLastItem, (EOFuncListItemFree)GprsCmdFree);

	// ����
	s_SemdLastItem = NULL;
}

// ����APN
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


// ����GPRSģ��
// ������Ч
void EON_Gprs_Power(void)
{
	_T("Reset GPRS");

	s_LastActiveTick = 0L;

	// ������ջ���
	EOS_Buffer_Clear(&s_GprsDMABuffer);
	EOS_Buffer_Clear(&s_GprsRecvData);

	// �����ʶ��λ
	EON_Gprs_SendCmdPop();

	EOS_List_Clear(&s_ListCmdSend, (EOFuncListItemFree)GprsCmdFree);

	osDelay(500);

	LL_GPIO_ResetOutputPin(GPIO_EC_RST, PIN_EC_RST);

	// ����2s
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

// ��������ϵͳ
void EON_Gprs_Reset(void)
{
	if (s_GprsStatus == GPRS_STATUS_RESET) return;
	s_GprsStatus = GPRS_STATUS_RESET;

	_T("ϵͳ���ÿ�ʼ\r\n");

	EOTGprsCmd* pCmd = NULL;
	if (s_SemdLastItem != NULL) pCmd = s_SemdLastItem->data;
	GprsCallbackReset(pCmd);

	EOS_Timer_StartCall(&s_TimerGprsStatus, 5000, NULL, OnTimer_SystmReset);
}

static int OnGprsCmd_AT(EOTBuffer* tBuffer)
{
	if (CHECK_STR("OK"))
	{
		// �رջ���
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
		// ��ȡSIM��״̬
		EON_Gprs_SendCmdPut(EOE_GprsCmd_CPIN, (uint8_t*)"AT+CPIN?\r\n", -1,
				GPRS_MODE_AT, GPRS_MODE_AT, GPRS_TIME_LONG, GPRS_TRY_MORE, GPRS_CMD_RESET);
		return 0;
	}

	return tBuffer->length;
}

// ��ȡSIM��״̬
static int OnGprsCmd_CPIN(EOTBuffer* tBuffer)
{
	if (CHECK_STR("OK"))
	{
		// ���ڷ��� ME �Ĺ����ƶ��豸ʶ���루IMEI �ţ�
		EON_Gprs_SendCmdPut(EOE_GprsCmd_CGSN, (uint8_t*)"AT+CGSN\r\n", -1,
				GPRS_MODE_AT, GPRS_MODE_AT, GPRS_TIME_NORMAL, GPRS_TRY_MORE, GPRS_CMD_RESET);

		return 0;
	}

	return tBuffer->length;
}

// SIM��IMEI
static int OnGprsCmd_CGSN(EOTBuffer* tBuffer)
{
	if (CHECK_STR("OK"))
	{
		// ��ѯICCID
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
//		// ��ѯICCID
//		EOB_Gprs_SendCmdPut(EOE_GprsCmd_CCID, (uint8_t*)"AT+CCID\r\n", -1,
//				GPRS_MODE_AT, GPRS_MODE_AT, GPRS_TIME_NORMAL, GPRS_TRY_NORMAL, GPRS_CMD_RESET);
//		return 0;
	}

	return tBuffer->length;
}

// SIM��ICCID
static int OnGprsCmd_CCID(EOTBuffer* tBuffer)
{
	if (CHECK_STR("OK"))
	{
		// ȫ����ģʽ
		// 0 ��С����ģʽ
		// 1 ȫ����ģʽ
		// 3 ���� ME ������Ƶ�ź�
		// 4 ���� ME ���ͺͽ�����Ƶ�źŹ���
		// 5 ����(U)SIM
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

// �ر�GSM
static int OnGprsCmd_CFUN_0(EOTBuffer* tBuffer)
{
	return 0;
}

// ��GSM
static int OnGprsCmd_CFUN_1(EOTBuffer* tBuffer)
{
	if (CHECK_STR("OK"))
	{
		// ��ѯ��Ӫ��
		EON_Gprs_SendCmdPut(EOE_GprsCmd_COPS, (uint8_t*)"AT+COPS?\r\n", -1,
				GPRS_MODE_AT, GPRS_MODE_AT, GPRS_TIME_LONG, GPRS_TRY_NORMAL, GPRS_CMD_RESET);

		return 0;
	}

	return tBuffer->length;
}

// ��Ӫ��
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

	// ע��PSҵ��
	EON_Gprs_SendCmdPut(EOE_GprsCmd_CGREG, (uint8_t*)"AT+CGREG?\r\n", -1,
			GPRS_MODE_AT, GPRS_MODE_AT, GPRS_TIME_LONG, GPRS_TRY_MORE, GPRS_CMD_RESET);

	return 0;
}

// GPRSע��״̬
// 0 δע�᣻ME ��ǰδ����Ҫע�����Ӫ��
// 1 ��ע�ᣬ����������
// 2 δע�ᣬME ��������Ҫע�����Ӫ��
// 3 ע�ᱻ�ܾ�
// 4 δ֪״̬
// 5 ��ע�ᣬ��������
static int OnGprsCmd_CGREG(EOTBuffer* tBuffer)
{
	if (CHECK_STR("OK"))
	{
		// ����APN�����
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
		// ����PDP����
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
		// ��ȡIP��ַ
		EON_Gprs_SendCmdPut(EOE_GprsCmd_CGPADDR, (uint8_t*)"AT+CGPADDR=1\r\n", -1,
				GPRS_MODE_AT, GPRS_MODE_AT, GPRS_TIME_NORMAL, GPRS_TRY_NORMAL, GPRS_CMD_RESET);

		return 0;
	}

	return tBuffer->length;
}

// IP��ַ
static int OnGprsCmd_CGPADDR(EOTBuffer* tBuffer)
{
	// +CGPADDR: 1,"10.35.162.137"

	char ipStr[32];
	if (NCHECK_STR(ipStr, "\"", "\""))
		return tBuffer->length;

	_T("IP Address: %s\r\n", ipStr);

	// ��ȡIP�ǳ�ʼ�����һ��
	// ת�����ݴ���׶�
	// ������������
	s_GprsStatus = GPRS_STATUS_RUN;

	GprsCallbackReady();

	// ��ʼ�ź�
	EON_Gprs_Cmd_CSQ();

	// ��ȡʱ��
	EON_Gprs_Cmd_QLTS();

	return 0;
}

static int OnGprsCmd_QIDEACT(EOTBuffer* tBuffer)
{
	// ����������

	if (CHECK_STR("OK"))
	{
		// ��ȡSIM��״̬
		EON_Gprs_SendCmdPut(EOE_GprsCmd_CPIN, (uint8_t*)"AT+CPIN?\r\n", -1,
				GPRS_MODE_AT, GPRS_MODE_AT, GPRS_TIME_LONG, GPRS_TRY_MORE, GPRS_CMD_RESET);

		return 0;
	}

	return tBuffer->length;
}

/**
 * ��ʼ������
 * ��������Ѵ�������0�����򷵻�-1
 */
static int OnGprsStatus_Ready(EOTGprsCmd* pCmd, EOTBuffer* tBuffer)
{
	int nCmdId = pCmd->id;
	// ״̬��
	switch (nCmdId)
	{
		case EOE_GprsCmd_AT:
			return OnGprsCmd_AT(tBuffer);
		case EOE_GprsCmd_ATE0:
			return OnGprsCmd_ATE0(tBuffer);

		// SIM��״̬
		case EOE_GprsCmd_CPIN:
			return OnGprsCmd_CPIN(tBuffer);
		// SIM��IMEI
		case EOE_GprsCmd_CGSN:
			return OnGprsCmd_CGSN(tBuffer);
		// SIM��ICCID
		case EOE_GprsCmd_CCID:
			return OnGprsCmd_CCID(tBuffer);
		// �ر�GSM
		case EOE_GprsCmd_CFUN_0:
			return OnGprsCmd_CFUN_0(tBuffer);
		// ��GSM
		case EOE_GprsCmd_CFUN_1:
			return OnGprsCmd_CFUN_1(tBuffer);

		// ��Ӫ��
		case EOE_GprsCmd_COPS:
			return OnGprsCmd_COPS(tBuffer);
		// GPRSע��״̬
		case EOE_GprsCmd_CGREG:
			return OnGprsCmd_CGREG(tBuffer);

		// �ƶ�APN�����
		case EOE_GprsCmd_QICSGP:
			return OnGprsCmd_QICSGP(tBuffer); // ����PDP����
		// ����PDP
		case EOE_GprsCmd_QIACT:
			return OnGprsCmd_QIACT(tBuffer);
		// ȡ������PDP
		case EOE_GprsCmd_QIDEACT:
			return OnGprsCmd_QIDEACT(tBuffer);
		// IP��ַ
		case EOE_GprsCmd_CGPADDR:
			return OnGprsCmd_CGPADDR(tBuffer);
	}

	return -1;
}

// ��������״̬
static int OnGprsCmd_CIPSTATUS(EOTBuffer* tBuffer)
{
	return 0;
}

// DNS
static int OnGprsCmd_CDNSGIP(EOTBuffer* tBuffer)
{
	return 0;
}


// ʱ��
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

	// ����RTC
	EOB_Date_Set(&tDate);

	EOB_Date_Get(&tDate);
	printf("DateTime: %04d-%02d-%02d %02d:%02d:%02d\r\n",
			YEAR_ZERO + tDate.year, tDate.month, tDate.date, tDate.hour, tDate.minute, tDate.second);

	return 0;
}

// �ź�
static int OnGprsCmd_CSQ(EOTBuffer* tBuffer)
{
	// +CSQ: 16,99
	char ret[8];
	if (NCHECK_STR(ret, "+CSQ: ", ",")) return tBuffer->length;

	s_CSQ = atoi(ret);
	_T("�ź�ǿ��: %d", s_CSQ);

	return 0;
}

// �����¼�
static int GprsEventDispacher(EOTBuffer* tBuffer)
{
	// +QIURC: "closed",<connectID><CR><LF>
	// +QIURC: "recv",<connectID>,<currentrecvlength><CR><LF><data>
	// +QIURC: "pdpdeact",<contextID><CR><LF>

	char qiurcStr[GPRS_CMD_SIZE];
	if (NCHECK_STR(qiurcStr, "+QIURC: ", NULL)) return -1;

	_T("GPRS�¼�: %s", qiurcStr);

	int ret = 0;
	if (strstr(qiurcStr, "pdpdeact"))
	{
		// ������PDP����
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
 * ��·����
 * ��������������0�����򷵻�-1
 */
static int OnGprsStatus_Run(EOTGprsCmd* pCmd, EOTBuffer* tBuffer)
{
	if (pCmd == NULL) return 0;
	int nCmdId = pCmd->id;

	// ״̬��
	switch (nCmdId)
	{
		// ��������״̬
		case EOE_GprsCmd_CIPSTATUS:
			return OnGprsCmd_CIPSTATUS(tBuffer);

		// DNS
		case EOE_GprsCmd_CDNSGIP:
			return OnGprsCmd_CDNSGIP(tBuffer);

		// ʱ��
		case EOE_GprsCmd_QLTS:
			return OnGprsCmd_QLTS(tBuffer);

		// �ź�
		case EOE_GprsCmd_CSQ:
			return OnGprsCmd_CSQ(tBuffer);
	}

	return GprsCallbackCommand(pCmd, tBuffer);
}

/**
 * ��������
 */
static int GprsParseCmd(EOTGprsCmd* pCmd, EOTBuffer* tBuffer)
{
	int ret = -1;

	// �����¼�
	// ������¼�������-1
	ret = GprsEventDispacher(tBuffer);
	if (ret >= 0) return ret;

	// ����
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

	// ��������-1������ȴ���0��ʾ�������ѽ���������0����ʾ��Ҫ���
	if (ret == 0)
	{
		EON_Gprs_SendCmdPop();
	}

	return ret;
}

/**
 * ����ÿһ������
 */
static int GprsParseDataLine(int nLine, EOTGprsCmd* pCmd, EOTBuffer* tRecvBuffer)
{
	int ret = -1;

	// ֱ�ӽ�������ģʽ
	if (s_CmdRecvCount > 0)
	{
		_Tmb(s_GprsRecvBuffer.buffer, s_GprsRecvBuffer.length);
		_T("��������: %d/%d", s_CmdRecvCount, tRecvBuffer->length);
		if (s_CmdRecvCount > tRecvBuffer->length)
		{
			// �������㣬�����ȴ�
			return -1;
		}

		// ȡ��ָ�����ȵ�����
		ret = EOS_Buffer_Pop(tRecvBuffer, (char*)s_GprsRecvData.buffer, s_CmdRecvCount);
		if (ret < 0) return -1;
		s_GprsRecvData.length = ret;

		GprsCallbackData(s_CmdRecvChannel, &s_GprsRecvData);

		// ���ݽ�����ɣ��Զ�����
		s_CmdRecvCount = 0;

		// EOS_Buffer_Pop�Ѿ�ȡ�����ݣ����践�س��ȣ�������һ������
		return 0;
	}
	else
	{
		// ���һ��б�ʶ��ֻ��AT����ػ��з�
		if (s_GprsRecvBuffer.length < 2) return -1;

		_Tmb(s_GprsRecvBuffer.buffer, s_GprsRecvBuffer.length);
		// ÿ�ζ�ȡ1�����ݣ�buffer���������з���ret�������з�
		ret = EOS_Buffer_PopReturn(&s_GprsRecvBuffer, (char*)s_GprsRecvData.buffer, s_GprsRecvData.size);
		if (ret < 0)
		{
			// �ر����ͱ�ʶ���������з�
			if (s_CheckCmdLength > 0)
			{
				_T("����������з�: [%d]%s", s_CheckCmdLength, s_CheckCmdFlag);
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
			// �����������з�
			s_GprsRecvData.length = ret - 2;
		}

		_T("----<[%d/%d] %s", nLine, s_GprsRecvData.length, (const char*)s_GprsRecvData.buffer);

		// ���в�����
		if (s_GprsRecvData.length <= 0) return 2;

		// ������ش���0����ʾ�Ѵ��������ǽ�������Ҫ����
		ret = GprsParseCmd(pCmd, &s_GprsRecvData);
		_T("����������[%d]: %d", nLine, ret);
	}

	return ret;
}

// ��ӦGprs����
static void GprsRecvProcess(uint64_t tick)
{
	int i;
	// ����DMA
	// DMARecvBuffer();
	if (s_GprsDMABuffer.length >= GPRS_DATA_SIZE)
	{
		// ����̫����
		EOS_Buffer_Clear(&s_GprsDMABuffer);
	}

	int ret = EOB_DMA_Recv(&s_GprsDMAInfo);
	if (ret < 0)
	{
		_T("**** DMA Error [%d] %d - %d",
				s_GprsDMAInfo.data->length, s_GprsDMAInfo.retain, s_GprsDMAInfo.retain_last);
	}

	if (ret == 0) return;

	// ���ʱ���
	s_LastActiveTick = tick;

	// ȡ��DMA���棬������������
	EOS_Buffer_PushBuffer(&s_GprsRecvBuffer, &s_GprsDMABuffer);

	// ����������Ƿ����ָ����ʶ
	// OK��ERROR ��+CME ERROR: <err>
	// <CR><LF>OK<CR><LF>
	// �ڷ��ر�ʶ֮ǰ�᷵����Ӧ��Ϣ

	// ���ִ�е�����
	EOTGprsCmd* pCmd = NULL;
	if (s_SemdLastItem != NULL)
	{
		pCmd = s_SemdLastItem->data;
	}

	// ����ÿ�����ѭ������
	for (i=0; i<16; i++)
	{
		// �������0�������ǰ�м���
		if (GprsParseDataLine(i, pCmd, &s_GprsRecvBuffer) <= 0) break;
	}
}

static void GprsSendData(EOTGprsCmd* pCmd)
{
	// ����������������
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

	// �¼��ص�
	GprsCallbackCommandBefore(pCmd);

	// �򴮿ڷ�������
	USARTSendData(pCmd->cmd_data, pCmd->cmd_len);
}

/**
 * ʵ���򴮿ڷ�������
 */
static void GprsSendProcess(uint64_t tick)
{
	// ��������ʱ��Ҫ����
	if (s_CmdRecvCount > 0) return;

	// ��ʱ0����������
//	osEvent event = osMessageGet(s_MessageGprsCmdHandle, 0);
//	if (event.status != osEventMessage) return;
//	EOTGprsCmd *cmd = event.value.p;

	// �����������δ�������֮ǰ���ܷ����µ�����
	if (s_SemdLastItem != NULL) return;

	// ȡ�����ȵ�������
	s_SemdLastItem = EOS_List_Pop(&s_ListCmdSend);
	if (s_SemdLastItem == NULL) return;

	EOTGprsCmd* pCmd = s_SemdLastItem->data;
	if (pCmd == NULL) return;

	// ���ü�����
	EOS_Timer_StartDelay(&s_TimerCmdTimeout, pCmd->delay_max, pCmd->try_max, NULL);

	GprsSendData(pCmd);
}


void EON_Gprs_Init(void)
{
	_T("GPRS��ʼ��...");

	// ��ʼ����ʱ��
	EOS_Timer_Init(&s_TimerGprsStatus, TIMER_ID_STATUS, 5000, TIMER_LOOP_ONCE, NULL, NULL);
	EOS_Timer_Init(&s_TimerCmdTimeout, TIMER_ID_CMD, 30000, TIMER_LOOP_ONCE, NULL, (EOFuncTimer)OnTimer_CmdTimeout);

	// ��ʼ������
	D_STATIC_BUFFER_INIT(s_GprsDMABuffer, GPRS_DATA_SIZE);
	D_STATIC_BUFFER_INIT(s_GprsRecvBuffer, GPRS_DATA_SIZE);
	D_STATIC_BUFFER_INIT(s_GprsRecvData, GPRS_DATA_SIZE);

	//osPoolDef(PoolGprsCmd, 32, EOTGprsCmd);
	//s_PoolGprsCmdHandle = osPoolCreate(osPool(PoolGprsCmd));
	_T("HeapSize = %u", xPortGetFreeHeapSize());

//	// �����ж�
//	NVIC_SetPriority(USART3_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0));
//	NVIC_EnableIRQ(USART3_IRQn);
//
//	// �ֶ�����RXNE
//	LL_USART_EnableIT_RXNE(USART_GPRS);

//	// ����DMA
//	LL_DMA_SetMemoryAddress(DMA_GPRS, DMA_STREAM_GPRS, (uint32_t)s_GprsDMABufer);
//	LL_DMA_SetDataLength(DMA_GPRS, DMA_STREAM_GPRS, GPRS_DATA_SIZE);
//	LL_DMA_SetPeriphAddress(DMA_GPRS, DMA_STREAM_GPRS, (uint32_t)&(USART_GPRS->DR));
//	LL_DMA_EnableStream(DMA_GPRS, DMA_STREAM_GPRS);

	// ����DMA
	EOB_DMA_Recv_Init(&s_GprsDMAInfo,
			DMA_GPRS, DMA_STREAM_GPRS, (uint32_t)&(USART_GPRS->DR),
			&s_GprsDMABuffer);

	// ���ڽ���DMA
	LL_USART_EnableDMAReq_RX(USART_GPRS);

	//printf("%u\r\n", xPortGetFreeHeapSize());

	// �����������
	EOS_List_Create(&s_ListCmdSend);

	_T("OK");

	osDelay(50);

	EON_Gprs_Power();

	// ��һ������AT����
	EON_Gprs_SendCmdPut(EOE_GprsCmd_AT, (uint8_t*)"AT\r\n", -1,
			GPRS_MODE_AT, GPRS_MODE_AT, GPRS_TIME_LONG, GPRS_TRY_MORE, GPRS_CMD_RESET);
}

// ����GPRS�̴߳���
void EON_Gprs_Update(uint64_t tick)
{
	if (s_LastActiveTick > 0L && (tick - s_LastActiveTick) > ACTIVE_TICK)
	{
		_T("*** ģ�鳤ʱ������Ӧ");
		EON_Gprs_Power();
		return;
	}

	GprsRecvProcess(tick);
	GprsSendProcess(tick);

	EOS_Timer_Update(&s_TimerGprsStatus, tick);
	EOS_Timer_Update(&s_TimerCmdTimeout, tick);
}


/**
 * ��ʱ����
 */
static void OnTimer_SystmReset(EOTTimer* tpTimer)
{
	_T("\r\n\r\n---------------- ϵͳ���� ----------------\r\n\r\n\r\n");
	LL_mDelay(3000);
	NVIC_SystemReset();

	while (1) {}
}

/**
 * ������ʱ
 */
static void OnTimer_CmdTimeout(EOTTimer* tpTimer)
{
	if (s_SemdLastItem == NULL) return;
	EOTGprsCmd* pCmd = s_SemdLastItem->data;
	if (pCmd == NULL) return;

	if (EOS_Timer_IsLast(tpTimer))
	{
		// �ؼ���������
		if (pCmd->reset == GPRS_CMD_RESET)
		{
			EON_Gprs_Reset();
		}
		else
		{
			// ��Ӧ����ֻ����ǹؼ�����
			GprsCallbackError(pCmd, 0);

			// ��������
			EON_Gprs_SendCmdPop();
		}
	}
	else
	{
		_T("��������\r\n");
		GprsSendData(pCmd);
	}
}

