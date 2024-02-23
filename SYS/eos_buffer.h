/*
 * eos_buffer.h
 *
 *  Created on: Jun 30, 2023
 *      Author: hjx
 */

#ifndef EOS_BUFFER_H_
#define EOS_BUFFER_H_

#include <stdint.h>
#include <stdio.h>

//
// 动态内存缓冲
//
typedef struct _stEOTBuffer
{
	uint8_t* buffer;
	// 数据长度
	int length;
	// 缓冲区大小
	int size;
	// 标记位置
	int pos;
	// 标记数据
	int tag;
}
EOTBuffer;


//
// 静态内存缓冲
//
#define D_STATIC_BUFFER_DECLARE(vname, vsize) static uint8_t s_Buffer_##vname[vsize];static EOTBuffer vname = {0};
#define D_STATIC_BUFFER_INIT(vname, vsize) vname.buffer=s_Buffer_##vname;vname.size=vsize;vname.length=0;


EOTBuffer* EOS_Buffer_Create(EOTBuffer* tpBuffer, int nSize);
EOTBuffer* EOS_Buffer_Destory(EOTBuffer* tpBuffer);
// 清空
void EOS_Buffer_Clear(EOTBuffer* tpBuffer);

void EOS_Buffer_Reset(EOTBuffer* tpBuffer);
int EOS_Buffer_Set(EOTBuffer* tpBuffer, int nPos, void* pData, int nLength);
int EOS_Buffer_Get(EOTBuffer* tpBuffer, int nPos, void* pData, int nLength);

int EOS_Buffer_CopyBuffer(EOTBuffer* tpBufferDst, EOTBuffer* tpBufferSrc);
int EOS_Buffer_Copy(EOTBuffer* tpBuffer, void* pData, int nLength);

int EOS_Buffer_PushOne(EOTBuffer* tpBuffer, uint8_t bByte);
int EOS_Buffer_Push(EOTBuffer* tpBuffer, void* pData, int nLength);
int EOS_Buffer_PushBuffer(EOTBuffer* tpBufferDst, EOTBuffer* tpBufferSrc);

int EOS_Buffer_Pop(EOTBuffer* tpBuffer, char* pOut, int nCount);
// 拷贝一行到制定的缓存，并移除
int EOS_Buffer_PopReturn(EOTBuffer* tpBuffer, char* pOut, int nCount);
// 不拷贝，直接返回一行（节省内存）
int EOS_Buffer_GetReturn(EOTBuffer* tpBuffer);
// 拷贝指定的标识之前的数据（包含标识）
int EOS_Buffer_PopFlag(EOTBuffer* tpBuffer, char* pOut, char* sFlag);
int EOS_Buffer_PopBuffer(EOTBuffer* tpBuffer, EOTBuffer* tpBufferOut);

#endif /* EOS_BUFFER_H_ */
