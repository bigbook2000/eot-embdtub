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

// CRC校验
static CRC_HandleTypeDef s_CRCHandle;

// 当前命令标识
static uint32_t s_CommandFlag = CMD_NONE;

static uint32_t s_W25Q_Address = 0x1FFFFFFF;
static int s_W25Q_Length = 0;

#define CMD_PROCESS_SIZE 16
static TCmdProcess s_ListCmdProcess[CMD_PROCESS_SIZE];

/**
 * 跳转到APP
 */
static void Cmd_JumpApp(uint32_t nCmdId, EOTBuffer* tBuffer, int nPos)
{
	EOS_Buffer_Pop(tBuffer, NULL, nPos);

	if (s_CommandFlag != CMD_NONE)
	{
		_T("无法执行命令: 跳转到APP [%08lX]", s_CommandFlag);
		return;
	}

	_T("执行命令: 跳转到APP");
	s_CommandFlag = CMD_JUMP_APP;

	// 将APP标识改为 CONFIG_FLAG_START(0x454F7377)
	F_IAPConfig_Save(CONFIG_FLAG_START);

	s_CommandFlag = CMD_NONE;

	EOB_JumpApp(ADDRESS_BASE_BIN);
}

/**
 * 版本文件开始
 */
static void Cmd_BinBegin(uint32_t nCmdId, EOTBuffer* tBuffer, int nPos)
{
	EOS_Buffer_Pop(tBuffer, NULL, nPos);

	if (s_CommandFlag != CMD_NONE)
	{
		_T("无法执行命令: 文件开始 [%08lX]", s_CommandFlag);
		return;
	}

	TIAPConfig* tIAPConfig = F_IAPConfig_Get();
	s_CommandFlag = CMD_BIN_BEGIN;

	// W25Q存储分为5个区，至少18个块，不少于2M

	//EOB_W25Q_EraseAll();
	_T("执行命令: 文件开始[版本%d]", tIAPConfig->region);
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

	// 重置文件大小
	s_W25Q_Length = 0;

	_T("####bin1@@");
	_T("请选择APP程序文件(*.bin) : ");
}

/**
 * 文件结束
 */
static void Cmd_BinEnd(uint32_t nCmdId, EOTBuffer* tBuffer, int nPos)
{
	EOS_Buffer_Pop(tBuffer, NULL, nPos);

	if (s_CommandFlag != CMD_BIN_BEGIN)
	{
		_T("无法执行命令: 文件结束 [%08lX]", s_CommandFlag);
		return;
	}

	TIAPConfig* tIAPConfig = F_IAPConfig_Get();
	s_CommandFlag = CMD_BIN_END;

	// CRC校验
	HAL_CRC_Init(&s_CRCHandle);

//	uint32_t nBinAddress;
//	int nBinLength = g_W25Q_Length;
//	// 更新文件大小
//	if (g_W25QConfig.app_index == 0)
//	{
//		nBinAddress = W25Q_SECTOR_BIN_2;
//	}
//	else
//	{
//		nBinAddress = W25Q_SECTOR_BIN_1;
//	}
//
//	// 读取bin程序
//	int i, cnt;
//	int nRead;
//
//	// 按页读
//	uint8_t pBuffer[W25Q_PAGE_SIZE];
//
//	cnt = nBinLength / W25Q_PAGE_SIZE + 1;
//	for (i=0; i<cnt; i++)
//	{
//		LL_IWDG_ReloadCounter(IWDG); // 喂狗
//
//		if (nBinLength <= 0) break;
//
//		nRead = W25Q_PAGE_SIZE;
//		if (nRead > nBinLength) nRead = nBinLength;
//
//		_T("校验APP程序文件(*.bin): [%08lX] %d", nBinAddress, nRead);
//		EOB_W25Q_ReadDirect(nBinAddress, pBuffer, nRead);
//		//if (i == 0) _Tmb(pBuffer, W25Q_PAGE_SIZE);
//
//		nBinLength -= nRead;
//		nBinAddress += nRead;
//	}

	HAL_CRC_DeInit(&s_CRCHandle);

	//
	// 更新文件大小，同时切换版本
	if (tIAPConfig->region == 0)
	{
		tIAPConfig->bin1_length = s_W25Q_Length;
	}
	else
	{
		tIAPConfig->bin2_length = s_W25Q_Length;
	}
	_T("执行命令: 文件结束[版本%d]", tIAPConfig->region);

	// 不改变APP标识
	F_IAPConfig_Save(CONFIG_FLAG_NONE);

	s_CommandFlag = CMD_NONE;

	_T("####bin2@@");
}

/**
 * 配置开始
 * 配置写入的是当前配置区，不改变配置区索引
 */
