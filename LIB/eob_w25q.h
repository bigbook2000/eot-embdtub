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

// ������С��λ
#define W25Q_PAGE_SIZE 		256 	// 0x100
#define W25Q_SECTOR_SIZE 	4096	// 0x1000
#define W25Q_BLOCK_SIZE 	65536	// 0x10000

//
// W25QxоƬ��Ϣ
//
#define W25Q_NAME_LENGTH 16
typedef struct _stEOTW25QInfo
{
	// id
	uint16_t id;
	// ��С
	int16_t size;
	// ����
	char name[W25Q_NAME_LENGTH];
	// ��������
	int sector_count;
	// ��ַ��Χ
	int addres_range; // 0x2FFFFF

}
EOTW25QInfo;

void EOB_W25Q_Init(void);

void EOB_W25Q_EraseAll(void);
void EOB_W25Q_EraseSector(uint32_t nAddress);
void EOB_W25Q_EraseBlock(uint32_t nAddress);

// ��ҳд��
int EOB_W25Q_WriteData(uint32_t nAddress, void* pData, int nLength);
// ҳ��ֱ��д��
void EOB_W25Q_WriteDirect(uint32_t nAddress, void* pData, int nLength);
// ��ҳ��ȡ
void EOB_W25Q_ReadData(uint32_t nAddress, void* pData, int nLength);
// ҳ��ֱ�Ӷ�ȡ
void EOB_W25Q_ReadDirect(uint32_t nAddress, void* pData, int nLength);
// ҳ��У��
int EOB_W25Q_CheckDirect(uint32_t nAddress, void* pData, int nLength);

#endif /* EOB_W25Q_H_ */
