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
// 通用模块
//

// 简化任务优先级，只分为3级，避免资源抢占
#define TASK_PRIORITY_LOW			1
#define TASK_PRIORITY_NORMAL		3
#define TASK_PRIORITY_HIGH			5

#define EO_TRUE 					0x1
#define EO_FALSE 					0x0

// 用于对齐，避免随意长度
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

// 将单个16进制字符转成16进制数字，原地转换
// 0x41(65) = A
// 0x61(96) = a
// 0x30(48) = 0
#define CHAR_HEX(d) 				if(d>=0x41&&d<=0x46){d-=0x41-10;}else if (d>=0x61&&d<=0x66){d-=0x61-10;}else if(d>=0x30&&d<0x39){d-=0x30;}else{d=0xFF;}

// 检测一个时刻
uint8_t EOG_Check_Delay(uint64_t* last, uint64_t tick, uint64_t delay);

// 判断是否是数字
uint8_t EOG_CheckNumber(char* sData, int nLength);
// 判断是否是IP
uint8_t EOG_CheckIP(char* sData, int nLength);
// 去除两端空白
char* EOG_Trim(char* sStr, int nLength);
// 拆分IP地址和端口 xxx.xxxxx.xxx:0000
int EOG_SplitIP(char* sData, int nLength, char* sHost, uint16_t* nPort);
// Strin转换HEX  'FFFFFFFF'->0xFFFFFFFF
uint32_t EOG_String2Hex(char* sStr, int nLength);

// 查找一行\r\n
char* EOG_GetLine(char* sStr, char* sOut);
// 查找指定的两个标识之间的字符串
char* EOG_FindString(char* sStr, int nLen, char* sOut, char* sStart, char* sEnd);
int EOG_SplitString(char* sData, int nLength, char cSplit, char** ppArray, uint8_t *nCount);

// 去掉特殊字符
int EOG_TrimChar(char* sData, int nLength, char cSplit);

// 一分为二，注意长度
void EOG_KeyValue(char* sStr, int nLenStr,
		char cSplit, char cEnd, char* sKey, int nLenKey, char* sVal, int nLenVal);
void EOG_KeyValueChar(char* sStr, char cSplit, char** ppKey, char** ppVal);

// 字节流转换为base64
int EOG_Base64Encode(uint8_t* pBuffer, int nLength, char* pBase64, char cFlag);
// base64转换为字节流
int EOG_Base64Decode(char* pBase64, int nLength, uint8_t* pBuffer, char cFlag);

//
// 调试输出
// 需要在串口输出中重定义
//



void EOG_PrintFormat(char* sInfo, ...);
void EOG_PrintTime(void);
void EOG_PrintLine(void);
void EOG_PrintBin(void* pData, int nLength);
void EOG_PrintSend(void);

// 输出不带任何标识
#define _P(...) EOG_PrintFormat(__VA_ARGS__);EOG_PrintSend()
// 格式化输出，带换行
#define _T(...) EOG_PrintTime();EOG_PrintFormat(__VA_ARGS__);EOG_PrintLine();EOG_PrintSend()
// 调试
#define _D(...) EOG_PrintTime();EOG_PrintFormat("[%s:%d]", __FILE__, __LINE__);EOG_PrintFormat(__VA_ARGS__);EOG_PrintLine();EOG_PrintSend()

// 输出二进制
#define _Tmb(p, n) EOG_PrintBin(p, n);EOG_PrintSend()


#endif /* EOS_INC_H_ */