static void Cmd_DatBegin(uint32_t nCmdId, EOTBuffer* tBuffer, int nPos)
{
	EOS_Buffer_Pop(tBuffer, NULL, nPos);

	if (s_CommandFlag != CMD_NONE)
	{
		_T("无法执行命令: 配置开始 [%08lX]", s_CommandFlag);
		return;
	}

	TIAPConfig* tIAPConfig = F_IAPConfig_Get();
	s_CommandFlag = CMD_DAT_BEGIN;

	_T("执行命令: 配置开始[版本%d]", tIAPConfig->region);
	//EOB_W25Q_EraseAll();
	// 和bin文件不同，配置写入的是当前版本
	if (tIAPConfig->region == 0)
	{
		s_W25Q_Address = W25Q_SECTOR_DAT_1;
	}
	else
	{
		s_W25Q_Address = W25Q_SECTOR_DAT_2;
	}

	// 配置只占1个区块
	EOB_W25Q_EraseBlock(s_W25Q_Address);

	// 重置配置大小
	s_W25Q_Length = 0;

	_T("####dat1@@");
	_T("请选择APP配置文件(*.txt) : ");
}

/**
 * 配置结束
 */
static void Cmd_DatEnd(uint32_t nCmdId, EOTBuffer* tBuffer, int nPos)
{
	EOS_Buffer_Pop(tBuffer, NULL, nPos);

	if (s_CommandFlag != CMD_DAT_BEGIN)
	{
		_T("无法执行命令: 配置结束 [%08lX]", s_CommandFlag);
		return;
	}

	TIAPConfig* tIAPConfig = F_IAPConfig_Get();
	s_CommandFlag = CMD_DAT_END;

	_T("执行命令: 配置结束[版本%d]", tIAPConfig->region);

	// 更新配置大小
	if (tIAPConfig->region == 0)
	{
		tIAPConfig->dat1_length = s_W25Q_Length;
	}
	else
	{
		tIAPConfig->dat2_length = s_W25Q_Length;
	}

	// 不改变APP标识
	F_IAPConfig_Save(CONFIG_FLAG_NONE);
	s_CommandFlag = CMD_NONE;

	_T("####dat2@@");
}

/**
 * 交换版本区域，每次更新bin之后，需要交换
 * 如果仅仅更新配置，可以不交换
 */
static void Cmd_Config(uint32_t nCmdId, EOTBuffer* tBuffer, int nPos)
{
	EOS_Buffer_Pop(tBuffer, NULL, nPos);

	_T("执行命令: 重置配置");
	s_CommandFlag = CMD_CONFIG;

	uint8_t ra = 0;
	int blen_a = 0;
	int dlen_a = 0;
	int blen_b = 0;
	int dlen_b = 0;

	// 交换索引
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
	_T("版本交换: %d(%d, %d) -> %d(%d, %d)",
			ra, blen_a, dlen_a,
			tIAPConfig->region, blen_b, dlen_b);

	F_IAPConfig_Save(CONFIG_FLAG_NONE);
	s_CommandFlag = CMD_NONE;
}


/**
 * 烧录文件到当前版本
 */
static void Cmd_Flash(uint32_t nCmdId, EOTBuffer* tBuffer, int nPos)
{
	EOS_Buffer_Pop(tBuffer, NULL, nPos);

	_T("执行命令: 版本烧录");
	s_CommandFlag = CMD_FLASH;

	// 将APP标识改为 CONFIG_FLAG_FLASH(0x454F6661)
	F_IAPConfig_Save(CONFIG_FLAG_FLASH);

	s_CommandFlag = CMD_NONE;

	LL_mDelay(1000);

	// 重启
	__set_FAULTMASK(1);
	NVIC_SystemReset();

	while (1) {};
}

