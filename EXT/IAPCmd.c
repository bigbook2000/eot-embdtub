/*
 * IAPCmd.c
 *
 *  Created on: Feb 6, 2024
 *      Author: hjx
 */

#include "IAPCmd.h"

#include <string.h>

#include "stm32f4xx_hal.h"
#include "stm32f4xx_ll_iwdg.h"
#include "stm32f4xx_ll_utils.h"

#include "eos_buffer.h"

#include "eob_util.h"
#include "eob_debug.h"
#include "eob_w25q.h"

#include "IAPConfig.h"

// CRCУ��
static CRC_HandleTypeDef s_CRCHandle;

// ��ǰ�����ʶ
static uint32_t s_CommandFlag = CMD_NONE;

static uint32_t s_W25Q_Address = 0x1FFFFFFF;
static int s_W25Q_Length = 0;

/**
 * ��ת��APP
 */
static void Cmd_JumpApp(EOTBuffer* tBuffer, int nPos)
{
	EOS_Buffer_Pop(tBuffer, NULL, nPos);

	if (s_CommandFlag != CMD_NONE)
	{
		_T("�޷�ִ������: ��ת��APP [%08lX]", s_CommandFlag);
		return;
	}

	_T("ִ������: ��ת��APP");
	s_CommandFlag = CMD_JUMP_APP;

	// ��APP��ʶ��Ϊ CONFIG_FLAG_START(0x454F7377)
	F_IAPConfig_Save(CONFIG_FLAG_START);

	s_CommandFlag = CMD_NONE;

	EOB_JumpApp(ADDRESS_BASE_BIN);
}

/**
 * �汾�ļ���ʼ
 */
static void Cmd_BinBegin(EOTBuffer* tBuffer, int nPos)
{
	EOS_Buffer_Pop(tBuffer, NULL, nPos);

	if (s_CommandFlag != CMD_NONE)
	{
		_T("�޷�ִ������: �ļ���ʼ [%08lX]", s_CommandFlag);
		return;
	}

	TIAPConfig* tIAPConfig = F_IAPConfig_Get();
	s_CommandFlag = CMD_BIN_BEGIN;

	// W25Q�洢��Ϊ5����������18���飬������2M

	//EOB_W25Q_EraseAll();
	_T("ִ������: �ļ���ʼ[�汾%d]", tIAPConfig->region);
	if (tIAPConfig->region == 0)
	{
		s_W25Q_Address = W25Q_SECTOR_BIN_1;
	}
	else
	{
		s_W25Q_Address = W25Q_SECTOR_BIN_2;
	}

	int i;
	int nAddress = s_W25Q_Address;
	for (i=0; i<W25Q_BIN_BLOCK_COUNT; i++)
	{
		EOB_W25Q_EraseBlock(nAddress);
		nAddress += W25Q_BLOCK_SIZE;
	}

	// �����ļ���С
	s_W25Q_Length = 0;

	_T("####bin1@@");
	_T("��ѡ��APP�����ļ�(*.bin) : ");
}

/**
 * �ļ�����
 */
static void Cmd_BinEnd(EOTBuffer* tBuffer, int nPos)
{
	EOS_Buffer_Pop(tBuffer, NULL, nPos);

	if (s_CommandFlag != CMD_BIN_BEGIN)
	{
		_T("�޷�ִ������: �ļ����� [%08lX]", s_CommandFlag);
		return;
	}

	TIAPConfig* tIAPConfig = F_IAPConfig_Get();
	s_CommandFlag = CMD_BIN_END;

	// CRCУ��
	HAL_CRC_Init(&s_CRCHandle);

//	uint32_t nBinAddress;
//	int nBinLength = g_W25Q_Length;
//	// �����ļ���С
//	if (g_W25QConfig.app_index == 0)
//	{
//		nBinAddress = W25Q_SECTOR_BIN_2;
//	}
//	else
//	{
//		nBinAddress = W25Q_SECTOR_BIN_1;
//	}
//
//	// ��ȡbin����
//	int i, cnt;
//	int nRead;
//
//	// ��ҳ��
//	uint8_t pBuffer[W25Q_PAGE_SIZE];
//
//	cnt = nBinLength / W25Q_PAGE_SIZE + 1;
//	for (i=0; i<cnt; i++)
//	{
//		LL_IWDG_ReloadCounter(IWDG); // ι��
//
//		if (nBinLength <= 0) break;
//
//		nRead = W25Q_PAGE_SIZE;
//		if (nRead > nBinLength) nRead = nBinLength;
//
//		_T("У��APP�����ļ�(*.bin): [%08lX] %d", nBinAddress, nRead);
//		EOB_W25Q_ReadDirect(nBinAddress, pBuffer, nRead);
//		//if (i == 0) _Tmb(pBuffer, W25Q_PAGE_SIZE);
//
//		nBinLength -= nRead;
//		nBinAddress += nRead;
//	}

	HAL_CRC_DeInit(&s_CRCHandle);

	//
	// �����ļ���С��ͬʱ�л��汾
	if (tIAPConfig->region == 0)
	{
		tIAPConfig->bin1_length = s_W25Q_Length;
	}
	else
	{
		tIAPConfig->bin2_length = s_W25Q_Length;
	}
	_T("ִ������: �ļ�����[�汾%d]", tIAPConfig->region);

	// ���ı�APP��ʶ
	F_IAPConfig_Save(CONFIG_FLAG_NONE);

	s_CommandFlag = CMD_NONE;

	_T("####bin2@@");
}

