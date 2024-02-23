/*
 * IAPCmd.h
 *
 *  Created on: Feb 6, 2024
 *      Author: hjx
 */

#ifndef IAPCMD_H_
#define IAPCMD_H_

#include <stdint.h>

#include "eos_buffer.h"

// 命令头
#define CMD_FLAG_BEGIN		0x23232323 // ####
#define CMD_FLAG_END		0x0A0D4040 // @@\r\n

#define CMD_DELAY			500 // 延时
#define CMD_LENGTH			12
#define CMD_NONE			0x00000000 //


// 跳转到APP
#define CMD_JUMP_APP		0x7070616A // japp
// 开始写入
#define CMD_FLASH_BEGIN		0x31616C66 // fla1
// 结束写入
#define CMD_FLASH_END		0x32616C66 // fla2
// 文件开始
#define CMD_BIN_BEGIN		0x316E6962 // bin1
// 文件结束
#define CMD_BIN_END			0x326E6962 // bin2
// 配置开始
#define CMD_DAT_BEGIN		0x31746164 // dat1
// 配置结束
#define CMD_DAT_END			0x32746164 // dat2
// 重置配置
#define CMD_CONFIG			0x666E6F63 // conf
// 设置Gate Api地址
#define CMD_GATE_API		0x65746167 // gate

// 软重启
#define CMD_RESET			0x74736572 // rest
// 取消命令
#define CMD_CANCEL			0x636E6163 // canc


void F_Cmd_Input(EOTBuffer* tBuffer, uint32_t nCheckFlag);

#endif /* IAPCMD_H_ */
