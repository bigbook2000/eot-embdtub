/*
 * Config.c
 *
 *  Created on: Jun 28, 2023
 *      Author: hjx
 *
 * 配置文件和版本bin文件保存在W25Q芯片之中，采用交替存储的方式，出现问题可以恢复到上一版本
 * 存储位置定义在IAPConfig.h中
 *
 * 配置信息以ASCII文本的方式存储，格式类似 title.name.subname=value;
 * 若值中有关键字，请进行转义 =对应&H61，;对应&H59
 *
 * 配置的参数名称长度不能超过CONFIG_NAME_MAX，值长度不能超过CONFIG_VALUE_MAX
 *
 */

#include "AppSetting.h"

#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

#include "eos_inc.h"
#include "eos_list.h"

#include "eob_debug.h"
#include "eob_uart.h"
#include "eob_w25q.h"

#include "cmsis_os.h"

#include "IAPConfig.h"
#include "IAPCmd.h"




// 配置参数列表
static EOTList s_ListSetting;

///**
// * 计算一个hashCode，避免每次都校验字符串
// */
//static int ConfigHashCode(char* sStr, int nLen)
//{
//	int h = 0;
//	if (nLen < 0) nLen = strnlen(sStr, CONFIG_NAME_MAX);
//	int i;
//	for (i=0; i<nLen; i++)
//	{
//		h = 31 * h + (uint8_t)sStr[i];
//	}
//
//	return h;
//}

/**
 * 单行解析
 * 破坏处理，会改变原有数据
 * 以分号;分割
 */
static int SettingParseLine(char* pLine, int nCount)
{
	if (nCount <= 0) return -1;

	//_Pf("[%d]%s\n", nCount, pLine);

	EOTList* tpList;

	char cFlag = 0;

	char cVal;
	char* p = pLine;

	char* sKey = NULL;
	char* sVal = NULL;

	int i;
	int n1 = 0;
	int n2 = 0;
	char* p1 = NULL;
	char* p2 = NULL;
	for (i=0; i<nCount; i++)
	{
		cVal = *p;
		++p;

		// 去掉空格，换行等符号
		if (CHAR_BLANK(cVal)) continue;

		if (cFlag == 0)
		{
			cFlag = 1;
			sKey = p - 1; // ++p
		}
		else if (cFlag == 1)
		{
			++n1;
			if (cVal == '=')
			{
				p1 = (p-1); // 记录地址 ++p
				cFlag = 2; // key结束
				sVal = p; // val开始
			}
		}
		else if (cFlag == 2)
		{
			if (cVal == ';')
			{
				p2 = (p-1); // 记录地址 ++p
				cFlag = 3; // val结束
				break;
			}
			++n2;
		}
	}

	if (cFlag != 3) return -1;

	*p1 = '\0';
	*p2 = '\0';
	if (n1 >= KEY_LENGTH || n2 >= VALUE_SIZE)
	{
		_T("配置参数太长 [%d/%d]: [%d]%s = [%d]%s", i, nCount, n1, sKey, n2, sVal);
		return (i + 1);
	}

	tpList = &s_ListSetting;
	// 添加到队列中
	EOS_KeyValue_Set(tpList, sKey, sVal);

	_P("@@[%03d/%03d]: %24s = %s;\r\n", i, nCount, sKey, sVal);

	return (i + 1);
}


/**
 * 从W25Q存储中解析配置数据
 */
