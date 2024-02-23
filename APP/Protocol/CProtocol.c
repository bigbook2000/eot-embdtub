/*
 * CProtocol.c
 *
 *  Created on: Jan 20, 2024
 *      Author: hjx
 */

#include "CProtocol.h"

#include <string.h>

#include "eob_debug.h"

#include "cmsis_os.h"

// 串行执行，所以不考虑同时访问，从堆上申请较大内存
char* s_SendBuffer = NULL;

void F_Protocol_Init(void)
{
	s_SendBuffer = pvPortMalloc(SEND_MAX);
}

char* F_Protocol_SendBuffer(void)
{
	return s_SendBuffer;
}

/**
 * 判断协议类型，大小写相关
 * 和_emProtocolType对应
 */
void F_Protocol_Type(TNetChannel* pNetChannel, char* sType)
{
	pNetChannel->protocol = ProtocolTypeNone;

	F_Protocol_TcpHJ212_Init(pNetChannel, sType);
	F_Protocol_TcpJson_Init(pNetChannel, sType);

	if (pNetChannel->protocol == ProtocolTypeNone)
	{
		_T("未配置协议");
	}
}

/**
 * 标准 HJ212 CRC 16
 * @param data 数据
 * @return 校验码
 */
uint16_t F_Protocol_HJ212CRC16(uint8_t* sData, int nLen)
{
	uint16_t crc = 0xFFFF;
	uint16_t check;
	int i, j;

	//_T("计算[%d/%d]: %s", nLen, (int)strlen((const char*)sData), (const char*)sData);
	if (nLen <= 0) nLen = strlen((const char*)sData);

	for (i=0; i<nLen; i++)
	{
		crc = (crc >> 8) ^ sData[i];
		for (j = 0; j < 8; j++)
		{
			check = crc & 0x0001;
			crc >>= 1;
			if (check == 0x0001)
			{
				crc ^= 0xA001;
			}
		}
	}

	return crc;
}

