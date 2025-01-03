/*
 * eos_inc.c
 *
 *  Created on: May 9, 2023
 *      Author: hjx
 */

#include "eos_inc.h"

#include "stdint.h"
#include "string.h"
#include "stdlib.h"
#include "stdio.h"

/**
 * 检查延时，需要一个静态变量
 */
uint8_t EOG_Check_Delay(uint64_t* last, uint64_t tick, uint64_t delay)
{
	if (tick < ((*last) + delay)) return EO_FALSE;

	(*last) = tick;
	return EO_TRUE;
}

// 判断是否是数字
uint8_t EOG_CheckNumber(char* sData, int nLength)
{
	if (nLength < 0) nLength = 0x1FFF;
	uint8_t ret = EO_FALSE;

	int i = 0;
	for (i=0; i<nLength; i++)
	{
		if (sData[i] == '\0') break;
		if (sData[i] < 0x30 || sData[i] > 0x39) return EO_FALSE;

		ret = EO_TRUE;
	}

	return ret;
}
// 判断是否是IP
uint8_t EOG_CheckIP(char* sData, int nLength)
{
	return EO_FALSE;
}

// 去除两端空白
char* EOG_Trim(char* sStr, int nLength)
{
	if (sStr == NULL) return NULL;
	if (nLength < 0) nLength = 0x1FFF;

	char* pStart = NULL;
	int nPos = 0;
	int nFlag = 0; // 标记开始

	int i = 0;
	for (i=0; i<nLength; i++)
	{
		if (nFlag == 0) pStart = &sStr[i];
		if (sStr[i] == '\0') break;

		if (sStr[i] == ' ' ||
				sStr[i] == '\t' ||
				sStr[i] == '\v' ||
				sStr[i] == '\f' ||
				sStr[i] == '\r' ||
				sStr[i] == '\n')
		{
			if (nFlag == 1)
			{
				sStr[nPos] = '\0';
				return pStart;
			}

			continue;
		}

		nFlag = 1;
		if (nPos != i)
		{
			sStr[nPos] = sStr[i];
		}
		nPos++;
	}

	return pStart;
}

// 拆分IP地址和端口 xxx.xxxxx.xxx:0000
int EOG_SplitIP(char* sData, int nLength, char* sHost, uint16_t* nPort)
{
	char val;
	char* p = sData;
	int i, j;

	uint8_t nFlag = 0;
	char sPort[SIZE_8];

	sHost[0] = '\0';
	*nPort = 0;

	j = 0;
	for (i=0; i<nLength; i++)
	{
		val = *p;
		++p;

		if (val == '\0') break;

		if (val == ' ' ||
				val == '\t' ||
				val == '\v' ||
				val == '\f' ||
				val == '\r' ||
				val == '\n')
		{
			continue;
		}

		if (val == ':')
		{
			sHost[j] = '\0';
			nFlag = 1;
			j = 0;

			continue;
		}

		if (nFlag == 0)
		{
			if (j == SIZE_32)
			{
				return -1;
			}
			sHost[j] = val;
		}
		else
		{
			if (j == SIZE_8)
			{
				return -1;
			}
			sPort[j] = val;
		}
		++j;
	}

	if (nFlag == 0)
	{
		return -1;
	}

	sPort[j] = '\0';
	*nPort = atoi(sPort);

	return 0;
}

// Strin转换HEX
uint32_t EOG_String2Hex(char* sStr, int nLength)
{
	uint8_t d1, d2, d3, d4, d5, d6, d7, d8;

	if (nLength != 8 && nLength != 4 && nLength != 2)
	{
		return 0xFFFFFFFF;
	}

	d1 = sStr[0];
	d2 = sStr[1];

	CHAR_HEX(d1);
	CHAR_HEX(d2);

	if (nLength == 8)
	{
		d3 = sStr[2];
		d4 = sStr[3];
		d5 = sStr[4];
		d6 = sStr[5];
		d7 = sStr[6];
		d8 = sStr[7];

		CHAR_HEX(d3);
		CHAR_HEX(d4);
		CHAR_HEX(d5);
		CHAR_HEX(d6);
		CHAR_HEX(d7);
		CHAR_HEX(d8);

		return (d1<<28)+(d2<<24)+(d3<<20)+(d4<<16)+(d5<<12)+(d6<<8)+(d7<<4)+d8;
	}
	else if (nLength == 4)
	{
		d3 = sStr[2];
		d4 = sStr[3];

		CHAR_HEX(d3);
		CHAR_HEX(d4);

		return (d1<<12)+(d2<<8)+(d3<<4)+(d4);
	}
	else
	{
		return (d1<<4)+d2;
	}
}


