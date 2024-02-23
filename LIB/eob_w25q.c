/*
 * eob_w25q.c
 *
 *  Created on: Jun 30, 2023
 *      Author: hjx
 */


#include "eob_w25q.h"

#include "stm32f4xx_ll_utils.h"
#include "stm32f4xx_ll_gpio.h"
#include "stm32f4xx_ll_spi.h"

#include "eob_debug.h"

// W25Qÿ�ζ�д��С��λΪ256�ֽ�
// 1ҳ(Page)     = 256 �ֽ�(Byte)
// 1����(sector) =  16 ҳ(Page)
// 1��(block)    =  16 ����(sector)

#define W25Q_TRY_MAX			5

// ָ���
#define W25X_WriteEnable        0x06
#define W25X_WriteDisable       0x04
#define W25X_ReadStatusReg      0x05
#define W25X_WriteStatusReg     0x01
#define W25X_ReadData           0x03
#define W25X_FastReadData       0x0B
#define W25X_FastReadDual       0x3B
#define W25X_PageProgram        0x02
#define W25X_BlockErase         0xD8
#define W25X_SectorErase        0x20
#define W25X_ChipErase          0xC7
#define W25X_PowerDown          0xB9
#define W25X_ReleasePowerDown   0xAB
#define W25X_DeviceID           0xAB
#define W25X_ManufactDeviceID   0x90
#define W25X_JedecDeviceID      0x9F

// CS E15
// MISO	A6
// MOSI A7
// CLK A5
// GPIO�ӿ�
#define GPIO_W25Q_CS 			GPIOC
#define PIN_W25Q_CS				LL_GPIO_PIN_1
#define SPI_W25Q				SPI2

// �͵�ƽѡ�У��ߵ�ƽȡ��
#define W25Q_CS_BEGIN()     	LL_GPIO_ResetOutputPin(GPIO_W25Q_CS, PIN_W25Q_CS)
#define W25Q_CS_END()     		LL_GPIO_SetOutputPin(GPIO_W25Q_CS, PIN_W25Q_CS)

#define W25Q_INDEX_NONE 		0x7F
// W25QXϵ��оƬ�б�
static EOTW25QInfo s_W25QList[] =
{
		{ 0xEF13, 1, "W25Q80", 256, 0xFFFFF },
		{ 0xEF14, 2, "W25Q16", 512, 0x1FFFFF },
		{ 0xEF15, 4, "W25Q32", 1024, 0x2FFFFF },
		{ 0xEF16, 8, "W25Q64", 2048, 0x4FFFFF },
		{ 0xEF17, 16, "W25Q128", 4096, 0x8FFFFF },
};
static uint8_t s_W25QIndex = W25Q_INDEX_NONE;

#if 0
// SPI��
static uint8_t W25Q_Read_Byte()
{
    while (!LL_I2S_IsActiveFlag_RXNE(SPI_W25Q)) { }
    return LL_SPI_ReceiveData8(SPI_W25Q);
}
// SPIд
static void W25Q_Write_Byte(uint8_t uData)
{
	while (!LL_SPI_IsActiveFlag_TXE(SPI_W25Q)) { }
    LL_SPI_TransmitData8(SPI_W25Q, uData);
}
#endif

// SPI��д����
static uint8_t W25Q_ReadWrite_Byte(uint8_t uData)
{
	while (!LL_SPI_IsActiveFlag_TXE(SPI_W25Q)) { }
    LL_SPI_TransmitData8(SPI_W25Q, uData);

    while (!LL_I2S_IsActiveFlag_RXNE(SPI_W25Q)) { }
    return LL_SPI_ReceiveData8(SPI_W25Q);
}

