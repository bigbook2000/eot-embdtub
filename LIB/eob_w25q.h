/*
 * eob_w25q.h
 *
 *  Created on: Jun 30, 2023
 *      Author: hjx
 */

#ifndef EOB_W25Q_H_
#define EOB_W25Q_H_

#include "eos_buffer.h"

#include <stdint.h>

// 操作最小单位
#define W25Q_PAGE_SIZE 		256 	// 0x100
#define W25Q_SECTOR_SIZE 	4096	// 0x1000
#define W25Q_BLOCK_SIZE 	65536	// 0x10000

//
// W25Qx芯片信息
//
#define W25Q_NAME_LENGTH 16
typedef struct _stEOTW25QInfo
{
	// id
	uint16_t id;
	// 大小
	int16_t size;
	// 名称
	char name[W25Q_NAME_LENGTH];
	// 扇区数量
	int sector_count;
	// 地址范围
	int addres_range; // 0x2FFFFF

}
EOTW25QInfo;

void EOB_W25Q_Init(void);

void EOB_W25Q_EraseAll(void);
void EOB_W25Q_EraseSector(uint32_t nAddress);
void EOB_W25Q_EraseBlock(uint32_t nAddress);

// 跨页写入
int EOB_W25Q_WriteData(uint32_t nAddress, void* pData, int nLength);
// 页内直接写入
void EOB_W25Q_WriteDirect(uint32_t nAddress, void* pData, int nLength);
// 跨页读取
void EOB_W25Q_ReadData(uint32_t nAddress, void* pData, int nLength);
// 页内直接读取
void EOB_W25Q_ReadDirect(uint32_t nAddress, void* pData, int nLength);
// 页内校验
int EOB_W25Q_CheckDirect(uint32_t nAddress, void* pData, int nLength);

#endif /* EOB_W25Q_H_ */