// 查找一行\r\n
char* EOG_GetLine(char* sStr, char* sOut)
{
	sOut[0] = '\0';

	if (sStr == NULL) return NULL;

	char* p = strstr(sStr, "\r\n");
	if (p == NULL) return NULL;

	int len = p - sStr;
	if (len > 0) strncpy(sOut, sStr, len);
	sOut[len] = '\0';

	return p + 2;
}

// 查找字符串
char* EOG_FindString2(char* sStr, int nLen, char* sOut, char* sStart, char* sEnd)
{
	sOut[0] = '\0';

	if (sStr == NULL) return NULL;

	// strnlen
	int len = strlen(sStr);
	if (len <= 0) return NULL;

	char* p1 = (char*)sStr;
	char* p2 = (char*)&sStr[len]; // 最后一个字符\0

	if (sStart != NULL)
	{
		p1 = strnstr(p1, sStart, len);
		if (p1 != NULL)
		{
			int n = strlen(sStart);
			p1 += n;
			len -= n;
		}
	}
	if (p1 == NULL) return NULL;

	if (sEnd != NULL)
	{
		p2 = strnstr(p1, sEnd, len);
		if (p2 == NULL) return NULL;

		len = p2 - p1;
	}

	if (len > 0) strncpy(sOut, p1, len);
	sOut[len] = '\0';

	// 返回结束位置
	return p2;
}

/**
 * 自定义查找字符串
 * 被查找的对象可能是混合数据（存在非显示字符）
 */
char* EOG_FindString(char* sStr, int nLen, char* sOut, char* sStart, char* sEnd)
{
	sOut[0] = '\0';

	if (sStr == NULL) return NULL;
	if (sStart == NULL)
	{
		printf("*** EOG_FindString start is null\r\n");
		return NULL;
	}

	int len = nLen;

	// 按字符串处理
	if (len < 0) len = strnlen(sStr, SIZE_4K);
	if (len <= 0) return NULL;

	char* p1 = (char*)sStr;
	char* p2 = p1;

	int j, jcnt;
	int i, icnt;

	// 先查找开始标识
	icnt = strlen(sStart);
	jcnt = len - icnt + 1; // 全部遍历

	char* pt;
	char* p;
	for (j=0; j<jcnt; j++)
	{
		// 每次比对完重置
		p = (char*)sStart;
		pt = p1;
		for (i=0; i<icnt; i++)
		{
			if ((*p) != (*pt)) break;
			++p;
			++pt;
		}
		// 比对成功
		if (i >= icnt) break;
		++p1;
	}

	// 未找到
	if (j >= jcnt) return NULL;

	p1 += icnt;
	len -= (j + icnt);

	// 继续查找结束标识
	if (sEnd != NULL)
	{
		icnt = strlen(sEnd);
		jcnt = len - icnt + 1;

		p2 = p1;
		//printf("[%d - %d]%d %s\r\n", jcnt, icnt, len, p2);
		for (j=0; j<jcnt; j++)
		{
			p = (char*)sEnd;
			pt = p2;
			for (i=0; i<icnt; i++)
			{
				if ((*p) != (*pt)) break;
				++p;
				++pt;
			}

			// 比对成功
			if (i >= icnt) break;
			++p2;
		}

		// 未找到
		if (j >= jcnt) return NULL;

		len = p2 - p1;
		p2 += icnt; // p2跳过结束符
	}

	//printf("[1]%s\r\n[2]%s\r\n[3]%s\r\n%d\r\n", sStr, sStart, p1, len);

	if (len > 0) strncpy(sOut, p1, len);
	sOut[len] = '\0';

	// 返回结束位置，包含结束符
	return p2;
}


