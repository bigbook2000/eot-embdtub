/*
 * eon_tcp.h
 *
 *  Created on: Jun 26, 2023
 *      Author: hjx
 */

#ifndef EON_TCP_H_
#define EON_TCP_H_

#include <stdint.h>

#include "eon_net.h"

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
