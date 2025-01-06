/*
 * Config.c
 *
 *  Created on: Jun 28, 2023
 *      Author: hjx
 *
 * �����ļ��Ͱ汾bin�ļ�������W25QоƬ֮�У����ý���洢�ķ�ʽ������������Իָ�����һ�汾
 * �洢λ�ö�����IAPConfig.h��
 *
 * ������Ϣ��ASCII�ı��ķ�ʽ�洢����ʽ���� title.name.subname=value;
 * ��ֵ���йؼ��֣������ת�� =��Ӧ&H61��;��Ӧ&H59
 *
 * ���õĲ������Ƴ��Ȳ��ܳ���CONFIG_NAME_MAX��ֵ���Ȳ��ܳ���CONFIG_VALUE_MAX
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




// ���ò����б�
static EOTList s_ListSetting;

///**
// * ����һ��hashCode������ÿ�ζ�У���ַ���
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
 * ���н���
 * �ƻ�������ı�ԭ������
 * �Էֺ�;�ָ�
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

		// ȥ���ո񣬻��еȷ���
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
				p1 = (p-1); // ��¼��ַ ++p
				cFlag = 2; // key����
				sVal = p; // val��ʼ
			}
		}
		else if (cFlag == 2)
		{
			if (cVal == ';')
			{
				p2 = (p-1); // ��¼��ַ ++p
				cFlag = 3; // val����
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
		_T("���ò���̫�� [%d/%d]: [%d]%s = [%d]%s", i, nCount, n1, sKey, n2, sVal);
		return (i + 1);
	}

	tpList = &s_ListSetting;
	// ��ӵ�������
	EOS_KeyValue_Set(tpList, sKey, sVal);

	_P("@@[%03d/%03d]: %24s = %s;\r\n", i, nCount, sKey, sVal);

	return (i + 1);
}


/**
 * ��W25Q�洢�н�����������
 */
void F_LoadAppSetting(void)
{
	uint32_t nAddress;
	int nDataLength;

	int nRead;
	uint8_t pBuffer[W25Q_PAGE_SIZE];

	TIAPConfig* tIAPConfig = F_IAPConfig_Get();

	W25Q_ADDRESS_DAT(nAddress, tIAPConfig->region);

	// ��һҳΪԤ������, ͷ4���ֽ�Ϊ����
	READ_HEAD_LENGTH(nAddress, pBuffer, nDataLength);
	nAddress += W25Q_PAGE_SIZE;

	int i, j, cnt;
	cnt = nDataLength / W25Q_PAGE_SIZE + 1;

	_T("�������ò��� [%08lX] %d", nAddress, nDataLength);

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

		// ����������������Ҫ��1
		EOS_Buffer_Pop(&tBuffer, NULL, nIndex);

		if (nDataLength <= 0) break;

		nDataLength -= nRead;
		nAddress += nRead;
	}

	EOS_Buffer_Destory(&tBuffer);

	_P("@@-- Setting End\r\n");
}

/**
 * ���������ļ�
 */
//static void SaveAppSetting(void)
//{
//	uint32_t nAddress, nHeadAddress;
//	int nDataLength;
//
//	TIAPConfig* tIAPConfig = F_IAPConfig_Get();
//	W25Q_ADDRESS_DAT(nAddress, tIAPConfig->region);
//
//	// ����ֻռ1������ 65536	0x10000
//	EOB_W25Q_EraseBlock(nAddress);
//	_T("������ַ %08X", (int)nAddress);
//
//	EOTBuffer tBuffer;
//	EOS_Buffer_Create(&tBuffer, SIZE_1K);
//
//	// ��һҳΪԤ�����ã�������
//	nHeadAddress = nAddress;
//	nAddress += W25Q_PAGE_SIZE;
//	// д���֪������
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
//		_T("д�� [%08X] %d %s", (int)nAddress, len, tBuffer.buffer);
//		//_T("д�� %s=%s;", (const char*)pConfigItem->name, (const char*)pConfigItem->value);
//
//		EOB_W25Q_WriteData(nAddress, tBuffer.buffer, len);
//		nAddress += len;
//		nDataLength += len;
//
//		pItem = pItem->_next;
//	}
//
//	// д��һ��0
//	uint8_t b = 0x0;
//	len = 1;
//	EOB_W25Q_WriteData(nAddress, &b, len);
//	nAddress += len;
//	nDataLength += len;
//
//	// ��һҳΪԤ������
//
//	// ͷ4���ֽ�Ϊ����
//	*((int*)tBuffer.buffer) = nDataLength;
//	EOB_W25Q_WriteDirect(nHeadAddress, tBuffer.buffer, W25Q_PAGE_SIZE);
//
//	EOS_Buffer_Destory(&tBuffer);
//}

/**
 * д�����ÿ�ʼ
 */
//void F_AppSettingSaveBegin(void)
//{
//	TIAPConfig* tIAPConfig = F_IAPConfig_Get();
//
//	int nAddress;
//	W25Q_ADDRESS_DAT(nAddress, tIAPConfig->region);
//
//	// ����ֻռ1������ 65536	0x10000
//	EOB_W25Q_EraseBlock(nAddress);
//	_T("������ַ %08X", (int)nAddress);
//}

/**
 * д�����ý���
 */
//void F_AppSettingSaveEnd(int nLength)
//{
//	TIAPConfig* tIAPConfig = F_IAPConfig_Get();
//
//	uint8_t pBuffer[W25Q_PAGE_SIZE];
//	int nAddress;
//	W25Q_ADDRESS_DAT(nAddress, tIAPConfig->region);
//
//	// ��һҳΪԤ������, ͷ4���ֽ�Ϊ����
//	WRITE_HEAD_LENGTH(nAddress, pBuffer, nLength);
//}

/**
 * �������ַ���ֱ�ӱ��浽W25Q��
 * @praram nOffset W25Qƫ�ƣ�����һ�ε�λ�ü����洢
 * @praram sSetting �ַ���
 * @praram nLength ����
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
//	// ��һҳΪԤ������, ͷ4���ֽ�Ϊ����
//	nAddress += W25Q_PAGE_SIZE + nOffset;
//
//	int i, len, wcnt;
//	char* p = sSetting;
//	len = nLength;
//	for (i=0; i<SIZE_256; i++)
//	{
//		// ÿ��д��̶�����
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
 * �����ò���������ַ���
 * @param nSet ��Ӧ�ڵ�
 * @return ����
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
			_D("*** F_AppSettingString ���治��");
			return 0;
		}

		pItem = pItem->_next;
	}

	return len;
}

/**
 * ���һ�����������������滻
 */
EOTKeyValue* F_SettingAdd(char* sName, char* sValue)
{
	return EOS_KeyValue_Set(&s_ListSetting, sName, sValue);
}
/**
 * �ݲ�֧��
 */
void F_SettingRemove(char* sName)
{
	// �������䣬ֻ������
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
 * ��ʼ�������ļ�
 * ��ȡ��̬���úͶ�̬����
 */
void F_AppSettingInit(void)
{
	// ��ȡIAP������
	F_IAPConfig_Load();
	// ����������˲��裬��ת״̬����������ģʽ
	// �����������ģʽ
	// ��APP��ʶ��Ϊ #define CONFIG_FLAG_NORMAL 0x454F6E72 EOnr ����ģʽ��ֱ�ӽ���
	F_IAPConfig_Save(CONFIG_FLAG_NORMAL);

	EOB_W25Q_Init();

	F_LoadAppSetting();
}

