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

// ���4������
#define MAX_TCP_CONNECT		4

// δ����
#define STATUS_NONE 		0
// �Ѿ�����
#define STATUS_OPENED 		1
// ���ڹر�
#define STATUS_CLOSE 		2

typedef struct _stEOTConnect EOTConnect;
typedef void (*EOFuncNetOpen)(EOTConnect *connect, int code);
typedef int (*EOFuncNetRecv)(EOTConnect *connect, unsigned char* pData, int nLength);
typedef void (*EOFuncNetClose)(EOTConnect *connect, int code);


#define HOST_LENGTH 	32

// ���2K����
#define MAX_TCP_RECV 	2048

//
// ���Ӷ���
//
typedef struct _stEOTConnect
{
	// ͨ��
	uint8_t channel;
	// ״̬
	uint8_t status;
	// �˿�
	uint16_t port;
	// ��������ַ
	char host[HOST_LENGTH];

	// ��ʱ��
	uint32_t last_tick;

	// �������
	uint32_t tag;

	// ���ջ��棬������ʽЭ��
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
