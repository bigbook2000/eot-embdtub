/*
 * Global.h
 *
 *  Created on: Jun 27, 2023
 *      Author: hjx
 */

#ifndef GLOBAL_H_
#define GLOBAL_H_

// 全局定义

// 不要太过于精确，防止浮点精度不够
#define VALUE_MIN			-1e18
#define VALUE_MAX			1e18
#define INT_MAX				0x7FFFFFFF

// 数据类型，低4位表示长度
// Little Big

#define DATA_TYPE_LITTLE	0x0
#define DATA_TYPE_BIG		0x1

#define DATA_TYPE_SIGNED	0x1
#define DATA_TYPE_UNSIGNED	0x2
#define DATA_TYPE_POINT		0x3

#define DATA_TYPE_N1		0x1
#define DATA_TYPE_N2		0x2
#define DATA_TYPE_N4		0x4
#define DATA_TYPE_N8		0x8

#define DATA_TYPE_NONE		0
#define DATA_TYPE_INT8		(DATA_TYPE_SIGNED << 4 		| DATA_TYPE_N1)
#define DATA_TYPE_UINT8		(DATA_TYPE_UNSIGNED << 4 	|DATA_TYPE_N1)
#define DATA_TYPE_INT16		(DATA_TYPE_SIGNED << 4 		|DATA_TYPE_N2)
#define DATA_TYPE_UINT16	(DATA_TYPE_UNSIGNED << 4 	|DATA_TYPE_N2)
#define DATA_TYPE_INT32		(DATA_TYPE_SIGNED << 4 		|DATA_TYPE_N4)
#define DATA_TYPE_UINT32	(DATA_TYPE_UNSIGNED << 4 	|DATA_TYPE_N4)
#define DATA_TYPE_FLOAT		(DATA_TYPE_POINT << 4 		|DATA_TYPE_N4)
#define DATA_TYPE_DOUBLE	(DATA_TYPE_POINT << 4 		|DATA_TYPE_N8)

#define DATA_TYPE_LENGTH(t) 	((t)&0xF)

#define ENDIAN_CHANGE_32(d) 	(((d)&0xFF000000)>>24)|(((d)&0x00FF0000)>>8)|(((d)&0x0000FF00)<<8)|(((d)&0x000000FF)<<24)
#define ENDIAN_CHANGE_16(d) 	(((d)&0xFF00)>>8)|(((d)&0x00FF)<<8)

#define DATA_TYPE_SET(v,t,d) 	switch ((t)) {\
								case DATA_TYPE_INT8:*(int8_t*)(v)=*(int8_t*)(d);break;\
								case DATA_TYPE_UINT8:*(uint8_t*)(v)=*(uint8_t*)(d);break;\
								case DATA_TYPE_INT16:*(int16_t*)(v)=*(int16_t*)(d);break;\
								case DATA_TYPE_UINT16:*(uint16_t*)(v)=*(uint16_t*)(d);break;\
								case DATA_TYPE_INT32:*(int32_t*)(v)=*(int32_t*)(d);break;\
								case DATA_TYPE_UINT32:*(uint32_t*)(v)=*(uint32_t*)(d);break;\
								case DATA_TYPE_FLOAT:*(float*)(v)=*(float*)(d);break;\
								case DATA_TYPE_DOUBLE:*(double*)(v)=*(double*)(d);break;\
								}

uint8_t F_DataType(char* sType);

#endif /* GLOBAL_H_ */
