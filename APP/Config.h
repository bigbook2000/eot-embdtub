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
#define __APP_BIN_VERSION		"1.0.1"


#define APP_CONFIG_VER 			2

void F_LoadAppConfig(void);
void F_SaveAppConfig(void);

void F_SaveAppConfigString(char* sConfig, int nLength);
int F_AppConfigString(char* sOut, int nLength);

EOTKeyValue* F_ConfigAdd(char* sName, char* sValue);
void F_ConfigRemove(char* sName);
char* F_ConfigGetString(char* sName, ...);
int F_ConfigGetInt32(char* sName, ...);
double F_ConfigGetDouble(char* sName, ...);

void F_AppConfigInit(void);

#endif /* CONFIG_H_ */