static uint16_t W25Q_Read_ID(void)
{
	uint16_t nId;
	W25Q_CS_BEGIN();

	W25Q_ReadWrite_Byte(W25X_ManufactDeviceID);
	W25Q_ReadWrite_Byte(0x00);
	W25Q_ReadWrite_Byte(0x00);
	W25Q_ReadWrite_Byte(0x00);

	nId = (W25Q_ReadWrite_Byte(0xFF) << 8) | W25Q_ReadWrite_Byte(0xFF);

	W25Q_CS_END();

	uint8_t i;
	uint8_t n = sizeof(s_W25QList) / sizeof(EOTW25QInfo);
	for (i=0; i<n; i++)
	{
		if (s_W25QList[i].id == nId) break;
	}

	if (i < n)
	{
		s_W25QIndex = i;
		_T("Flash[%d]: %s, Size = %dM, Sector = %d", n,
				s_W25QList[i].name, s_W25QList[i].size, s_W25QList[i].sector_count);
	}
	else
	{
		_T("Flash[%d]: δ֪ %04X", n, nId);
	}

	return nId;
}

void EOB_W25Q_Init(void)
{
	_T("��ʼ�� Flash");

	// CSĬ����Ҫ���øߵ�ƽ
	W25Q_CS_END();

	// LL_SPI_SetRxFIFOThreshold();
	LL_SPI_Enable(SPI_W25Q);

	LL_mDelay(50);
	W25Q_Read_ID();

	if (s_W25QIndex == W25Q_INDEX_NONE)
	{
		LL_mDelay(50);
		W25Q_Read_ID();
	}
}

// ����оƬд��
void W25Q_WriteEnable(void)
{
	W25Q_CS_BEGIN();
	W25Q_ReadWrite_Byte(W25X_WriteEnable);
	W25Q_CS_END();
}
// ��ֹоƬд��
void W25Q_WriteDisable(void)
{
	W25Q_CS_BEGIN();
	W25Q_ReadWrite_Byte(W25X_WriteDisable);
	W25Q_CS_END();
}
void W25Q_WaitBusy(void)
{
	uint8_t b;
	while (1)
	{
		W25Q_CS_BEGIN();

		W25Q_ReadWrite_Byte(W25X_ReadStatusReg);
		b = W25Q_ReadWrite_Byte(0xFF);

		W25Q_CS_END();

		if ((b & 0x01) == 0x00) break;
	}
}

