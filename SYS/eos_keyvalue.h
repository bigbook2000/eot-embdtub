/*
 * eos_keyvalue.h
 *
 *  Created on: Jan 31, 2024
 *      Author: hjx
 *
 * 处理字符串键值对
 * 键为固定长度，值动态分配
 *
 * 配置作用在于持久化，暂不支持释放，请谨慎使用
 *
 */

#ifndef EOS_KEYVALUE_H_
#define EOS_KEYVALUE_H_

#include <stdint.h>

#include "eos_list.h"

#define KEY_LENGTH 		32
#define VALUE_SIZE		256

typedef struct _stEOTKeyValue
{
	// 字符串长度
	uint8_t key_length;
	// 缓存大小
	uint8_t value_size;

	// 对齐保留
	uint16_t r0;

	// 配置参数名称
	char key[KEY_LENGTH];
	// 设备Key
	char* value;
}
EOTKeyValue;

EOTKeyValue* EOS_KeyValue_Find(EOTList* tpList, int nHashCode, char* sKey, int nLen);
EOTKeyValue* EOS_KeyValue_Set(EOTList* tpList, char* sKey, char* sValue);

char* EOS_KeyValue_Get(EOTList* tpList, char* sKey);

#endif /* EOS_KEYVALUE_H_ */
