/*
 * CProtocol.h
 *
 *  Created on: Jan 20, 2024
 *      Author: hjx
 *
 * Ϊ�˼򻯣�����Э��ͷ�ļ��ϲ�
 */

#ifndef CPROTOCOL_H_
#define CPROTOCOL_H_

#include <stdint.h>

#include "eob_date.h"

// ���֧��ͨ����
#define MAX_NET_CHANNEL		4


#define NET_TYPE_NONE				0
#define NET_TCP						1
#define NET_HTTP					2
#define NET_MQTT					3
#define NET_UDP						4

#define TICK_UPDATE_MAX 			4

// ���ͻ���
#define SEND_MAX 					2048

typedef struct _stNetChannel TNetChannel;

// �ص�����
typedef void (*FuncNetStart)(TNetChannel* pNetChannel);
typedef void (*FuncNetUpdate)(TNetChannel* pNetChannel, uint64_t tick, EOTDate* pDate);
typedef void (*FuncNetStop)(TNetChannel* pNetChannel);

typedef enum _emProtocolType
{
	ProtocolTypeNone = 0,
	TcpJson,
	TcpHJ212,
}
MProtocolType;

typedef struct _stNetChannel
{
	// ����
	uint8_t id;

	// �������� 1 NET_TCP 2 NET_HTTP 3 NET_MQTT
	uint8_t type;

	// ��Ӧ�Ĳ�����ţ�server��
	uint8_t cfg_id;

	// ״̬
	uint8_t status;

	// GPRS��Ӧ��ͨ��
	uint8_t gprs_id;

	// Э������
	uint8_t protocol;

	uint8_t r1;
	uint8_t r2;

	int tick_delay[TICK_UPDATE_MAX];
	int tick_last[TICK_UPDATE_MAX];

	FuncNetStart start_cb;
	FuncNetStop stop_cb;
	FuncNetUpdate update_cb;
}
TNetChannel;

void F_Protocol_Init(void);
char* F_Protocol_SendBuffer(void);

void F_Protocol_Type(TNetChannel* pNetChannel, char* sType);

uint16_t F_Protocol_HJ212CRC16(uint8_t* sData, int nLen);

void F_Protocol_TcpHJ212_Init(TNetChannel* pNetChannel, char* sType);
void F_Protocol_TcpJson_Init(TNetChannel* pNetChannel, char* sType);

#endif /* CPROTOCOL_H_ */
