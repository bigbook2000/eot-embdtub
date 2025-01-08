/*
 * AppMain.c
 *
 *  Created on: May 8, 2023
 *      Author: hjx
 */

#include "AppMain.h"

#include "stm32f4xx_ll_utils.h"
#include "stm32f4xx_ll_gpio.h"

#include "cmsis_os.h"
#include "crc.h"

#include <string.h>
#include <stdio.h>

#include "eos_inc.h"
#include "eos_timer.h"
#include "eob_debug.h"
#include "eob_date.h"
#include "eob_tick.h"
#include "eob_w25q.h"

#include "eon_ec800.h"
#include "eon_http.h"
#include "eon_tcp.h"

#include "AppSetting.h"
#include "IAPCmd.h"
#include "CDevManager.h"
#include "CNetManager.h"

#define CFG_BUFFER_SIZE			0x2000

//#define CMD_DELAY 				1000

#define GPIO_SYS 				GPIOE
#define PIN_SYS 				LL_GPIO_PIN_14
#define GPIO_4G 				GPIOE
#define PIN_4G 					LL_GPIO_PIN_15

#define GPIO_BT_RST 			GPIOC
#define PIN_BT_RST 				LL_GPIO_PIN_9

#define GPIO_BT_PWR 			GPIOA
#define PIN_BT_PWR 				LL_GPIO_PIN_8

// 主线程
osThreadId taskMainHandle;

// 控制线程
osThreadId taskDebugHandle;
// 外设线程
osThreadId taskDevHandle;
// 网络线程
osThreadId taskNetHandle;

#define FLAG_DEBUG_NONE 		0
#define FLAG_DEBUG_READY 		1
#define FLAG_DEBUG_OPENED 		2




static uint64_t s_TickDebugInput = 0L;
/**
 * 处理命令输入
 * 所有控制操作都在IAP中进行
 * 因此APP运行时，只能接收调试跳转命令
 * ####debg@\\r\\n
 */
void OnApp_Input(void)
{
	uint64_t tick = EOB_Tick_Get();

	EOTBuffer* tBuffer = EOB_Debug_InputData();
	if (tBuffer->length <= 0)
	{
		s_TickDebugInput = 0L;
		return;
	}

	if (s_TickDebugInput == 0L)
	{
		s_TickDebugInput = tick;
	}
	else if ((tick - s_TickDebugInput) > CMD_DELAY)
	{
		// 超时
		_T("输入超时");
		EOS_Buffer_Clear(tBuffer);
	}

	//_T("接收输入 = %d", tBuffer->length);

	F_Cmd_Input(tBuffer);
}

/**
 * 打印配置
 */
static void OnCmd_DatShow(uint32_t nCmdId, EOTBuffer* tBuffer, int nPos)
{
	_T("执行命令: 打印配置");

	EOS_Buffer_Pop(tBuffer, NULL, nPos);

	// 还原标识
	F_Cmd_SetFlag(CMD_NONE);

	// 打印配置
	F_LoadAppSetting();
}

// 处理外设任务
void f_Task_Dev(void const * arg)
{
	uint64_t i = 0;
	uint64_t tick;

	F_DevManager_Init();
	while (1)
	{
		++i;

		if ((i % 400) == 0) LL_GPIO_SetOutputPin(GPIO_SYS, PIN_SYS);
		if ((i % 400) == 200) LL_GPIO_ResetOutputPin(GPIO_SYS, PIN_SYS);

		tick = EOB_Tick_Get();
		F_DevManager_Update(tick);

		osDelay(10);
	}
}

// 处理网络任务
void f_Task_Net(void const * arg)
{
	uint64_t i = 0;
	uint64_t tick;

	F_NetManager_Init();
	while (1)
	{
		++i;

		if ((i % 200) == 0) LL_GPIO_SetOutputPin(GPIO_4G, PIN_4G);
		if ((i % 200) == 100) LL_GPIO_ResetOutputPin(GPIO_4G, PIN_4G);

		tick = EOB_Tick_Get();

		// 24位
		//tick = osKernelSysTick() * 1000L / osKernelSysTickFrequency;

		F_NetManager_Update(tick);

		osDelay(10);
	}
}

// 主线程
void f_Task_Main(void const * arg)
{
	F_AppSettingInit();

	osDelay(100);

	osThreadDef(taskDev, f_Task_Dev, osPriorityNormal, 0, 1024);
	taskDevHandle = osThreadCreate(osThread(taskDev), NULL);
	if (taskDevHandle == NULL)
	{
		_P("osThreadCreate Fail : Dev\r\n");
	}

	osThreadDef(taskNet, f_Task_Net, osPriorityNormal, 0, 1024);
	taskNetHandle = osThreadCreate(osThread(taskNet), NULL);
	if (taskNetHandle == NULL)
	{
		_P("osThreadCreate Fail : Net\r\n");
	}

	// 勿忘初始化
	F_Cmd_ProcessInit();
	F_Cmd_ProcessSet(CMD_RESET);
	F_Cmd_ProcessExt(CMD_DAT_SHOW, (EOFuncCmdProcess)OnCmd_DatShow);

	while (1)
	{
		EOB_Debug_Update();

		// 处理输入命令
		OnApp_Input();

		osDelay(1);
	}
}

void AppMain(void)
{
	//
	// printf函数必须以换行符结束\n
	//
	EOB_Debug_Init();

	_P("\r\n");
	_P("********************************\r\n");
	_P("**   EOIOTAPP\r\n");
	_P("**   Type = %s\r\n", __APP_BIN_TYPE);
	_P("**   Version = %s\r\n", __APP_BIN_VERSION);
	_P("**   Copyright@eobject 2020\r\n");
	_P("********************************\r\n\r\n");

	osThreadDef(taskMain, f_Task_Main, osPriorityHigh, 0, 512);
	taskMainHandle = osThreadCreate(osThread(taskMain), NULL);

//	char* s = pvPortMalloc(CFG_BUFFER_SIZE);
//	vPortFree(s);

	//LL_GPIO_SetOutputPin(GPIOD, LL_GPIO_PIN_5);
	//LL_GPIO_ResetOutputPin(GPIOD, LL_GPIO_PIN_5);

	//F_SysSwitch_Output_Set(3, 0);
	//F_SysSwitch_Output_Set(4, 1);

	while (1)
	{
		osDelay(100);
	}
}
