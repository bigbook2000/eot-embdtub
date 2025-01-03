/*
 * Config.h
 *
 *  Created on: Jun 30, 2023
 *      Author: hjx
 */

#ifndef CONFIG_H_
#define CONFIG_H_

#include <stdint.h>

#include "eos_keyvalue.h"
#include "eob_flash.h"

#include "IAPConfig.h"

/**
 * ʹ��4����ĸ����ʾ����
 * �����ж��壬��ͬ�Ĵ��룬���޸�
 */
#define __APP_BIN_TYPE 			"EOTB"
/**
 * ʹ��3����������ʾ
 * �����ж��壬��ͬ�Ĵ��룬���޸�
 */
#define __APP_BIN_VERSION		"1.03.002"


// ����212����Base64β�ַ�3D ��Ϊ - 2D
#define BASE64_CHAR_END  0x2D


void F_LoadAppSetting(void);


//void F_AppSettingSaveBegin(void);
//int F_AppSettingStringSave(int nOffset, char* sSetting, int nLength);
//void F_AppSettingSaveEnd(int nLength);

int F_AppSettingString(char* sOut, int nLength);

EOTKeyValue* F_SettingAdd(char* sName, char* sValue);
void F_SettingRemove(char* sName);
char* F_SettingGetString(char* sName, ...);
int F_SettingGetInt32(char* sName, ...);
double F_SettingGetDouble(char* sName, ...);

void F_AppSettingInit(void);

#endif /* CONFIG_H_ */