void F_LoadAppSetting(void)
{
	uint32_t nAddress;
	int nDataLength;

	int nRead;
	uint8_t pBuffer[W25Q_PAGE_SIZE];

	TIAPConfig* tIAPConfig = F_IAPConfig_Get();

	W25Q_ADDRESS_DAT(nAddress, tIAPConfig->region);

	// 第一页为预留配置, 头4个字节为长度
	READ_HEAD_LENGTH(nAddress, pBuffer, nDataLength);
	nAddress += W25Q_PAGE_SIZE;

	int i, j, cnt;
	cnt = nDataLength / W25Q_PAGE_SIZE + 1;

	_T("加载配置参数 [%08lX] %d", nAddress, nDataLength);

	_P("@@-- Setting Begin\r\n");

	EOTBuffer tBuffer;
	EOS_Buffer_Create(&tBuffer, SIZE_1K);

	int nIndex, nCount;
	for (i=0; i<cnt; i++)
	{
		if (nDataLength > 0)
		{
			nRead = W25Q_PAGE_SIZE;
			if (nRead > nDataLength) nRead = nDataLength;

			EOB_W25Q_ReadDirect(nAddress, pBuffer, nRead);
			EOS_Buffer_Push(&tBuffer, pBuffer, nRead);
		}

		nIndex = 0;
		nCount = tBuffer.length;
		for (j=0; j<0xFF; j++)
		{
			nCount = SettingParseLine((char*)&(tBuffer.buffer[nIndex]), nCount);
			if (nCount <= 0) break;

			nIndex += nCount;
			nCount = tBuffer.length - nIndex;
		}

		// 索引就是数量，不要加1
		EOS_Buffer_Pop(&tBuffer, NULL, nIndex);

		if (nDataLength <= 0) break;

		nDataLength -= nRead;
		nAddress += nRead;
	}

	EOS_Buffer_Destory(&tBuffer);

	_P("@@-- Setting End\r\n");
}

/**
 * 保存配置文件
 */
//static void SaveAppSetting(void)
//{
//	uint32_t nAddress, nHeadAddress;
//	int nDataLength;
//
//	TIAPConfig* tIAPConfig = F_IAPConfig_Get();
//	W25Q_ADDRESS_DAT(nAddress, tIAPConfig->region);
//
//	// 配置只占1个区块 65536	0x10000
//	EOB_W25Q_EraseBlock(nAddress);
//	_T("擦除地址 %08X", (int)nAddress);
//
//	EOTBuffer tBuffer;
//	EOS_Buffer_Create(&tBuffer, SIZE_1K);
//
//	// 第一页为预留配置，先跳过
//	nHeadAddress = nAddress;
//	nAddress += W25Q_PAGE_SIZE;
//	// 写完才知道长度
//	nDataLength = 0;
//
//	int len;
//	EOTKeyValue* pConfigItem;
//	EOTListItem* pItem;
//
//	pItem = s_ListSetting._head;
//	while (pItem != NULL)
//	{
//		pConfigItem = (EOTKeyValue*)pItem->data;
//
//		len = sprintf((char*)tBuffer.buffer, "%s=%s;",
//				(const char*)pConfigItem->key, (const char*)pConfigItem->value);
//
//		_T("写入 [%08X] %d %s", (int)nAddress, len, tBuffer.buffer);
//		//_T("写入 %s=%s;", (const char*)pConfigItem->name, (const char*)pConfigItem->value);
//
//		EOB_W25Q_WriteData(nAddress, tBuffer.buffer, len);
//		nAddress += len;
//		nDataLength += len;
//
//		pItem = pItem->_next;
//	}
//
//	// 写入一个0
//	uint8_t b = 0x0;
//	len = 1;
//	EOB_W25Q_WriteData(nAddress, &b, len);
//	nAddress += len;
//	nDataLength += len;
//
//	// 第一页为预留配置
//
//	// 头4个字节为长度
//	*((int*)tBuffer.buffer) = nDataLength;
//	EOB_W25Q_WriteDirect(nHeadAddress, tBuffer.buffer, W25Q_PAGE_SIZE);
//
//	EOS_Buffer_Destory(&tBuffer);
//}

/**
 * 写入配置开始
 */
//void F_AppSettingSaveBegin(void)
//{
//	TIAPConfig* tIAPConfig = F_IAPConfig_Get();
//
//	int nAddress;
//	W25Q_ADDRESS_DAT(nAddress, tIAPConfig->region);
//
//	// 配置只占1个区块 65536	0x10000
//	EOB_W25Q_EraseBlock(nAddress);
//	_T("擦除地址 %08X", (int)nAddress);
//}

/**
 * 写入配置结束
 */
//void F_AppSettingSaveEnd(int nLength)
//{
//	TIAPConfig* tIAPConfig = F_IAPConfig_Get();
//
//	uint8_t pBuffer[W25Q_PAGE_SIZE];
//	int nAddress;
//	W25Q_ADDRESS_DAT(nAddress, tIAPConfig->region);
//
//	// 第一页为预留配置, 头4个字节为长度
//	WRITE_HEAD_LENGTH(nAddress, pBuffer, nLength);
//}

