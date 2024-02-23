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
// ��̬�ڴ滺��
//
typedef struct _stEOTBuffer
{
	uint8_t* buffer;
	// ���ݳ���
	int length;
	// ��������С
	int size;
	// ���λ��
	int pos;
	// �������
	int tag;
}
EOTBuffer;


//
// ��̬�ڴ滺��
//
#define D_STATIC_BUFFER_DECLARE(vname, vsize) static uint8_t s_Buffer_##vname[vsize];static EOTBuffer vname = {0};
#define D_STATIC_BUFFER_INIT(vname, vsize) vname.buffer=s_Buffer_##vname;vname.size=vsize;vname.length=0;


EOTBuffer* EOS_Buffer_Create(EOTBuffer* tpBuffer, int nSize);
EOTBuffer* EOS_Buffer_Destory(EOTBuffer* tpBuffer);
// ���
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
// ����һ�е��ƶ��Ļ��棬���Ƴ�
int EOS_Buffer_PopReturn(EOTBuffer* tpBuffer, char* pOut, int nCount);
// ��������ֱ�ӷ���һ�У���ʡ�ڴ棩
int EOS_Buffer_GetReturn(EOTBuffer* tpBuffer);
// ����ָ���ı�ʶ֮ǰ�����ݣ�������ʶ��
int EOS_Buffer_PopFlag(EOTBuffer* tpBuffer, char* pOut, char* sFlag);
int EOS_Buffer_PopBuffer(EOTBuffer* tpBuffer, EOTBuffer* tpBufferOut);

#endif /* EOS_BUFFER_H_ */
