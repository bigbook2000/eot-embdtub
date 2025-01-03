/*
 * IAPCmd.h
 *
 *  Created on: Feb 6, 2024
 *      Author: hjx
 */

#ifndef IAPCMD_H_
#define IAPCMD_H_

#include <stdint.h>

#include "eos_buffer.h"

// ����ͷ
#define CMD_FLAG_BEGIN		0x23232323 // ####
#define CMD_FLAG_END		0x0A0D4040 // @@\r\n

#define CMD_DELAY			500 // ��ʱ
#define CMD_LENGTH			12
#define CMD_NONE			0x00000000 //


// ��ת��APP
#define CMD_JUMP_APP		0x7070616A // japp
// ��ӡ�ļ�
#define CMD_BIN_SHOW		0x306E6962 // bin0
// �ļ���ʼ
#define CMD_BIN_BEGIN		0x316E6962 // bin1
// �ļ�����
#define CMD_BIN_END			0x326E6962 // bin2
// ��ӡ����
#define CMD_DAT_SHOW		0x30746164 // dat0
// ���ÿ�ʼ
#define CMD_DAT_BEGIN		0x31746164 // dat1
// ���ý���
#define CMD_DAT_END			0x32746164 // dat2
// ��������
#define CMD_CONFIG			0x666E6F63 // conf
// �汾��¼
#define CMD_FLASH			0x73616C66 // flas
// ��ȡ�汾
#define CMD_FAPP			0x70706166 // fapp

// ������
#define CMD_RESET			0x74736572 // rest
// ȡ������
#define CMD_CANCEL			0x636E6163 // canc



// �������
typedef void (*EOFuncCmdProcess)(uint32_t nCmdId, EOTBuffer* tBuffer, int nPos);
typedef struct _stTCmdProcess
{
	uint32_t id;
	EOFuncCmdProcess func;
}
TCmdProcess;

void F_Cmd_ProcessInit(void);
void F_Cmd_ProcessExt(uint32_t nCmdId, EOFuncCmdProcess tProcess);
void F_Cmd_ProcessSet(uint32_t nCmdId);

void F_Cmd_Input(EOTBuffer* tBuffer);
void F_Cmd_SetFlag(uint32_t nCmdId);

#endif /* IAPCMD_H_ */
