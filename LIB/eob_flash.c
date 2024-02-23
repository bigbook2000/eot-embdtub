/*
 * eob_flash.c
 *
 *  Created on: Jun 30, 2023
 *      Author: hjx
 */

#include "eob_flash.h"

#include "eob_debug.h"

#define FLASH_TRY_MAX		5

// USE_HAL_DRIVER ��Ҫֱ�Ӱ��������ļ�
#include "stm32f4xx_hal.h"
#include "stm32f4xx_ll_utils.h"

#define FLASH_READ_INT32(addr) (uint32_t)(*(uint32_t*)(addr))

int EOB_Flash_Erase(unsigned int nSector)
{
	HAL_FLASH_Unlock();
	__HAL_FLASH_DATA_CACHE_DISABLE(); // �ر����ݻ���

	__HAL_FLASH_CLEAR_FLAG(
			FLASH_FLAG_EOP |
			FLASH_FLAG_OPERR |
			FLASH_FLAG_WRPERR |
			FLASH_FLAG_PGAERR |
			FLASH_FLAG_PGPERR |
			FLASH_FLAG_PGSERR);

	FLASH_Erase_Sector(nSector, FLASH_VOLTAGE_RANGE_3); // ÿ�β���32λ

	__HAL_FLASH_DATA_CACHE_ENABLE();
	HAL_FLASH_Lock();

	return 0;
}

int EOB_Flash_Check(unsigned int nAddress, void* pBuffer, int nSize)
{
	if ((nSize % sizeof(uint32_t)) != 0)
	{
		_T("Flash Check ����: 32λ");
		return -1;
	}

	uint32_t* pSrc = (uint32_t*)nAddress;
	uint32_t* pDst32 = (uint32_t*)pBuffer;

	int nCheckCount = 0;
	int i, cnt;
	cnt = nSize / sizeof(uint32_t);
	for (i=0; i<cnt; i++)
	{
		if ((*pDst32) != (*pSrc))
		{
			nCheckCount += 4;
			return nCheckCount;
		}
		++pDst32;
		++pSrc;
	}

	return 0;
}

// nSize ���ֽ����ݳ���
int EOB_Flash_Read(unsigned int nAddress, void* pBuffer, int nSize)
{
	if ((nSize % sizeof(uint32_t)) != 0)
	{
		_T("Flash Read ����: 32λ");
		return -1;
	}

	uint32_t* pSrc = (uint32_t*)nAddress;
	uint32_t* pDst32 = (uint32_t*)pBuffer;

	int i, cnt;
	cnt = nSize / sizeof(uint32_t);
	for (i=0; i<cnt; i++)
	{
		*pDst32 = *pSrc;
		++pDst32;
		++pSrc;
	}

	return nSize;
}
// ����飬д֮ǰ��ִ�в����������������
// nSize ���ֽ����ݳ���
int EOB_Flash_Write(unsigned int nAddress, void* pBuffer, int nSize)
{
	if ((nSize % sizeof(uint32_t)) != 0)
	{
		_T("Flash Write ����: 32λ");
		return -1;
	}

	HAL_FLASH_Unlock();
	__HAL_FLASH_DATA_CACHE_DISABLE(); // �ر����ݻ���

	__HAL_FLASH_CLEAR_FLAG(
			FLASH_FLAG_EOP |
			FLASH_FLAG_OPERR |
			FLASH_FLAG_WRPERR |
			FLASH_FLAG_PGAERR |
			FLASH_FLAG_PGPERR |
			FLASH_FLAG_PGSERR);

	uint32_t* pSrc = (uint32_t*)pBuffer;
	uint32_t* pDst = (uint32_t*)nAddress;

	int i, j, cnt;
	HAL_StatusTypeDef dStatus;

	uint32_t dSrc;
	cnt = nSize / sizeof(uint32_t);
	for (i=0; i<cnt; i++)
	{
		dSrc = (*pSrc);

		for (j=0; j<FLASH_TRY_MAX; i++)
		{
			dStatus = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, (uint32_t)pDst, (uint64_t)dSrc);
			if (HAL_OK == dStatus) break;

			LL_mDelay(1);
		}

		if (j >= FLASH_TRY_MAX)
		{
			__HAL_FLASH_DATA_CACHE_ENABLE();
			HAL_FLASH_Lock();

			return -1;
		}

		++pDst;
		++pSrc;
	}

	__HAL_FLASH_DATA_CACHE_ENABLE();
	HAL_FLASH_Lock();

	return nSize;
}