/**
 * ���ÿ�ʼ
 * ����д����ǵ�ǰ�����������ı�����������
 */
static void Cmd_DatBegin(EOTBuffer* tBuffer, int nPos)
{
	EOS_Buffer_Pop(tBuffer, NULL, nPos);

	if (s_CommandFlag != CMD_NONE)
	{
		_T("�޷�ִ������: ���ÿ�ʼ [%08lX]", s_CommandFlag);
		return;
	}

	TIAPConfig* tIAPConfig = F_IAPConfig_Get();
	s_CommandFlag = CMD_DAT_BEGIN;

	_T("ִ������: ���ÿ�ʼ[�汾%d]", tIAPConfig->region);
	//EOB_W25Q_EraseAll();
	// ��bin�ļ���ͬ������д����ǵ�ǰ�汾
	if (tIAPConfig->region == 0)
	{
		s_W25Q_Address = W25Q_SECTOR_DAT_1;
	}
	else
	{
		s_W25Q_Address = W25Q_SECTOR_DAT_2;
	}

	// ����ֻռ1������
	EOB_W25Q_EraseBlock(s_W25Q_Address);

	// �������ô�С
	s_W25Q_Length = 0;

	_T("####dat1@@");
	_T("��ѡ��APP�����ļ�(*.txt) : ");
}

/**
 * ���ý���
 */
static void Cmd_DatEnd(EOTBuffer* tBuffer, int nPos)
{
	EOS_Buffer_Pop(tBuffer, NULL, nPos);

	if (s_CommandFlag != CMD_DAT_BEGIN)
	{
		_T("�޷�ִ������: ���ý��� [%08lX]", s_CommandFlag);
		return;
	}

	TIAPConfig* tIAPConfig = F_IAPConfig_Get();
	s_CommandFlag = CMD_DAT_END;

	_T("ִ������: ���ý���[�汾%d]", tIAPConfig->region);

	// �������ô�С
	if (tIAPConfig->region == 0)
	{
		tIAPConfig->dat1_length = s_W25Q_Length;
	}
	else
	{
		tIAPConfig->dat2_length = s_W25Q_Length;
	}

	// ���ı�APP��ʶ
	F_IAPConfig_Save(CONFIG_FLAG_NONE);
	s_CommandFlag = CMD_NONE;

	_T("####dat2@@");
}

/**
 * �����汾����ÿ�θ���bin֮����Ҫ����
 * ��������������ã����Բ�����
 */
static void Cmd_Config(EOTBuffer* tBuffer, int nPos)
{
	EOS_Buffer_Pop(tBuffer, NULL, nPos);

	_T("ִ������: ��������");
	s_CommandFlag = CMD_CONFIG;

	uint8_t ra = 0;
	int blen_a = 0;
	int dlen_a = 0;
	int blen_b = 0;
	int dlen_b = 0;

	// ��������
	TIAPConfig* tIAPConfig = F_IAPConfig_Get();
	ra = tIAPConfig->region;
	if (ra == 0)
	{
		blen_a = tIAPConfig->bin1_length;
		dlen_a = tIAPConfig->dat1_length;
		blen_b = tIAPConfig->bin2_length;
		dlen_b = tIAPConfig->dat2_length;

		tIAPConfig->region = 1;
	}
	else
	{
		blen_a = tIAPConfig->bin2_length;
		dlen_a = tIAPConfig->dat2_length;
		blen_b = tIAPConfig->bin1_length;
		dlen_b = tIAPConfig->dat1_length;

		tIAPConfig->region = 0;
	}
	_T("�汾����: %d(%d, %d) -> %d(%d, %d)",
			ra, blen_a, dlen_a,
			tIAPConfig->region, blen_b, dlen_b);

	F_IAPConfig_Save(CONFIG_FLAG_NONE);
	s_CommandFlag = CMD_NONE;
}


/**
 * ��¼�ļ�����ǰ�汾
 */
static void Cmd_Flash(EOTBuffer* tBuffer, int nPos)
{
	EOS_Buffer_Pop(tBuffer, NULL, nPos);

	_T("ִ������: �汾��¼");
	s_CommandFlag = CMD_FLASH;

	// ��APP��ʶ��Ϊ CONFIG_FLAG_FLASH(0x454F6661)
	F_IAPConfig_Save(CONFIG_FLAG_FLASH);

	s_CommandFlag = CMD_NONE;

	LL_mDelay(1000);

	// ����
	__set_FAULTMASK(1);
	NVIC_SystemReset();

	while (1) {};
}

