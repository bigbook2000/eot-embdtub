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
 * 使用4个字母来表示类型
 * 代码中定义，不同的代码，请修改
 */
#define __APP_BIN_TYPE 			"EOTB"
/**
 * 使用3个数字来表示
 * 代码中定义，不同的代码，请修改
 */
#define __APP_BIN_VERSION		"1.03.002"


// 兼容212，将Base64尾字符3D 改为 - 2D
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
