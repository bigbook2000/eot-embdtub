/*
 * eon_net.h
 *
 *  Created on: Jun 26, 2023
 *      Author: hjx
 *
 *  ����ͨ��ģ��
 */

#ifndef EON_NET_H_
#define EON_NET_H_

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
	uint64_t last_tick;

	// �������
	uint32_t tag;

	// ���ջ��棬������ʽЭ��
	EOTBuffer recv_buffer;

	EOFuncNetOpen open_cb;
	EOFuncNetRecv recv_cb;
	EOFuncNetClose close_cb;
}
EOTConnect;

#endif /* EON_NET_H_ */
