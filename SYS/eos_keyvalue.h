/*
 * eos_keyvalue.h
 *
 *  Created on: Jan 31, 2024
 *      Author: hjx
 *
 * �����ַ�����ֵ��
 * ��Ϊ�̶����ȣ�ֵ��̬����
 *
 * �����������ڳ־û����ݲ�֧���ͷţ������ʹ��
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
	// �ַ�������
	uint8_t key_length;
	// �����С
	uint8_t value_size;

	// ���뱣��
	uint16_t r0;

	// ���ò�������
	char key[KEY_LENGTH];
	// �豸Key
	char* value;
}
EOTKeyValue;

EOTKeyValue* EOS_KeyValue_Find(EOTList* tpList, int nHashCode, char* sKey, int nLen);
EOTKeyValue* EOS_KeyValue_Set(EOTList* tpList, char* sKey, char* sValue);

char* EOS_KeyValue_Get(EOTList* tpList, char* sKey);

#endif /* EOS_KEYVALUE_H_ */
