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

#include "Config.h"

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
static EOTList s_ListConfig;

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
static int ConfigParseLine(char* pLine, int nCount)
{
	if (nCount <= 0) return -1;

	//_Pf("[%d]%s\n", nCount, pLine);

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
		_Pf("配置参数太长 [%d/%d]: [%d]%s = [%d]%s", i, nCount, n1, sKey, n2, sVal);
		return (i + 1);
	}

	F_ConfigAdd(sKey, sVal);
	_Pf("@@[%03d/%03d]: %24s = %s;\r\n", i, nCount, sKey, sVal);

	return (i + 1);
}


// 从W25Q存储中解析配置数据
void F_LoadAppConfig(void)
{
	uint32_t nAddress;
	int nDataLength;

	_Pf("@@-- Config Data Begin\r\n");

	TIAPConfig* tIAPConfig = F_IAPConfig_Get();
	if (tIAPConfig->region == 0)
	{
		nAddress = W25Q_SECTOR_DAT_1;
		nDataLength = tIAPConfig->dat1_length;
	}
	else
	{
		nAddress = W25Q_SECTOR_DAT_2;
		nDataLength = tIAPConfig->dat2_length;
	}

	int i, j, cnt;
	cnt = nDataLength / W25Q_PAGE_SIZE + 1;

	int nRead;
	uint8_t pBuffer[W25Q_PAGE_SIZE];

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
			nCount = ConfigParseLine((char*)&(tBuffer.buffer[nIndex]), nCount);
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

	_Pf("@@-- Config Data End\r\n");
}

/**
 * 保存配置文件
 */
void F_SaveAppConfig(void)
{
	uint32_t nAddress;
	int nDataLength;

	TIAPConfig* tIAPConfig = F_IAPConfig_Get();
	if (tIAPConfig->region == 0)
	{
		nAddress = W25Q_SECTOR_DAT_1;
	}
	else
	{
		nAddress = W25Q_SECTOR_DAT_2;
	}
	nDataLength = 0;

	// 配置只占1个区块 65536	0x10000
	EOB_W25Q_EraseBlock(nAddress);
	_T("擦除地址 %08X", (int)nAddress);

	EOTBuffer tBuffer;
	EOS_Buffer_Create(&tBuffer, SIZE_1K);

	int len;
	EOTKeyValue* pConfigItem;
	EOTListItem* pItem = s_ListConfig._head;
	while (pItem != NULL)
	{
		pConfigItem = (EOTKeyValue*)pItem->data;
		len = sprintf((char*)tBuffer.buffer, "%s=%s;",
				(const char*)pConfigItem->key, (const char*)pConfigItem->value);

		_T("写入 [%08X] %d %s", (int)nAddress, len, tBuffer.buffer);
		//_T("写入 %s=%s;", (const char*)pConfigItem->name, (const char*)pConfigItem->value);

		EOB_W25Q_WriteData(nAddress, tBuffer.buffer, len);
		nAddress += len;
		nDataLength += len;

		pItem = pItem->_next;
	}

	EOS_Buffer_Destory(&tBuffer);

	if (tIAPConfig->region == 0)
	{
		tIAPConfig->dat1_length = nDataLength;
	}
	else
	{
		tIAPConfig->dat2_length = nDataLength;
	}

	// 写入配置
	F_IAPConfig_Save(CONFIG_FLAG_NONE);
}

/**
 * 保存字符串配置
 */
void F_SaveAppConfigString(char* sConfig, int nLength)
{
	uint32_t nAddress;

	if (nLength <= 0)
	{
		// 多写一个字符
		nLength = strlen(sConfig) + 1;
	}

	TIAPConfig* tIAPConfig = F_IAPConfig_Get();
	uint8_t nRegion = tIAPConfig->region;

	if (nRegion == 0)
		nAddress = W25Q_SECTOR_DAT_1;
	else
		nAddress = W25Q_SECTOR_DAT_2;

	// 配置只占1个区块 65536	0x10000
	EOB_W25Q_EraseBlock(nAddress);
	_T("擦除地址 %08X", (int)nAddress);

	int i, len, wcnt;
	char* p = sConfig;
	len = nLength;
	for (i=0; i<SIZE_256; i++)
	{
		// 每次写入固定长度
		wcnt = SIZE_256;
		if (len < wcnt) wcnt = len;
		EOB_W25Q_WriteData(nAddress, p, wcnt);
		nAddress += wcnt;

		len -= wcnt;
		if (len <= 0) break;

		p += wcnt;
	}

	if (nRegion == 0)
		tIAPConfig->dat1_length = nLength;
	else
		tIAPConfig->dat2_length = nLength;

	// 写入配置
	F_IAPConfig_Save(CONFIG_FLAG_NONE);
}

/**
 * 输出到字符串
 */
int F_AppConfigString(char* sOut, int nLength)
{
	int len = 0;
	EOTKeyValue* pConfigItem;
	EOTListItem* pItem = s_ListConfig._head;
	while (pItem != NULL)
	{
		pConfigItem = (EOTKeyValue*)pItem->data;
		len += sprintf(&sOut[len], "%s=%s;",
				(const char*)pConfigItem->key, (const char*)pConfigItem->value);

		//_T("[%d]%s", len, sOut);

		if (len > nLength)
		{
			_D("*** F_AppConfigString 缓存不够");
			return 0;
		}

		pItem = pItem->_next;
	}

	return len;
}

