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

// 前3个扇区48K为IAP程序区
// 第4个扇区16K为IAP配置区
// 第5个扇区起为APP程序区 0x08010000

#define ADDRESS_BASE_IAP 			ADDRESS_FLASH_SECTOR_0
#define ADDRESS_BASE_CFG 			ADDRESS_FLASH_SECTOR_3 // 存储 TIAPConfig
#define ADDRESS_BASE_BIN 			ADDRESS_FLASH_SECTOR_4

#define SECTOR_IAP0 				FLASH_SECTOR_0 	// 16K
#define SECTOR_IAP1 				FLASH_SECTOR_1 	// 16K
#define SECTOR_IAP2 				FLASH_SECTOR_2 	// 16K
#define SECTOR_CFG 					FLASH_SECTOR_3 	// 16K
#define SECTOR_APP0 				FLASH_SECTOR_4 	// 64K 0x8010000 - 0x801FFFF
#define SECTOR_APP1 				FLASH_SECTOR_5	// 128K
#define SECTOR_APP2 				FLASH_SECTOR_6	// 128K
#define SECTOR_APP3 				FLASH_SECTOR_7	// 128K


// APP配置标识
#define CONFIG_FLAG_NONE			0x00000000
#define CONFIG_FLAG_NORMAL			0x454F6E72 // EOnr 正常模式，直接进入
#define CONFIG_FLAG_START			0x454F7377 // EOsw 运行模式
#define CONFIG_FLAG_FLASH			0x454F6661 // EOfa 烧录模式
#define CONFIG_FLAG_INPUT			0x454F6974 // EOit 命令行模式



// W25Q存储分为5个区，至少18个块，不少于2M
#define W25Q_SECTOR_INFO			0x00000000 // 信息数据区，保留
#define W25Q_SECTOR_BIN_0			0x00010000 // 版本0文件区
#define W25Q_SECTOR_DAT_0			0x00100000 // 版本0数据区
#define W25Q_SECTOR_BIN_1			0x00110000 // 版本1文件区
#define W25Q_SECTOR_DAT_1			0x00200000 // 版本1数据区
#define W25Q_SECTOR_DATA_EX			0x00210000 // 逻辑应用数据区，可以存储应用数据


// 获取当前地址
#define W25Q_ADDRESS_BIN(addr, r)	if ((r)==0)(addr)=W25Q_SECTOR_BIN_0;else (addr)=W25Q_SECTOR_BIN_1
#define W25Q_ADDRESS_DAT(addr, r)	if ((r)==0)(addr)=W25Q_SECTOR_DAT_0;else (addr)=W25Q_SECTOR_DAT_1

/**
 * 写入头页数据
 * 头4个字节为长度
 */
#define WRITE_HEAD_LENGTH(nAddress, pBuffer, nLength) *((int*)(pBuffer))=(nLength);EOB_W25Q_WriteDirect((nAddress),(pBuffer),W25Q_PAGE_SIZE)
/**
 * 读取头页数据
 */
#define READ_HEAD_LENGTH(nAddress, pBuffer, nLength) EOB_W25Q_ReadDirect((nAddress),(pBuffer),W25Q_PAGE_SIZE);(nLength)=*((int*)(pBuffer))


// Bin版本文件8个块
#define W25Q_BIN_BLOCK_COUNT		8


// IAP的配置存在flash中
#define IAP_CONFIG_ID 				0x10010007
#define GATE_HOST_LENGTH			64
#define DEVICE_KEY_LENGTH			64

typedef struct _stTIAPConfig
{
	// Config版本
	uint32_t id;
	// 标识
	uint32_t flag;
	// 设备Key
	char device_key[DEVICE_KEY_LENGTH];

	// W25Q外部存储器存储两个版本的数据，包括bin文件和配置参数
	// 用于记录当前版本和历史版本
	// 最后一个版本索引，0，1
	uint8_t region;

	uint8_t r2;
	uint8_t r3;
	uint8_t r4;
}
TIAPConfig;

// 转换命令为显示字符串
const char* F_ConfigFlagString(uint32_t nFlag);

// 获取设备Key，32位字符串
void F_DeviceKey(void);

TIAPConfig* F_IAPConfig_Get(void);
// 保存基础配置信息到内部Flash
void F_IAPConfig_Save(uint32_t nFlag);
// 从内部Flash读取基础配置信息
int F_IAPConfig_Load(void);

#endif /* IAPCONFIG_H_ */