void EOB_W25Q_EraseAll(void)
{
	W25Q_WriteEnable();
	W25Q_WaitBusy();

	W25Q_CS_BEGIN();
	W25Q_ReadWrite_Byte(W25X_ChipErase);
	W25Q_CS_END();

	W25Q_WaitBusy();

	_T("W25Q ȫƬ�������");
}
void EOB_W25Q_EraseSector(uint32_t nAddress)
{
	W25Q_WriteEnable();
	W25Q_WaitBusy();

	W25Q_CS_BEGIN();

	W25Q_ReadWrite_Byte(W25X_SectorErase);

	W25Q_ReadWrite_Byte((nAddress >> 16) & 0xFF);
	W25Q_ReadWrite_Byte((nAddress >> 8) & 0xFF);
	W25Q_ReadWrite_Byte((nAddress) & 0xFF);

	W25Q_CS_END();

	W25Q_WaitBusy();
}
void EOB_W25Q_EraseBlock(uint32_t nAddress)
{
	//_T("W25Q Block����: 0x%08X", nAddress);

	W25Q_WriteEnable();
	W25Q_WaitBusy();

	W25Q_CS_BEGIN();

	W25Q_ReadWrite_Byte(W25X_BlockErase);

	W25Q_ReadWrite_Byte((nAddress >> 16) & 0xFF);
	W25Q_ReadWrite_Byte((nAddress >> 8) & 0xFF);
	W25Q_ReadWrite_Byte((nAddress) & 0xFF);

	W25Q_CS_END();

	W25Q_WaitBusy();
}
// ҳ��ֱ��д��
void EOB_W25Q_WriteDirect(uint32_t nAddress, void* pData, int nLength)
{
	int i;

	W25Q_WriteEnable();
	W25Q_WaitBusy();

	W25Q_CS_BEGIN();

	W25Q_ReadWrite_Byte(W25X_PageProgram);

//	W25Q_ReadWrite_Byte(0x00);
//	W25Q_ReadWrite_Byte(0x01);
//	W25Q_ReadWrite_Byte(0x00);
//
//	W25Q_ReadWrite_Byte(0x55);
//	W25Q_ReadWrite_Byte(0x66);

	W25Q_ReadWrite_Byte((nAddress >> 16) & 0xFF);  //����24bit��ַ
	W25Q_ReadWrite_Byte((nAddress >> 8) & 0xFF);
	W25Q_ReadWrite_Byte((nAddress) & 0xFF);

	uint8_t* pBuffer = pData;
	for (i=0; i<nLength; i++)
	{
		W25Q_ReadWrite_Byte(pBuffer[i]);
	}

//	if ((nAddress % 256) == 0 && pBuffer[0] == 0xFF && pBuffer[1] == 0xFF)
//	{
//		_T("[%08X] FF FF", nAddress);
//	}

	W25Q_CS_END();

	W25Q_WaitBusy();
}
// ҳ��ֱ�Ӷ�ȡ
void EOB_W25Q_ReadDirect(uint32_t nAddress, void* pData, int nLength)
{
	int i;

	W25Q_CS_BEGIN();

	W25Q_ReadWrite_Byte(W25X_ReadData);

//	W25Q_ReadWrite_Byte(0x00);
//	W25Q_ReadWrite_Byte(0x01);
//	W25Q_ReadWrite_Byte(0x00);
//
//	uint8_t n1 = W25Q_ReadWrite_Byte(0xFF);
//	uint8_t n2 = W25Q_ReadWrite_Byte(0xFF);
//	_T("%02X", n1);
//	_T("%02X", n2);

	W25Q_ReadWrite_Byte((nAddress >> 16) & 0xFF);  //����24bit��ַ
	W25Q_ReadWrite_Byte((nAddress >> 8) & 0xFF);
	W25Q_ReadWrite_Byte((nAddress) & 0xFF);

	uint8_t* pBuffer = pData;
	for (i=0; i<nLength; i++)
	{
		pBuffer[i] = W25Q_ReadWrite_Byte(0xFF);
	}

//	if ((nAddress % 256) == 0 && pBuffer[0] == 0xFF && pBuffer[1] == 0xFF)
//	{
//		_T("[%08X] FF FF ", nAddress);
//	}

	W25Q_CS_END();
}
// ҳ��У��
int EOB_W25Q_CheckDirect(uint32_t nAddress, void* pData, int nLength)
{
	int i;
	int nCheckCount = 0;

	W25Q_CS_BEGIN();

	W25Q_ReadWrite_Byte(W25X_ReadData);

	W25Q_ReadWrite_Byte((nAddress >> 16) & 0xFF);  //����24bit��ַ
	W25Q_ReadWrite_Byte((nAddress >> 8) & 0xFF);
	W25Q_ReadWrite_Byte((nAddress) & 0xFF);

	uint8_t dRead;
	uint8_t* pBuffer = pData;
	for (i=0; i<nLength; i++)
	{
		dRead = W25Q_ReadWrite_Byte(0xFF);
		if (pBuffer[i] != dRead)
		{
			_T("W25Q���� [%08X/%d]: %02X - %02X", (int)nAddress, i, pBuffer[i], dRead);
			nCheckCount++;
		}
	}

	W25Q_CS_END();

	return nCheckCount;
}
// ��ҳд��
int EOB_W25Q_WriteData(uint32_t nAddress, void* pData, int nLength)
{
	uint32_t nWrite;
	uint8_t* pBytes = pData;

	int i, j, cnt;

	// ��λ�����ֶܷΣ���ѭ������
	cnt = nLength / W25Q_PAGE_SIZE + 5;
	int nCheckCount;

	for (i=0; i<cnt; i++)
	{
		if (nLength <= 0) break;

		nWrite = W25Q_PAGE_SIZE - nAddress % W25Q_PAGE_SIZE;
		if (nWrite > nLength)
		{
			nWrite = nLength;
		}

		for (j=0; j<W25Q_TRY_MAX; j++)
		{
			EOB_W25Q_WriteDirect(nAddress, pBytes, nWrite);

			nCheckCount = EOB_W25Q_CheckDirect(nAddress, pBytes, nWrite);
			if (nCheckCount == 0) break;
		}

		if (j >= W25Q_TRY_MAX)
		{
			return -1;
		}

		nAddress += nWrite;
		pBytes += nWrite;
		nLength -= nWrite;
	}

	return nLength;
}

