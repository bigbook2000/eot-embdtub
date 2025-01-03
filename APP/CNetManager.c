/*
 * CNetManager.c
 *
 *  Created on: Dec 29, 2023
 *      Author: hjx
 *
 * 网络通讯管理
 * 同时可支持4路网络连接，包括TCP/HTTP/MQTT三种协议
 */

#include "CNetManager.h"

#include "stm32f4xx_ll_utils.h"
#include "stm32f4xx_ll_gpio.h"
#include "stm32f4xx_ll_usart.h"

#include "cmsis_os.h"

#include <string.h>
#include <stdio.h>

#include "eos_inc.h"
#include "eos_timer.h"
#include "eob_debug.h"
#include "eob_date.h"
#include "eob_tick.h"
#include "eob_w25q.h"

#include "eon_ec800.h"
#include "eon_tcp.h"
#include "eon_http.h"
#include "eon_mqtt.h"

#include "Global.h"
#include "AppSetting.h"
#include "CProtocol.h"

// 网络是否准备好
static uint8_t s_NetStatusReady = EO_FALSE;

static TNetChannel s_NetChannels[MAX_NET_CHANNEL];

static void OnGprsReady(void)
{
	int i;
	for (i=0; i<MAX_NET_CHANNEL; i++)
	{
		if (s_NetChannels[i].start_cb != NULL)
		{
			s_NetChannels[i].start_cb(&s_NetChannels[i]);
		}
	}

	s_NetStatusReady = EO_TRUE;
}
static void OnGprsReset(int nCmdId)
{
	s_NetStatusReady = EO_FALSE;

	int i;
	for (i=0; i<MAX_NET_CHANNEL; i++)
	{
		if (s_NetChannels[i].stop_cb != NULL)
		{
			s_NetChannels[i].stop_cb(&s_NetChannels[i]);
		}
	}
}

static void NetChannelInit(TNetChannel* pNetChannel, uint8_t nCfgId)
{
	// 暂时对应
	pNetChannel->gprs_id = pNetChannel->id;

	char* sType = F_SettingGetString("svr%d.type", nCfgId);
	if (strnstr(sType, "tcp", KEY_LENGTH) != NULL)
	{
		pNetChannel->type = NET_TCP;
	}
	else if (strnstr(sType, "http", KEY_LENGTH) != NULL)
	{
		pNetChannel->type = NET_HTTP;
	}
	else if (strnstr(sType, "mqtt", KEY_LENGTH) != NULL)
	{
		pNetChannel->type = NET_MQTT;
	}
	else
	{
		_T("未配置网络类型: %d", nCfgId);
		return;
	}

	pNetChannel->cfg_id = nCfgId;
	pNetChannel->status = 0;

	_T("初始化网络服务[%d]: %d, %d, %d",
			pNetChannel->id, pNetChannel->type, pNetChannel->cfg_id, pNetChannel->gprs_id);

	char* sProtocol = F_SettingGetString("svr%d.protocol", nCfgId);
	F_Protocol_Type(pNetChannel, sProtocol);

	int i;
	char* ss[8];
	char s[VALUE_SIZE];

	uint8_t cnt;

	char* sTick = F_SettingGetString("svr%d.tick", nCfgId);
	cnt = 8;
	strcpy(s, sTick); // 避免破坏性分割
	if (EOG_SplitString(s, -1, ',', (char**)ss, &cnt) != TICK_UPDATE_MAX)
	{
		_T("*** 网络参数设置错误 %d", cnt);
		return;
	}
	for (i=0; i<TICK_UPDATE_MAX; i++)
	{
		pNetChannel->tick_delay[i] = atoi(ss[i]);
		pNetChannel->tick_last[i] = INT_MAX; // 用于标记特殊点
	}
}

void F_NetManager_Init(void)
{
	EON_Gprs_Init();
	// 注入回调
	EON_Gprs_Callback(
			(EOFuncGprsReady)OnGprsReady,
			(EOFuncGprsReset)OnGprsReset,
			NULL, NULL, NULL, NULL, NULL);

	// 网络协议
	EON_Tcp_Init();
	EON_Http_Init();

	// 应用协议
	F_Protocol_Init();

	// 从配置信息初始化通道
	int i, cnt;
	TNetChannel* pNetChannel;
	for (i=0; i<MAX_NET_CHANNEL; i++)
	{
		pNetChannel = &s_NetChannels[i];

		memset(pNetChannel, 0, sizeof(TNetChannel));
		pNetChannel->id = i;
	}
	cnt = F_SettingGetInt32("svr.count");
	for (i=0; i<cnt; i++)
	{
		NetChannelInit(&s_NetChannels[i], i + 1);
	}
}

EOTConnect s_ConnectHttp = {};

static uint64_t s_TickCSQ = 0L;
static uint8_t s_NetChannelIndex = 0;
void F_NetManager_Update(uint64_t tick)
{
	EOTDate tDate = {0};
	EOB_Date_Get(&tDate);

	// 检查信号（不用太频繁）
	if (s_NetStatusReady == EO_TRUE && EOB_Tick_Check(&s_TickCSQ, 200000))
	{
		EON_Gprs_Cmd_CSQ();

		//EON_Http_Open(&s_ConnectHttp);
	}

	EON_Gprs_Update(tick);

	EON_Tcp_Update(tick);
	EON_Http_Update(tick);

	// 错开分时更新
	TNetChannel* pNetChannel = &s_NetChannels[s_NetChannelIndex];
	if (pNetChannel->update_cb != NULL)
	{
		pNetChannel->update_cb(pNetChannel, tick, &tDate);
	}
	++s_NetChannelIndex;
	if (s_NetChannelIndex >= MAX_NET_CHANNEL) s_NetChannelIndex = 0;
}
