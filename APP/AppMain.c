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

// ���߳�
osThreadId taskMainHandle;

// �����߳�
osThreadId taskDebugHandle;
// �����߳�
osThreadId taskDevHandle;
// �����߳�
osThreadId taskNetHandle;

#define FLAG_DEBUG_NONE 		0
#define FLAG_DEBUG_READY 		1
#define FLAG_DEBUG_OPENED 		2




static uint64_t s_TickDebugInput = 0L;
/**
 * ������������
 * ���п��Ʋ�������IAP�н���
 * ���APP����ʱ��ֻ�ܽ��յ�����ת����
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
		// ��ʱ
		_T("���볬ʱ");
		EOS_Buffer_Clear(tBuffer);
	}

	//_T("�������� = %d", tBuffer->length);

	F_Cmd_Input(tBuffer);
}

/**
 * ��ӡ����
 */
static void OnCmd_DatShow(uint32_t nCmdId, EOTBuffer* tBuffer, int nPos)
{
	_T("ִ������: ��ӡ����");

	EOS_Buffer_Pop(tBuffer, NULL, nPos);

	// ��ԭ��ʶ
	F_Cmd_SetFlag(CMD_NONE);

	// ��ӡ����
	F_LoadAppSetting();
}

// ������������
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

// ������������
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

		// 24λ
		//tick = osKernelSysTick() * 1000L / osKernelSysTickFrequency;

		F_NetManager_Update(tick);

		osDelay(10);
	}
}

// ���߳�
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

	// ������ʼ��
	F_Cmd_ProcessInit();
	F_Cmd_ProcessSet(CMD_RESET);
	F_Cmd_ProcessExt(CMD_DAT_SHOW, (EOFuncCmdProcess)OnCmd_DatShow);

	while (1)
	{
		EOB_Debug_Update();

		// ������������
		OnApp_Input();

		osDelay(1);
	}
}

void AppMain(void)
{
	//
	// printf���������Ի��з�����\n
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
