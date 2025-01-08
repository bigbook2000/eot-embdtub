/*
 * IAPConfig.c
 *
 *  Created on: Jul 15, 2023
 *      Author: hjx
 */

#include "IAPConfig.h"

#include <string.h>
#include <stdio.h>


#include "eob_debug.h"
#include "eob_util.h"

// 全局配置信息
TIAPConfig s_IAPConfig = { };

TIAPConfig* F_IAPConfig_Get(void)
{
	return &s_IAPConfig;
}

// 转换命令为显示字符串
const char* F_ConfigFlagString(uint32_t nFlag)
{
	switch (nFlag)
	{
	case CONFIG_FLAG_NORMAL:
		return "正常模式";
	case CONFIG_FLAG_START:
		return "运行模式";
	case CONFIG_FLAG_FLASH:
		return "烧录模式";
	case CONFIG_FLAG_INPUT:
		return "命令行模式";
	}

	return "未知命令标识";
}

// 获取设备Key，32位字符串
void F_DeviceKey(void)
{
	uint8_t nUids[STM32_UID_LENGTH];
	EOB_STM32_Uid(nUids);

	memset(s_IAPConfig.device_key, 0, DEVICE_KEY_LENGTH);

	sprintf(s_IAPConfig.device_key,
			"EOIOTA01%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
			nUids[0], nUids[1], nUids[2], nUids[3],
			nUids[4], nUids[5], nUids[6], nUids[7],
			nUids[8], nUids[9], nUids[10], nUids[11]);

	_T("设备标识: %s", s_IAPConfig.device_key);
}

// 保存IAP配置信息到内部Flash
void F_IAPConfig_Save(uint32_t nFlag)
{
	if (CONFIG_FLAG_NONE != nFlag) s_IAPConfig.flag = nFlag;

	_T("保存 Flash Em 配置[%08X] : %s, 当前版本%d",
			s_IAPConfig.id, F_ConfigFlagString(s_IAPConfig.flag),
			s_IAPConfig.region);

	EOB_Flash_Erase(SECTOR_CFG);
	EOB_Flash_Write(ADDRESS_BASE_CFG, &s_IAPConfig, sizeof(TIAPConfig));
}
// 从内部Flash读取IAP配置信息
int F_IAPConfig_Load(void)
{
	memset(&s_IAPConfig, 0, sizeof(TIAPConfig));

	EOB_Flash_Read(ADDRESS_BASE_CFG, &s_IAPConfig, sizeof(TIAPConfig));
	s_IAPConfig.device_key[DEVICE_KEY_LENGTH - 1] = '\0';

	_T("加载 Flash Em 配置[%08X] : %s, 当前版本%d",
			s_IAPConfig.id, F_ConfigFlagString(s_IAPConfig.flag),
			s_IAPConfig.region);

	// 错误的版本
	if (s_IAPConfig.id != IAP_CONFIG_ID)
	{
		memset(&s_IAPConfig, 0, sizeof(TIAPConfig));
		s_IAPConfig.id = IAP_CONFIG_ID;
		s_IAPConfig.flag = CONFIG_FLAG_INPUT;

		F_DeviceKey();

		F_IAPConfig_Save(CONFIG_FLAG_INPUT);

		_T("创建 Flash Em 配置[%08X] : %s, 当前版本%d",
				s_IAPConfig.id, F_ConfigFlagString(s_IAPConfig.flag),
				s_IAPConfig.region);

		return -1;
	}

	_T("设备标识: %s", s_IAPConfig.device_key);

	return 0;
}

