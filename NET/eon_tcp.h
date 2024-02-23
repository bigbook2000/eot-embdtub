/*
 * eon_tcp.h
 *
 *  Created on: Jun 26, 2023
 *      Author: hjx
 */

#ifndef EON_TCP_H_
#define EON_TCP_H_

#include <stdint.h>

#include "eos_buffer.h"

// 最大4个连接
#define MAX_TCP_CONNECT		4

// 未设置
#define STATUS_NONE 		0
// 已经连接
#define STATUS_OPENED 		1
// 正在关闭
#define STATUS_CLOSE 		2

typedef struct _stEOTConnect EOTConnect;
typedef void (*EOFuncNetOpen)(EOTConnect *connect, int code);
typedef int (*EOFuncNetRecv)(EOTConnect *connect, unsigned char* pData, int nLength);
typedef void (*EOFuncNetClose)(EOTConnect *connect, int code);


#define HOST_LENGTH 	32

// 最大2K缓存
#define MAX_TCP_RECV 	2048

//
// 连接对象
//
typedef struct _stEOTConnect
{
	// 通道
	uint8_t channel;
	// 状态
	uint8_t status;
	// 端口
	uint16_t port;
	// 服务器地址
	char host[HOST_LENGTH];

	// 计时器
	uint32_t last_tick;

	// 外接数据
	uint32_t tag;

	// 接收缓存，用于流式协议
	EOTBuffer recv_buffer;

	EOFuncNetOpen open_cb;
	EOFuncNetRecv recv_cb;
	EOFuncNetClose close_cb;
}
EOTConnect;

int EON_Tcp_Init(void);
void EON_Tcp_Update(uint64_t tick);

void EON_Tcp_Open(uint8_t nChannel);
void EON_Tcp_SendData(uint8_t nChannel, uint8_t* pData, int nLength);
void EON_Tcp_Close(uint8_t nChannel);

uint8_t EON_Tcp_IsConnect(uint8_t nChannel);
EOTConnect* EON_Tcp_GetConnect(uint8_t nChannel);
EOTConnect* EON_Tcp_SetConnect(uint8_t nChannel,
		char* sHost, uint16_t nPort,
		EOFuncNetOpen tCallbackOpen,
		EOFuncNetRecv tCallbackRecv,
		EOFuncNetClose tCallbackClose);

#endif /* EON_TCP_H_ */