// �����ļ�����
static void ProcessData(EOTBuffer* tBuffer, int nPos)
{
	// �����ļ�ʱÿ�β��ó���1K�ֽڣ������ӳ�10ms

	switch (s_CommandFlag)
	{
		case CMD_BIN_BEGIN:
			{
				//if (g_W25Q_Length < 256) _Tmb(tBuffer->buffer, tBuffer->length);
				EOB_W25Q_WriteData(s_W25Q_Address, tBuffer->buffer, tBuffer->length);
				s_W25Q_Address += tBuffer->length;
				s_W25Q_Length += tBuffer->length;

				_T("����APP�����ļ�(*.bin) : [%08lX] %d / %d",
						s_W25Q_Address, s_W25Q_Length, tBuffer->length);

				EOS_Buffer_Clear(tBuffer);
			}
			break;
//		case CMD_FLASH_BEGIN:
//			{
//				int nWrite = (tBuffer->length / 4) * 4;
//				if (nWrite > 0)
//				{
//					if (EOB_Flash_Write(s_W25Q_Address, tBuffer->buffer, nWrite) < 0)
//					{
//						_T("��¼ʧ��");
//						return;
//					}
//
//					s_W25Q_Address += nWrite;
//					s_W25Q_Length += nWrite;
//
//					_T("��¼APP�����ļ�(*.bin) : [%08lX] %d / %d, %d",
//							s_W25Q_Address, s_W25Q_Length, nWrite, tBuffer->length);
//					//_Tmb(tBuffer->buffer, nWrite);
//
//					EOS_Buffer_Pop(tBuffer, NULL, nWrite);
//				}
//			}
//			break;
		case CMD_DAT_BEGIN:
			{
				EOB_W25Q_WriteData(s_W25Q_Address, tBuffer->buffer, tBuffer->length);
				s_W25Q_Address += tBuffer->length;
				s_W25Q_Length += tBuffer->length;

				_T("����APP��������(*.txt) : [%08lX] %d / %d",
						s_W25Q_Address, s_W25Q_Length, tBuffer->length);

				EOS_Buffer_Clear(tBuffer);
			}
			break;
	}
}

/**
 * ����
 */
static void Cmd_Reset(EOTBuffer* tBuffer, int nPos)
{
	// ����������ģʽ CONFIG_FLAG_INPUT
	F_IAPConfig_Save(CONFIG_FLAG_INPUT);

	EOS_Buffer_Clear(tBuffer);

	_T("ִ������: ϵͳ����");
	_T("\r\n\r\n---------------- ϵͳ���� ----------------\r\n\r\n\r\n");
	LL_mDelay(3000);
	NVIC_SystemReset();

	while (1) {}
}

void F_Cmd_Input(EOTBuffer* tBuffer, uint32_t nCheckFlag)
{
	uint8_t* pBuffer = tBuffer->buffer;

	int i, cnt;
	uint32_t n1, n2;

	uint32_t nCmdFlag = CMD_NONE;
	int nPos = 0;

	if (tBuffer->length >= CMD_LENGTH)
	{
		// ������####��ͷ�������4���ַ� @@\r\n���з���β
		cnt = tBuffer->length - CMD_LENGTH + 1;
		//_Tmb(tBuffer->buffer, tBuffer->length);

		for (i=0; i<cnt; i++)
		{
			n1 = *((uint32_t*)pBuffer);
			n2 = *((uint32_t*)(pBuffer + sizeof(uint32_t) + sizeof(uint32_t)));
			if (n1 == CMD_FLAG_BEGIN && n2 == CMD_FLAG_END)
			{
				nCmdFlag = *((uint32_t*)(pBuffer + sizeof(uint32_t)));
				nPos = i + CMD_LENGTH;
				break;
			}
			pBuffer++;
		}
	}

	// ֻУ�鵥һ����
	if (CMD_NONE != nCheckFlag && nCmdFlag != nCheckFlag) return;

	switch (nCmdFlag)
	{
		case CMD_JUMP_APP: // japp ��ת��APP
			Cmd_JumpApp(tBuffer, nPos);
			break;

		case CMD_BIN_BEGIN: // bin1 �ļ���ʼ
			Cmd_BinBegin(tBuffer, nPos);
			break;
		case CMD_BIN_END: // bin2 �ļ�����
			Cmd_BinEnd(tBuffer, nPos);
			break;
		case CMD_DAT_BEGIN: // dat1 ���ÿ�ʼ
			Cmd_DatBegin(tBuffer, nPos);
			break;
		case CMD_DAT_END: // dat2 ���ý���
			Cmd_DatEnd(tBuffer, nPos);
			break;
		case CMD_CONFIG: // conf ��������
			Cmd_Config(tBuffer, nPos);
			break;
		case CMD_FLASH: // flas �汾��¼
			Cmd_Flash(tBuffer, nPos);
			break;
		case CMD_RESET: // rest ����
			Cmd_Reset(tBuffer, nPos);
			break;
		case CMD_CANCEL: // canc ȡ������
			_T("����ȡ��");
			EOS_Buffer_Clear(tBuffer);
			s_CommandFlag = CMD_NONE;
			break;
		default:
			ProcessData(tBuffer, nPos);
			break;
	}

	// ������Ч����
	if (tBuffer->length >= DEBUG_LIMIT)
	{
		EOS_Buffer_Clear(tBuffer);
	}
}

