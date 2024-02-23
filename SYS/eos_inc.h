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

#define SIZE_8						8
#define SIZE_32						32
#define SIZE_128					128
#define SIZE_256					256
#define SIZE_1K						1024
#define SIZE_4K						4096
#define SIZE_8K						8192


#define CHAR_BLANK(c)				((c)==' '||(c)=='\t'||(c)=='\v'||(c)=='\f'||(c)=='\r'||(c)=='\n')

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
// Strin转换HEX
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

#endif /* EOS_INC_H_ */
