/*
 * IAPConfig.h
 *
 *  Created on: Jul 15, 2023
 *      Author: hjx
 */

#ifndef IAPCONFIG_H_
#define IAPCONFIG_H_

#include "eob_flash.h"

#include <stdint.h>

// ǰ3������48KΪIAP������
// ��4������16KΪIAP������
// ��5��������ΪAPP������ 0x08010000

#define ADDRESS_BASE_IAP 			ADDRESS_FLASH_SECTOR_0
#define ADDRESS_BASE_CFG 			ADDRESS_FLASH_SECTOR_3 // �洢 TIAPConfig
#define ADDRESS_BASE_BIN 			ADDRESS_FLASH_SECTOR_4

#define SECTOR_IAP0 				FLASH_SECTOR_0 	// 16K
#define SECTOR_IAP1 				FLASH_SECTOR_1 	// 16K
#define SECTOR_IAP2 				FLASH_SECTOR_2 	// 16K
#define SECTOR_CFG 					FLASH_SECTOR_3 	// 16K
#define SECTOR_APP0 				FLASH_SECTOR_4 	// 64K 0x8010000 - 0x801FFFF
#define SECTOR_APP1 				FLASH_SECTOR_5	// 128K
#define SECTOR_APP2 				FLASH_SECTOR_6	// 128K
#define SECTOR_APP3 				FLASH_SECTOR_7	// 128K


// APP���ñ�ʶ
#define CONFIG_FLAG_NONE			0x00000000
#define CONFIG_FLAG_NORMAL			0x454F6E72 // EOnr ����ģʽ��ֱ�ӽ���
#define CONFIG_FLAG_START			0x454F7377 // EOsw ����ģʽ
#define CONFIG_FLAG_FLASH			0x454F6661 // EOfa ��¼ģʽ
#define CONFIG_FLAG_INPUT			0x454F6974 // EOit ������ģʽ



// W25Q�洢��Ϊ5����������18���飬������2M
#define W25Q_SECTOR_INFO			0x00000000 // ��Ϣ������������
#define W25Q_SECTOR_BIN_0			0x00010000 // �汾0�ļ���
#define W25Q_SECTOR_DAT_0			0x00100000 // �汾0������
#define W25Q_SECTOR_BIN_1			0x00110000 // �汾1�ļ���
#define W25Q_SECTOR_DAT_1			0x00200000 // �汾1������
#define W25Q_SECTOR_DATA_EX			0x00210000 // �߼�Ӧ�������������Դ洢Ӧ������


// ��ȡ��ǰ��ַ
#define W25Q_ADDRESS_BIN(addr, r)	if ((r)==0)(addr)=W25Q_SECTOR_BIN_0;else (addr)=W25Q_SECTOR_BIN_1
#define W25Q_ADDRESS_DAT(addr, r)	if ((r)==0)(addr)=W25Q_SECTOR_DAT_0;else (addr)=W25Q_SECTOR_DAT_1

/**
 * д��ͷҳ����
 * ͷ4���ֽ�Ϊ����
 */
#define WRITE_HEAD_LENGTH(nAddress, pBuffer, nLength) *((int*)(pBuffer))=(nLength);EOB_W25Q_WriteDirect((nAddress),(pBuffer),W25Q_PAGE_SIZE)
/**
 * ��ȡͷҳ����
 */
#define READ_HEAD_LENGTH(nAddress, pBuffer, nLength) EOB_W25Q_ReadDirect((nAddress),(pBuffer),W25Q_PAGE_SIZE);(nLength)=*((int*)(pBuffer))


// Bin�汾�ļ�8����
#define W25Q_BIN_BLOCK_COUNT		8


// IAP�����ô���flash��
#define IAP_CONFIG_ID 				0x10010007
#define GATE_HOST_LENGTH			64
#define DEVICE_KEY_LENGTH			64

typedef struct _stTIAPConfig
{
	// Config�汾
	uint32_t id;
	// ��ʶ
	uint32_t flag;
	// �豸Key
	char device_key[DEVICE_KEY_LENGTH];

	// W25Q�ⲿ�洢���洢�����汾�����ݣ�����bin�ļ������ò���
	// ���ڼ�¼��ǰ�汾����ʷ�汾
	// ���һ���汾������0��1
	uint8_t region;

	uint8_t r2;
	uint8_t r3;
	uint8_t r4;
}
TIAPConfig;

// ת������Ϊ��ʾ�ַ���
const char* F_ConfigFlagString(uint32_t nFlag);

// ��ȡ�豸Key��32λ�ַ���
void F_DeviceKey(void);

TIAPConfig* F_IAPConfig_Get(void);
// �������������Ϣ���ڲ�Flash
void F_IAPConfig_Save(uint32_t nFlag);
// ���ڲ�Flash��ȡ����������Ϣ
int F_IAPConfig_Load(void);

#endif /* IAPCONFIG_H_ */
