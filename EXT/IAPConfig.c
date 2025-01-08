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

// ȫ��������Ϣ
TIAPConfig s_IAPConfig = { };

TIAPConfig* F_IAPConfig_Get(void)
{
	return &s_IAPConfig;
}

// ת������Ϊ��ʾ�ַ���
const char* F_ConfigFlagString(uint32_t nFlag)
{
	switch (nFlag)
	{
	case CONFIG_FLAG_NORMAL:
		return "����ģʽ";
	case CONFIG_FLAG_START:
		return "����ģʽ";
	case CONFIG_FLAG_FLASH:
		return "��¼ģʽ";
	case CONFIG_FLAG_INPUT:
		return "������ģʽ";
	}

	return "δ֪�����ʶ";
}

// ��ȡ�豸Key��32λ�ַ���
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

	_T("�豸��ʶ: %s", s_IAPConfig.device_key);
}

// ����IAP������Ϣ���ڲ�Flash
void F_IAPConfig_Save(uint32_t nFlag)
{
	if (CONFIG_FLAG_NONE != nFlag) s_IAPConfig.flag = nFlag;

	_T("���� Flash Em ����[%08X] : %s, ��ǰ�汾%d",
			s_IAPConfig.id, F_ConfigFlagString(s_IAPConfig.flag),
			s_IAPConfig.region);

	EOB_Flash_Erase(SECTOR_CFG);
	EOB_Flash_Write(ADDRESS_BASE_CFG, &s_IAPConfig, sizeof(TIAPConfig));
}
// ���ڲ�Flash��ȡIAP������Ϣ
int F_IAPConfig_Load(void)
{
	memset(&s_IAPConfig, 0, sizeof(TIAPConfig));

	EOB_Flash_Read(ADDRESS_BASE_CFG, &s_IAPConfig, sizeof(TIAPConfig));
	s_IAPConfig.device_key[DEVICE_KEY_LENGTH - 1] = '\0';

	_T("���� Flash Em ����[%08X] : %s, ��ǰ�汾%d",
			s_IAPConfig.id, F_ConfigFlagString(s_IAPConfig.flag),
			s_IAPConfig.region);

	// ����İ汾
	if (s_IAPConfig.id != IAP_CONFIG_ID)
	{
		memset(&s_IAPConfig, 0, sizeof(TIAPConfig));
		s_IAPConfig.id = IAP_CONFIG_ID;
		s_IAPConfig.flag = CONFIG_FLAG_INPUT;

		F_DeviceKey();

		F_IAPConfig_Save(CONFIG_FLAG_INPUT);

		_T("���� Flash Em ����[%08X] : %s, ��ǰ�汾%d",
				s_IAPConfig.id, F_ConfigFlagString(s_IAPConfig.flag),
				s_IAPConfig.region);

		return -1;
	}

	_T("�豸��ʶ: %s", s_IAPConfig.device_key);

	return 0;
}