/**
 * 添加一个配置项，如果存在则替换
 */
EOTKeyValue* F_ConfigAdd(char* sName, char* sValue)
{
	return EOS_KeyValue_Set(&s_ListConfig, sName, sValue);
}
void F_ConfigRemove(char* sName)
{
	// 按最大分配，只增不减
}

char* F_ConfigGetString(char* sName, ...)
{
	char sAll[KEY_LENGTH];
	va_list args;
	va_start(args, sName);
	vsnprintf(sAll, KEY_LENGTH, sName, args);
	va_end(args);

	return EOS_KeyValue_Get(&s_ListConfig, sAll);
}
int F_ConfigGetInt32(char* sName, ...)
{
	char sAll[KEY_LENGTH];
	va_list args;
	va_start(args, sName);
	vsnprintf(sAll, KEY_LENGTH, sName, args);
	va_end(args);

	const char* s = EOS_KeyValue_Get(&s_ListConfig, sAll);
	return atoi(s);
}
double F_ConfigGetDouble(char* sName, ...)
{
	char sAll[KEY_LENGTH];
	va_list args;
	va_start(args, sName);
	vsnprintf(sAll, KEY_LENGTH, sName, args);
	va_end(args);

	const char* s = EOS_KeyValue_Get(&s_ListConfig, sAll);
	return atof(s);
}


// 初始化配置文件
void F_AppConfigInit(void)
{
	// 读取IAP配置区
	F_IAPConfig_Load();
	// 如果程序进入此步骤，则反转状态，进入正常模式
	// 否则继续调试模式
	// 将APP标识改为 #define CONFIG_FLAG_NORMAL 0x454F6E72 EOnr 正常模式，直接进入
	F_IAPConfig_Save(CONFIG_FLAG_NORMAL);

	EOB_W25Q_Init();

	F_LoadAppConfig();

	// 测试
//	F_ConfigAdd("verison", "1");
//
//	// 用于串口屏
//	F_ConfigAdd("uart2", "115200,8,0,NONE");
//	// 设备寄存器
//	F_ConfigAdd("uart3", "9600,8,0,NONE");
//	// LED
//	F_ConfigAdd("uart4", "9600,8,0,NONE");
//	// 外接传感器
//	F_ConfigAdd("uart5", "9600,8,0,NONE");
//
//	F_ConfigAdd("server.count", "2");
//	F_ConfigAdd("server1.type", "tcp");
//	F_ConfigAdd("server1.host", "iot.eobjectline.cn");
//	F_ConfigAdd("server1.port", "11898");
//	F_ConfigAdd("server1.protocol", "TcpHJ212");
//	// 发送间隔，和传感器参数对应
//	F_ConfigAdd("server1.tick", "10,60,1440,0");
//	F_ConfigAdd("server1.form", "MN:TEST2021-01,ST:39,a01001:1,a01002:2");
//
//	F_ConfigAdd("server2.type", "tcp");
//	F_ConfigAdd("server2.host", "38.24.41.109");
//	F_ConfigAdd("server2.port", "56909");
//	F_ConfigAdd("server2.protocol", "TcpJson");
//	// 发送间隔，和传感器参数对应
//	F_ConfigAdd("server2.tick", "10,60,1440,0");
//	F_ConfigAdd("server2.form", "DeviceId:TEST2021-01,Temp:1,Humi:2");
//
//	F_ConfigAdd("device.count", "3");
//
//	F_ConfigAdd("device1.type", "UART5");
//	F_ConfigAdd("device1.protocol", "TempHumi");
//	F_ConfigAdd("device1.command", "01 04 00 00 00 02 71 CB");
//
//	F_ConfigAdd("device2.type", "INPUT1");
//	F_ConfigAdd("device2.protocol", "SysSwitch");
//	F_ConfigAdd("device2.command", "0");
//
//	F_ConfigAdd("device3.type", "ADC1");
//	F_ConfigAdd("device3.protocol", "SysADC");
//	F_ConfigAdd("device3.command", "0");
//
//	F_ConfigAdd("sensor.count", "2");
//	F_ConfigAdd("sensor1.name", "温度");
//	// 存储在大寄存器中的参数，寄存器地址（索引），数据类型
//	F_ConfigAdd("sensor1.data", "0,float");
//	// 设备编号，寄存器地址，数据类型，字节顺序
//	F_ConfigAdd("sensor1.device", "1,00,uint16,little");
//	F_ConfigAdd("sensor1.tick", "10,60,1440,0");
//
//	F_ConfigAdd("sensor2.name", "湿度");
//	F_ConfigAdd("sensor2.data", "1,float");
//	F_ConfigAdd("sensor2.device", "1,02,uint16,little");
//	F_ConfigAdd("sensor2.tick", "10,60,1440,0");
//
//	_T("HeapSize = %u\r\n", xPortGetFreeHeapSize());
//	_T("写入测试配置 %d", s_ListConfig.count);
//
//	// 保存应用配置
//	F_SaveAppConfig();
}

