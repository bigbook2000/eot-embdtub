/*
 * eos_inc.h
 *
 *  Created on: May 9, 2023
 *      Author: hjx
 */

#ifndef EOS_INC_H_
#define EOS_INC_H_

#include <stdint.h>

//
// ͨ��ģ��
//

// ���������ȼ���ֻ��Ϊ3����������Դ��ռ
#define TASK_PRIORITY_LOW			1
#define TASK_PRIORITY_NORMAL		3
#define TASK_PRIORITY_HIGH			5

#define EO_TRUE 					0x1
#define EO_FALSE 					0x0

// ���ڶ��룬�������ⳤ��
#define SIZE_8						8
#define SIZE_16						16
#define SIZE_24						24
#define SIZE_32						32
#define SIZE_40						40
#define SIZE_64						64
#define SIZE_80						80
#define SIZE_100					100
#define SIZE_160					160
#define SIZE_128					128
#define SIZE_256					256
#define SIZE_512					512
#define SIZE_1000					1000
#define SIZE_1K						1024
#define SIZE_4K						4096
#define SIZE_8K						8192


#define CHAR_BLANK(c)				((c)==' '||(c)=='\t'||(c)=='\v'||(c)=='\f'||(c)=='\r'||(c)=='\n')

// ������16�����ַ�ת��16�������֣�ԭ��ת��
// 0x41(65) = A
// 0x61(96) = a
// 0x30(48) = 0
#define CHAR_HEX(d) 				if(d>=0x41&&d<=0x46){d-=0x41-10;}else if (d>=0x61&&d<=0x66){d-=0x61-10;}else if(d>=0x30&&d<0x39){d-=0x30;}else{d=0xFF;}

// ���һ��ʱ��
uint8_t EOG_Check_Delay(uint64_t* last, uint64_t tick, uint64_t delay);

// �ж��Ƿ�������
uint8_t EOG_CheckNumber(char* sData, int nLength);
// �ж��Ƿ���IP
uint8_t EOG_CheckIP(char* sData, int nLength);
// ȥ�����˿հ�
char* EOG_Trim(char* sStr, int nLength);
// ���IP��ַ�Ͷ˿� xxx.xxxxx.xxx:0000
int EOG_SplitIP(char* sData, int nLength, char* sHost, uint16_t* nPort);
// Strinת��HEX  'FFFFFFFF'->0xFFFFFFFF
uint32_t EOG_String2Hex(char* sStr, int nLength);

// ����һ��\r\n
char* EOG_GetLine(char* sStr, char* sOut);
// ����ָ����������ʶ֮����ַ���
char* EOG_FindString(char* sStr, int nLen, char* sOut, char* sStart, char* sEnd);
int EOG_SplitString(char* sData, int nLength, char cSplit, char** ppArray, uint8_t *nCount);

// ȥ�������ַ�
int EOG_TrimChar(char* sData, int nLength, char cSplit);

// һ��Ϊ����ע�ⳤ��
void EOG_KeyValue(char* sStr, int nLenStr,
		char cSplit, char cEnd, char* sKey, int nLenKey, char* sVal, int nLenVal);
void EOG_KeyValueChar(char* sStr, char cSplit, char** ppKey, char** ppVal);

// �ֽ���ת��Ϊbase64
int EOG_Base64Encode(uint8_t* pBuffer, int nLength, char* pBase64, char cFlag);
// base64ת��Ϊ�ֽ���
int EOG_Base64Decode(char* pBase64, int nLength, uint8_t* pBuffer, char cFlag);

//
// �������
// ��Ҫ�ڴ���������ض���
//



void EOG_PrintFormat(char* sInfo, ...);
void EOG_PrintTime(void);
void EOG_PrintLine(void);
void EOG_PrintBin(void* pData, int nLength);
void EOG_PrintSend(void);

// ��������κα�ʶ
#define _P(...) EOG_PrintFormat(__VA_ARGS__);EOG_PrintSend()
// ��ʽ�������������
#define _T(...) EOG_PrintTime();EOG_PrintFormat(__VA_ARGS__);EOG_PrintLine();EOG_PrintSend()
// ����
#define _D(...) EOG_PrintTime();EOG_PrintFormat("[%s:%d]", __FILE__, __LINE__);EOG_PrintFormat(__VA_ARGS__);EOG_PrintLine();EOG_PrintSend()

// ���������
#define _Tmb(p, n) EOG_PrintBin(p, n);EOG_PrintSend()


#endif /* EOS_INC_H_ */
