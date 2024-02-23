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
// ��ʼд��
#define CMD_FLASH_BEGIN		0x31616C66 // fla1
// ����д��
#define CMD_FLASH_END		0x32616C66 // fla2
// �ļ���ʼ
#define CMD_BIN_BEGIN		0x316E6962 // bin1
// �ļ�����
#define CMD_BIN_END			0x326E6962 // bin2
// ���ÿ�ʼ
#define CMD_DAT_BEGIN		0x31746164 // dat1
// ���ý���
#define CMD_DAT_END			0x32746164 // dat2
// ��������
#define CMD_CONFIG			0x666E6F63 // conf
// ����Gate Api��ַ
#define CMD_GATE_API		0x65746167 // gate

// ������
#define CMD_RESET			0x74736572 // rest
// ȡ������
#define CMD_CANCEL			0x636E6163 // canc


void F_Cmd_Input(EOTBuffer* tBuffer, uint32_t nCheckFlag);

#endif /* IAPCMD_H_ */