// 破坏性分隔
// 会改变 sData，设置\0
// sData必须以 \0 结束
// nLength 不包括 \0
// nCount 可以设置最大分割数量，如果为0则无限制
int EOG_SplitString(char* sData, int nLength, char cSplit, char** ppArray, uint8_t *nCount)
{
	unsigned char nMax = *nCount;
	if (nMax == 0) nMax = 0xFF;

	char* p = sData;
	if (nLength <= 0) nLength = strlen(sData);

	unsigned char cnt = 0;
	int i;

	ppArray[cnt] = p;
	for (i=0; i<nLength; i++)
	{
		if (*p == cSplit)
		{
			*p = '\0';

			if (cnt >= nMax) break;

			cnt++;
			ppArray[cnt] = p + 1;
		}

		p++;
	}

	cnt++;
	*nCount = cnt;

	//printf("\r\n\r\n******** ********\r\n%s\r\n%d\r\n\r\n", sData, cnt);

	return cnt;
}

int EOG_TrimChar(char* sData, int nLength, char cTrim)
{
	int i;
	char* p1 = sData;
	char* p2 = sData;
	for (i=0; i<nLength - 1; i++)
	{
		++p2;
		if ((*p1) != cTrim)
		{
			++p1;
		}
		(*p1) = (*p2);
	}

	return 0;
}

/**
 * 一分为二
 * 不破环sStr原有数据，复制到sKey和sValue，注意长度
 */
void EOG_KeyValue(char* sStr, int nLenStr,
		char cSplit, char cEnd,
		char* sKey, int nLenKey,
		char* sVal, int nLenVal)
{
	int i;
	char* p1 = sKey;
	char* p2 = sVal;

	uint8_t flag = 0;
	char c;
	char* p = sStr;
	int len = nLenStr;
	if (nLenStr <= 0) len = strlen(sStr);

	int n1 = 0;
	int n2 = 0;

	// 包含结束符
	for (i=0; i<len+1; i++)
	{
		c = *p;
		if (flag == 0)
		{
			if (c == cSplit)
			{
				*p1 = '\0';
				flag = 1;
				++n1;
			}
			else
			{
				*p1 = c;
				++p1;
				++n1;
			}
		}
		else if (flag == 1)
		{
			if (c == cEnd)
			{
				*p2 = '\0';
				flag = 2;
				++n2;
			}
			else
			{
				*p2 = c;
				++p2;
				++n2;
			}
		}
		++p;
	}

	if (flag != 2)
	{
		printf("*** EOG_KeyValue no flag = %d\r\n", flag);
		return;
	}

	if (n1 > nLenKey || n2 > nLenVal)
	{
		printf("*** EOG_KeyValue overflow = %d/%d, %d/%d\r\n", n1, nLenKey, n2, nLenVal);
		return;
	}
}

/**
 * 直接分割，破坏
 */
void EOG_KeyValueChar(char* sStr, char cSplit, char** ppKey, char** ppVal)
{
	*ppKey = NULL;
	*ppVal = NULL;

	char* p = strchr(sStr, cSplit);
	if (p == NULL) return;

	*p = '\0';
	*ppKey = sStr;

	++p;
	*ppVal = p;
}

