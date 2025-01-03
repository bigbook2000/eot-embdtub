/*
 * eon_ec800.h
 *
 *  Created on: Jan 18, 2024
 *      Author: hjx
 */

#ifndef EON_EC800_H_
#define EON_EC800_H_


#include "stdint.h"
#include "eos_buffer.h"

// ���ݷ�������ģʽ������ģʽ��͸��ģʽ
#define GPRS_MODE_AT 		0
#define GPRS_MODE_DATA 		1

// ����ִ��ʧ���Ƿ�����
#define GPRS_CMD_NORMAL 	0
#define GPRS_CMD_RESET 		1
#define GPRS_CMD_POWER 		2

// ���ʱ
#define GPRS_TIME_SHORT 	3000
#define GPRS_TIME_NORMAL 	8000
#define GPRS_TIME_LONG 		15000

#define GPRS_TRY_ONCE 		1
#define GPRS_TRY_NORMAL 	3
#define GPRS_TRY_MORE 		5

// GPRS�����������С
#define GPRS_CMD_SIZE 256

// �жϷ���tBuffer�ַ����Ƿ����
#define CHECK_STR(s) 				strnstr((char*)tBuffer->buffer, s, tBuffer->length) != NULL
// ���ҷ���tBufferָ�����ַ���
#define NCHECK_STR(r, s1, s2) 		EOG_FindString((char*)tBuffer->buffer, tBuffer->length, r, s1, s2) == NULL

typedef enum _emEOEGprsCmd
{
	EOE_GprsCmd_AT = 1,

	// �رջ���
	EOE_GprsCmd_ATE0,

	// SIM��״̬
	EOE_GprsCmd_CPIN,
	// SIM��IMEI
	EOE_GprsCmd_CGSN,
	// SIM��ICCID
	EOE_GprsCmd_CCID,

	// ����ģʽ0
	EOE_GprsCmd_CFUN_0,
	// ����ģʽ1
	EOE_GprsCmd_CFUN_1,

	// ��Ӫ��
	EOE_GprsCmd_COPS,

	// GPRSע��״̬
	EOE_GprsCmd_CGREG,
	// GPRSע��
	EOE_GprsCmd_CGATT,

	// �����
	EOE_GprsCmd_QICSGP,
	// �ƶ�APN�����
	EOE_GprsCmd_QICSGP_CM,
	// ��ͨAPN�����
	EOE_GprsCmd_QICSGP_UN,

	// ����GPRS
	EOE_GprsCmd_QIACT,
	// ȡ������GPRS
	EOE_GprsCmd_QIDEACT,

	// ��������״̬
	EOE_GprsCmd_CIPSTATUS,
	// IP��ַ
	EOE_GprsCmd_CGPADDR,
	// DNS
	EOE_GprsCmd_CDNSGIP,


	// ʱ��
	EOE_GprsCmd_QLTS,
	// �ź�
	EOE_GprsCmd_CSQ,


	// TCP Э��

	// TCP����
	EOE_GprsCmd_QIOPEN,
	// TCP�Ͽ�
	EOE_GprsCmd_QICLOSE,

	// TCP��������
	EOE_GprsCmd_QISEND,
	// TCP��������
	EOE_GprsCmd_QISEND_D,
	// TCP��������
	EOE_GprsCmd_QIRD,

	// HTTP Э��

	// ���� HTTP PDP
	EOE_HttpCmd_QHTTPCFG_PDP,
	// ���� HTTP �����Ӧͷ
	EOE_HttpCmd_QHTTPCFG_RESP,
	// ���� HTTP SSL ID
	EOE_HttpCmd_QHTTPCFG_SSL_ID,
	// ���� SSL �汾
	EOE_HttpCmd_QSSLCFG_VER,
	// ���� SSL �����׼�
	EOE_HttpCmd_QSSLCFG_SUITE,
	// ���� SSL ��֤����
	EOE_HttpCmd_QSSLCFG_LEVEL,
	// ���� URL
	EOE_HttpCmd_QHTTPURL,
	// ���� URL
	EOE_HttpCmd_QHTTPURL_D,

	// GET
	EOE_HttpCmd_QHTTPGET,
	// POST
	EOE_HttpCmd_QHTTPPOST,

	// ��ȡ��Ӧ
	EOE_HttpCmd_QHTTPREAD,


	EOE_GprsCmd_Test,
	EOE_GprsCmd_Count,

	// ���ڶ�̬��������Ҫ����Ҫ���δ��������
	EOE_GprsCmd_Tag = 0xFFFF,

	EOE_GprsCmd_None = 0xFFFFFFFF,
}
EOEGprsCmd;

// �������
typedef struct _stEOTGprsCmd
{
	// �����ʶ
	EOEGprsCmd id;

	// ��ұ�ʶ
	uint32_t tag;

	// ͨ����ʶ
	uint8_t channel;
	// ���뱣��
	uint8_t r1;
	// ���뱣��
	uint16_t r2;

	int8_t send_mode;
	int8_t recv_mode;

	// ����ִ��ʧ���Ƿ�����
	int8_t reset;
	// ������
	int8_t try_max;

	// ��ʱʱ��
	int16_t delay_max;
	// �����
	int16_t cmd_len;

	// ����
	uint8_t* cmd_data;
}
EOTGprsCmd;

typedef void (*EOFuncGprsReady)(void);
typedef void (*EOFuncGprsError)(EOTGprsCmd* pCmd, int nError);
typedef void (*EOFuncGprsReset)(EOTGprsCmd* pCmd);
typedef int (*EOFuncGprsCommandBefore)(EOTGprsCmd* pCmd);
typedef int (*EOFuncGprsCommand)(EOTGprsCmd* pCmd, EOTBuffer* tBuffer);
typedef int (*EOFuncGprsData)(uint8_t nChannel, EOTBuffer* tBuffer);
typedef int (*EOFuncGprsEvent)(EOTBuffer* tBuffer);

void EON_Gprs_Init(void);
void EON_Gprs_Update(uint64_t tick);

void EON_Gprs_Power(void);
void EON_Gprs_Reset(void);

void EON_Gprs_Callback(
		EOFuncGprsReady tCallbackReady,
		EOFuncGprsReset tCallbackReset,
		EOFuncGprsError tCallbackError,
		EOFuncGprsCommandBefore tCallbackCommandBefore,
		EOFuncGprsCommand tCallbackCommand,
		EOFuncGprsData tCallbackData,
		EOFuncGprsEvent tCallbackEvent);

// �Ƿ��Ѿ�����ͨѶ
uint8_t EON_Gprs_IsReady(void);

EOTGprsCmd* EON_Gprs_LastCmd(void);
EOTGprsCmd* EON_Gprs_SendCmdPut(
		EOEGprsCmd nCmdId,
		uint8_t* pCmdData, int16_t nCmdLen,
		int8_t nSendMode, int8_t nRecvMode,
		int16_t nTimeout, int8_t nTryCount, int8_t nReset);
void EON_Gprs_SendCmdPop(void);

// ֱ���򴮿�д����
int EON_Gprs_DataPut(uint8_t* pData, int nLength);
// ��ȡָ�����ȵ�����
void EON_Gprs_DataGet(int nChannel, int nCount);
// ���������������
void EON_Gprs_CheckCmdFlag(char* sFlag, int nLength);

// ��ȡ�ź�
void EON_Gprs_Cmd_CSQ(void);
// ��ȡʱ��
void EON_Gprs_Cmd_QLTS(void);


uint8_t EON_Gprs_CSQ(void);

#endif /* EON_EC800_H_ */
