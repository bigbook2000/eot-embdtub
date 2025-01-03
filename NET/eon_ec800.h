/*
 * eon_ec800.h
 *
 *  Created on: Jan 18, 2024
 *      Author: hjx
 */

#ifndef EON_EC800_H_
#define EON_EC800_H_


#include "stdint.h"
#include "eos_buffer.h"

// 数据返回命令模式，命令模式或透传模式
#define GPRS_MODE_AT 		0
#define GPRS_MODE_DATA 		1

// 命令执行失败是否重启
#define GPRS_CMD_NORMAL 	0
#define GPRS_CMD_RESET 		1
#define GPRS_CMD_POWER 		2

// 命令超时
#define GPRS_TIME_SHORT 	3000
#define GPRS_TIME_NORMAL 	8000
#define GPRS_TIME_LONG 		15000

#define GPRS_TRY_ONCE 		1
#define GPRS_TRY_NORMAL 	3
#define GPRS_TRY_MORE 		5

// GPRS命令基本都很小
#define GPRS_CMD_SIZE 256

// 判断返回tBuffer字符串是否存在
#define CHECK_STR(s) 				strnstr((char*)tBuffer->buffer, s, tBuffer->length) != NULL
// 查找返回tBuffer指定的字符串
#define NCHECK_STR(r, s1, s2) 		EOG_FindString((char*)tBuffer->buffer, tBuffer->length, r, s1, s2) == NULL

typedef enum _emEOEGprsCmd
{
	EOE_GprsCmd_AT = 1,

	// 关闭回显
	EOE_GprsCmd_ATE0,

	// SIM卡状态
	EOE_GprsCmd_CPIN,
	// SIM卡IMEI
	EOE_GprsCmd_CGSN,
	// SIM卡ICCID
	EOE_GprsCmd_CCID,

	// 功能模式0
	EOE_GprsCmd_CFUN_0,
	// 功能模式1
	EOE_GprsCmd_CFUN_1,

	// 运营商
	EOE_GprsCmd_COPS,

	// GPRS注册状态
	EOE_GprsCmd_CGREG,
	// GPRS注册
	EOE_GprsCmd_CGATT,

	// 接入点
	EOE_GprsCmd_QICSGP,
	// 移动APN接入点
	EOE_GprsCmd_QICSGP_CM,
	// 联通APN接入点
	EOE_GprsCmd_QICSGP_UN,

	// 激活GPRS
	EOE_GprsCmd_QIACT,
	// 取消激活GPRS
	EOE_GprsCmd_QIDEACT,

	// 网络链接状态
	EOE_GprsCmd_CIPSTATUS,
	// IP地址
	EOE_GprsCmd_CGPADDR,
	// DNS
	EOE_GprsCmd_CDNSGIP,


	// 时钟
	EOE_GprsCmd_QLTS,
	// 信号
	EOE_GprsCmd_CSQ,


	// TCP 协议

	// TCP连接
	EOE_GprsCmd_QIOPEN,
	// TCP断开
	EOE_GprsCmd_QICLOSE,

	// TCP发送命令
	EOE_GprsCmd_QISEND,
	// TCP发送数据
	EOE_GprsCmd_QISEND_D,
	// TCP接收数据
	EOE_GprsCmd_QIRD,

	// HTTP 协议

	// 设置 HTTP PDP
	EOE_HttpCmd_QHTTPCFG_PDP,
	// 设置 HTTP 输出响应头
	EOE_HttpCmd_QHTTPCFG_RESP,
	// 设置 HTTP SSL ID
	EOE_HttpCmd_QHTTPCFG_SSL_ID,
	// 设置 SSL 版本
	EOE_HttpCmd_QSSLCFG_VER,
	// 设置 SSL 加密套件
	EOE_HttpCmd_QSSLCFG_SUITE,
	// 设置 SSL 验证级别
	EOE_HttpCmd_QSSLCFG_LEVEL,
	// 设置 URL
	EOE_HttpCmd_QHTTPURL,
	// 输入 URL
	EOE_HttpCmd_QHTTPURL_D,

	// GET
	EOE_HttpCmd_QHTTPGET,
	// POST
	EOE_HttpCmd_QHTTPPOST,

	// 读取响应
	EOE_HttpCmd_QHTTPREAD,


	EOE_GprsCmd_Test,
	EOE_GprsCmd_Count,

	// 用于动态标记命令，主要是需要二次处理的数据
	EOE_GprsCmd_Tag = 0xFFFF,

	EOE_GprsCmd_None = 0xFFFFFFFF,
}
EOEGprsCmd;

// 命令参数
typedef struct _stEOTGprsCmd
{
	// 命令标识
	EOEGprsCmd id;

	// 外挂标识
	uint32_t tag;

	// 通道标识
	uint8_t channel;
	// 对齐保留
	uint8_t r1;
	// 对齐保留
	uint16_t r2;

	int8_t send_mode;
	int8_t recv_mode;

	// 命令执行失败是否重启
	int8_t reset;
	// 最大次数
	int8_t try_max;

	// 超时时长
	int16_t delay_max;
	// 命令长度
	int16_t cmd_len;

	// 命令
	uint8_t* cmd_data;
}
EOTGprsCmd;

typedef void (*EOFuncGprsReady)(void);
typedef void (*EOFuncGprsError)(EOTGprsCmd* pCmd, int nError);
typedef void (*EOFuncGprsReset)(EOTGprsCmd* pCmd);
typedef int (*EOFuncGprsCommandBefore)(EOTGprsCmd* pCmd);
typedef int (*EOFuncGprsCommand)(EOTGprsCmd* pCmd, EOTBuffer* tBuffer);
typedef int (*EOFuncGprsData)(uint8_t nChannel, EOTBuffer* tBuffer);
typedef int (*EOFuncGprsEvent)(EOTBuffer* tBuffer);

void EON_Gprs_Init(void);
void EON_Gprs_Update(uint64_t tick);

void EON_Gprs_Power(void);
void EON_Gprs_Reset(void);

void EON_Gprs_Callback(
		EOFuncGprsReady tCallbackReady,
		EOFuncGprsReset tCallbackReset,
		EOFuncGprsError tCallbackError,
		EOFuncGprsCommandBefore tCallbackCommandBefore,
		EOFuncGprsCommand tCallbackCommand,
		EOFuncGprsData tCallbackData,
		EOFuncGprsEvent tCallbackEvent);

// 是否已经可以通讯
uint8_t EON_Gprs_IsReady(void);

EOTGprsCmd* EON_Gprs_LastCmd(void);
EOTGprsCmd* EON_Gprs_SendCmdPut(
		EOEGprsCmd nCmdId,
		uint8_t* pCmdData, int16_t nCmdLen,
		int8_t nSendMode, int8_t nRecvMode,
		int16_t nTimeout, int8_t nTryCount, int8_t nReset);
void EON_Gprs_SendCmdPop(void);

// 直接向串口写数据
int EON_Gprs_DataPut(uint8_t* pData, int nLength);
// 获取指定长度的数据
void EON_Gprs_DataGet(int nChannel, int nCount);
// 用来处理特殊断行
void EON_Gprs_CheckCmdFlag(char* sFlag, int nLength);

// 获取信号
void EON_Gprs_Cmd_CSQ(void);
// 获取时钟
void EON_Gprs_Cmd_QLTS(void);


uint8_t EON_Gprs_CSQ(void);

#endif /* EON_EC800_H_ */