static char s_Base64_Encode[] =
{
		'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
		'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
		'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
		'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
		'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
		'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
		'w', 'x', 'y', 'z', '0', '1', '2', '3',
		'4', '5', '6', '7', '8', '9', '+', '/',
};
static uint8_t s_Base64_Decode[] =
{
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3E, 0x00, 0x00, 0x00, 0x3F,
		0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
		0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
		0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00,

		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

/**
 * 字节流转换为base64
 */
int EOG_Base64Encode(uint8_t* pBuffer, int nLength, char* pBase64, char cFlag)
{
	int i, count, retain;
	count = nLength / 3;
	retain = nLength % 3;

	int v24; // 24位

	int k1, k2;
	k1 = 0;
	k2 = 0;
	for (i=0; i<count; i++)
	{
		v24 = (pBuffer[k1] << 16) | (pBuffer[k1+1] << 8) | pBuffer[k1+2];

		pBase64[k2] = s_Base64_Encode[(v24 >> 18) & 0x3F];
		pBase64[k2+1] = s_Base64_Encode[(v24 >> 12) & 0x3F];
		pBase64[k2+2] = s_Base64_Encode[(v24 >> 6) & 0x3F];
		pBase64[k2+3] = s_Base64_Encode[(v24) & 0x3F];

		k1 += 3;
		k2 += 4;
	}

	if (retain == 2)
	{
		v24 = (pBuffer[k1] << 16) | (pBuffer[k1+1] << 8);

		pBase64[k2] = s_Base64_Encode[(v24 >> 18) & 0x3F];
		pBase64[k2+1] = s_Base64_Encode[(v24 >> 12) & 0x3F];
		pBase64[k2+2] = s_Base64_Encode[(v24 >> 6) & 0x3F];
		pBase64[k2+3] = cFlag;

		k1 += 2;
		k2 += 4;
	}
	else if (retain == 1)
	{
		v24 = (pBuffer[k1] << 16);

		pBase64[k2] = s_Base64_Encode[(v24 >> 18) & 0x3F];
		pBase64[k2+1] = s_Base64_Encode[(v24 >> 12) & 0x3F];
		pBase64[k2+2] = cFlag;
		pBase64[k2+3] = cFlag;

		k1 += 1;
		k2 += 4;
	}

	// 转换字符串结束符
	pBase64[k2] = '\0';

	return k2;
}
/**
 * base64转换为字节流
 */
int EOG_Base64Decode(char* pBase64, int nLength, uint8_t* pBuffer, char cFlag)
{
	if ((nLength % 4) != 0)
	{
		// 错误的编码
		printf("*** EOG_Base64Decode error code = %d\r\n", nLength);
		return 0;
	}

	int n1, n2, n3, n4;
	int v24; // 24位

	int i, count;
	count = nLength / 4 - 1;

	int k1, k2;
	k1 = 0;
	k2 = 0;
	for (i=0; i<count; i++)
	{
		n1 = s_Base64_Decode[(uint8_t)pBase64[k1]] & 0x3F;
		n2 = s_Base64_Decode[(uint8_t)pBase64[k1+1]] & 0x3F;
		n3 = s_Base64_Decode[(uint8_t)pBase64[k1+2]] & 0x3F;
		n4 = s_Base64_Decode[(uint8_t)pBase64[k1+3]] & 0x3F;

		v24 = (n1 << 18) | (n2 << 12) | (n3 << 6) | n4;

		pBuffer[k2] = (v24 >> 16) & 0xFF;
		pBuffer[k2+1] = (v24 >> 8) & 0xFF;
		pBuffer[k2+2] = (v24) & 0xFF;

		k1 += 4;
		k2 += 3;
	}

	count = k2;
	n1 = s_Base64_Decode[(uint8_t)pBase64[k1]] & 0x3F;
	n2 = s_Base64_Decode[(uint8_t)pBase64[k1+1]] & 0x3F;
	count++;
	n3 = 0;
	if (pBase64[k1+2] != cFlag)
	{
		n3 = s_Base64_Decode[(uint8_t)pBase64[k1+2]] & 0x3F;
		count++;
	}
	n4 = 0;
	if (pBase64[k1+3] != cFlag)
	{
		n4 = s_Base64_Decode[(uint8_t)pBase64[k1+3]] & 0x3F;
		count++;
	}

	v24 = (n1 << 18) | (n2 << 12) | (n3 << 6) | n4;

	pBuffer[k2] = (v24 >> 16) & 0xFF;
	pBuffer[k2+1] = (v24 >> 8) & 0xFF;
	pBuffer[k2+2] = (v24) & 0xFF;

	k1 += 4;
	k2 += 3;

	return count;
}