// 处理文件数据
static void ProcessData(EOTBuffer* tBuffer, int nPos)
{
	// 传输文件时每次不得超过1K字节，至少延迟10ms

	switch (s_CommandFlag)
	{
		case CMD_BIN_BEGIN:
			{
				//if (g_W25Q_Length < 256) _Tmb(tBuffer->buffer, tBuffer->length);
				EOB_W25Q_WriteData(s_W25Q_Address, tBuffer->buffer, tBuffer->length);
				s_W25Q_Address += tBuffer->length;
				s_W25Q_Length += tBuffer->length;

				_T("处理APP程序文件(*.bin) : [%08lX] %d / %d",
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
//						_T("烧录失败");
//						return;
//					}
//
//					s_W25Q_Address += nWrite;
//					s_W25Q_Length += nWrite;
//
//					_T("烧录APP程序文件(*.bin) : [%08lX] %d / %d, %d",
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

				_T("处理APP配置数据(*.txt) : [%08lX] %d / %d",
						s_W25Q_Address, s_W25Q_Length, tBuffer->length);

				EOS_Buffer_Clear(tBuffer);
			}
			break;
	}
}

/**
 * 重启
 */
static void Cmd_Reset(uint32_t nCmdId, EOTBuffer* tBuffer, int nPos)
{
	// 更改命令行模式 CONFIG_FLAG_INPUT
	F_IAPConfig_Save(CONFIG_FLAG_INPUT);

	EOS_Buffer_Clear(tBuffer);

	_T("执行命令: 系统重启");

	EOB_SystemReset();
}

static void Cmd_Cancel(uint32_t nCmdId, EOTBuffer* tBuffer, int nPos)
{
	EOS_Buffer_Clear(tBuffer);

	_T("执行命令: 命令取消");
	s_CommandFlag = CMD_NONE;
}

/**
 * 一定要记得初始化
 */
void F_Cmd_ProcessInit(void)
{
	_T("初始化命令输入");

	int i;
	TCmdProcess* p;
	for (i=0; i<CMD_PROCESS_SIZE; i++)
	{
		p = &s_ListCmdProcess[i];
		p->id = 0x0;
		p->func = NULL;
	}
}
/**
 * 使用之前记得初始化
 */
void F_Cmd_ProcessExt(uint32_t nCmdId, EOFuncCmdProcess tProcess)
{
	int i;
	TCmdProcess* p;
	for (i=0; i<CMD_PROCESS_SIZE; i++)
	{
		p = &s_ListCmdProcess[i];
		if (p->id == 0x0)
		{
			p->id = nCmdId;
			p->func = tProcess;

			_T("设置扩展输入命令 %08X", nCmdId);
			break;
		}
	}

	if (i >= CMD_PROCESS_SIZE)
	{
		_T("命令设置超出: %08X", nCmdId);
		return;
	}
}

void F_Cmd_ProcessSet(uint32_t nCmdId)
{
	int i;
	TCmdProcess* p = NULL;
	for (i=0; i<CMD_PROCESS_SIZE; i++)
	{
		p = &s_ListCmdProcess[i];
		if (p->id == 0x0) break;
	}

	if (i >= CMD_PROCESS_SIZE)
	{
		_T("命令设置超出: %08X", nCmdId);
		return;
	}

	_T("设置默认输入命令 %08X", nCmdId);
	p->id = nCmdId;

	switch (nCmdId)
	{
		case CMD_JUMP_APP: // japp 跳转到APP
			p->func = (EOFuncCmdProcess)Cmd_JumpApp;
			break;
		case CMD_BIN_BEGIN: // bin1 文件开始
			p->func = (EOFuncCmdProcess)Cmd_BinBegin;
			break;
		case CMD_BIN_END: // bin2 文件结束
			p->func = (EOFuncCmdProcess)Cmd_BinEnd;
			break;
		case CMD_DAT_BEGIN: // dat1 配置开始
			p->func = (EOFuncCmdProcess)Cmd_DatBegin;
			break;
		case CMD_DAT_END: // dat2 配置结束
			p->func = (EOFuncCmdProcess)Cmd_DatEnd;
			break;
		case CMD_CONFIG: // conf 重置配置
			p->func = (EOFuncCmdProcess)Cmd_Config;
			break;
		case CMD_FLASH: // flas 版本烧录
			p->func = (EOFuncCmdProcess)Cmd_Flash;
			break;
		case CMD_RESET: // rest 重启
			p->func = (EOFuncCmdProcess)Cmd_Reset;
			break;
		case CMD_CANCEL: // canc 取消命令
			p->func = (EOFuncCmdProcess)Cmd_Cancel;
			break;
	}
}

/**
 * 处理命令
 */
void F_Cmd_Input(EOTBuffer* tBuffer)
{
	uint8_t* pBuffer = tBuffer->buffer;

	int i, cnt;
	uint32_t n1, n2;

	uint32_t nCmdFlag = CMD_NONE;
	int nPos = 0;

	if (tBuffer->length >= CMD_LENGTH)
	{
		// 命令以####开头，后面带4个字符 @@\r\n换行符结尾
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

	TCmdProcess* p;

	i = CMD_PROCESS_SIZE; // 当无命令时也要处理ProcessData
	if (nCmdFlag != CMD_NONE)
	{
		for (i=0; i<CMD_PROCESS_SIZE; i++)
		{
			p = &s_ListCmdProcess[i];
			if (p->id == nCmdFlag)
			{
				p->func(nCmdFlag, tBuffer, nPos);
				break;
			}
		}
	}

	if (i >= CMD_PROCESS_SIZE)
	{
		ProcessData(tBuffer, nPos);
	}

	// 大量无效数据
	if (tBuffer->length >= DEBUG_LIMIT)
	{
		_T("输入过多无效数据");
		EOS_Buffer_Clear(tBuffer);
	}
}

void F_Cmd_SetFlag(uint32_t nCmdId)
{
	s_CommandFlag = nCmdId;
}