/**
 * 将配置字符串直接保存到W25Q中
 * @praram nOffset W25Q偏移，从上一次的位置继续存储
 * @praram sSetting 字符串
 * @praram nLength 长度
 *
 */
//int F_AppSettingStringSave(int nOffset, char* sSetting, int nLength)
//{
//	uint32_t nAddress;
//	if (nLength <= 0) nLength = strlen(sSetting);
//
//	TIAPConfig* tIAPConfig = F_IAPConfig_Get();
//
//	W25Q_ADDRESS_DAT(nAddress, tIAPConfig->region);
//	// 第一页为预留配置, 头4个字节为长度
//	nAddress += W25Q_PAGE_SIZE + nOffset;
//
//	int i, len, wcnt;
//	char* p = sSetting;
//	len = nLength;
//	for (i=0; i<SIZE_256; i++)
//	{
//		// 每次写入固定长度
//		wcnt = SIZE_256;
//		if (len < wcnt) wcnt = len;
//		EOB_W25Q_WriteData(nAddress, p, wcnt);
//		nAddress += wcnt;
//
//		len -= wcnt;
//		if (len <= 0) break;
//
//		p += wcnt;
//	}
//
//	return nLength;
//}

/**
 * 将配置参数输出到字符串
 * @param nSet 对应节点
 * @return 长度
 */
int F_AppSettingString(char* sOut, int nLength)
{
	char* sKey;
	char* sVal;
	int len = 0;
	EOTKeyValue* pConfigItem;

	EOTListItem* pItem = s_ListSetting._head;
	while (pItem != NULL)
	{
		pConfigItem = (EOTKeyValue*)pItem->data;

		sKey = pConfigItem->key;
		sVal = pConfigItem->value;

		len += sprintf(&sOut[len], "%s=%s;",
				(const char*)sKey, (const char*)sVal);

		if (len > nLength)
		{
			_D("*** F_AppSettingString 缓存不够");
			return 0;
		}

		pItem = pItem->_next;
	}

	return len;
}

/**
 * 添加一个配置项，如果存在则替换
 */
EOTKeyValue* F_SettingAdd(char* sName, char* sValue)
{
	return EOS_KeyValue_Set(&s_ListSetting, sName, sValue);
}
/**
 * 暂不支持
 */
void F_SettingRemove(char* sName)
{
	// 按最大分配，只增不减
}

char* F_SettingGetString(char* sName, ...)
{
	char sAll[KEY_LENGTH];
	va_list args;
	va_start(args, sName);
	vsnprintf(sAll, KEY_LENGTH, sName, args);
	va_end(args);

	return EOS_KeyValue_Get(&s_ListSetting, sAll);
}
int F_SettingGetInt32(char* sName, ...)
{
	char sAll[KEY_LENGTH];
	va_list args;
	va_start(args, sName);
	vsnprintf(sAll, KEY_LENGTH, sName, args);
	va_end(args);

	const char* s = EOS_KeyValue_Get(&s_ListSetting, sAll);
	return atoi(s);
}
double F_SettingGetDouble(char* sName, ...)
{
	char sAll[KEY_LENGTH];
	va_list args;
	va_start(args, sName);
	vsnprintf(sAll, KEY_LENGTH, sName, args);
	va_end(args);

	const char* s = EOS_KeyValue_Get(&s_ListSetting, sAll);
	return atof(s);
}


/**
 * 初始化配置文件
 * 读取静态配置和动态配置
 */
void F_AppSettingInit(void)
{
	// 读取IAP配置区
	F_IAPConfig_Load();
	// 如果程序进入此步骤，则反转状态，进入正常模式
	// 否则继续调试模式
	// 将APP标识改为 #define CONFIG_FLAG_NORMAL 0x454F6E72 EOnr 正常模式，直接进入
	F_IAPConfig_Save(CONFIG_FLAG_NORMAL);

	EOB_W25Q_Init();

	F_LoadAppSetting();
}

