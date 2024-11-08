/*
 * eob_util.c
 *
 *  Created on: Jul 15, 2023
 *      Author: hjx
 */

#include "eob_util.h"

//#include "core_cm4.h"
#include "stm32f4xx_ll_utils.h"
#include "stm32f4xx_ll_usart.h"
#include "stm32f4xx_ll_dma.h"

#include "eob_debug.h"

// 96位，12个字节
void EOB_STM32_Uid(unsigned char* pByte12)
{
	pByte12[0x0] = *((__IO uint32_t*)(ADDRESS_UID_STM32F4 + 0x0));
	pByte12[0x1] = *((__IO uint32_t*)(ADDRESS_UID_STM32F4 + 0x1));
	pByte12[0x2] = *((__IO uint32_t*)(ADDRESS_UID_STM32F4 + 0x2));
	pByte12[0x3] = *((__IO uint32_t*)(ADDRESS_UID_STM32F4 + 0x3));

	pByte12[0x4] = *((__IO uint32_t*)(ADDRESS_UID_STM32F4 + 0x4));
	pByte12[0x5] = *((__IO uint32_t*)(ADDRESS_UID_STM32F4 + 0x5));
	pByte12[0x6] = *((__IO uint32_t*)(ADDRESS_UID_STM32F4 + 0x6));
	pByte12[0x7] = *((__IO uint32_t*)(ADDRESS_UID_STM32F4 + 0x7));

	pByte12[0x8] = *((__IO uint32_t*)(ADDRESS_UID_STM32F4 + 0x8));
	pByte12[0x9] = *((__IO uint32_t*)(ADDRESS_UID_STM32F4 + 0x9));
	pByte12[0xA] = *((__IO uint32_t*)(ADDRESS_UID_STM32F4 + 0xA));
	pByte12[0xB] = *((__IO uint32_t*)(ADDRESS_UID_STM32F4 + 0xB));
}

typedef void (*APP_MAIN)(void);
void EOB_JumpApp(unsigned int nAddressApp)
{
	uint32_t uCode = *((__IO uint32_t*)nAddressApp);
	_T("跳转到APP区 [%08X] ... %08lX", nAddressApp, uCode);
	LL_mDelay(1000);

	// RAM 0x20020000
	if ((uCode & 0x2FFC0000) == 0x20000000)
	{
		// 必须调用关闭串口，跳转后复用
		EOB_Debug_DeInit();

		//__set_FAULTMASK(1);
		__disable_irq();
		APP_MAIN fMain = (APP_MAIN)(*((__IO uint32_t*)(nAddressApp + sizeof(uint32_t))));

		__set_MSP((*((__IO uint32_t*)nAddressApp)));

		fMain();

		while (1) {};
	}
	else
	{
		_T("无效的APP区");
	}
}
