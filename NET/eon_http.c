/*
 * eon_http.c
 *
 *  Created on: Jul 10, 2023
 *      Author: hjx
 */

#include "eon_http.h"

#include "string.h"

#include "eos_inc.h"
#include "eos_list.h"
#include "eob_debug.h"
#include "eob_tick.h"

#include "eon_ec800.h"

#define HTTP_CONNECT_NONE 	0
#define HTTP_CONNECT_SET 	1
#define HTTP_CONNECT_DONE 	9

#define HTTP_TIMEOUT		200000

static EOTList s_ListHttpConnect;

static int s_ConnectID = 0;

static void HttpClose(void)
{
	EOTListItem* itemConnect = EOS_List_Top(&s_ListHttpConnect);
	if (itemConnect == NULL) return;

	if (itemConnect->flag == HTTP_CONNECT_SET)
	{
		_T("关闭HTTP: %d", itemConnect->id);
		itemConnect->flag = HTTP_CONNECT_DONE;
	}
}

// 设置 HTTP PDP
static void OnHttpCmd_QHTTPCFG_PDP(EOTBuffer* tBuffer)
{
	EON_Gprs_SendCmdPut(
			EOE_HttpCmd_QHTTPCFG_RESP,
			(uint8_t*)"AT+QHTTPCFG=\"responseheader\",1\r\n", -1,
			GPRS_MODE_AT, GPRS_MODE_AT,
			GPRS_TIME_LONG, GPRS_TRY_NORMAL, GPRS_CMD_NORMAL);
}
// 设置 HTTP 输出响应头
static void OnHttpCmd_QHTTPCFG_RESP(EOTBuffer* tBuffer)
{
	EON_Gprs_SendCmdPut(
			EOE_HttpCmd_QHTTPCFG_SSL_ID,
			(uint8_t*)"AT+QHTTPCFG=\"sslctxid\",1\r\n", -1,
			GPRS_MODE_AT, GPRS_MODE_AT,
			GPRS_TIME_LONG, GPRS_TRY_NORMAL, GPRS_CMD_NORMAL);
}
// 设置 HTTP SSL ID
static void OnHttpCmd_QHTTPCFG_SSL_ID(EOTBuffer* tBuffer)
{
	// 设置 SSL 版本
	//0 SSL 3.0
	//1 TLS 1.0
	//2 TLS 1.1
	//3 TLS 1.2
	//4 全部
	EON_Gprs_SendCmdPut(
			EOE_HttpCmd_QSSLCFG_VER,
			(uint8_t*)"AT+QSSLCFG=\"sslversion\",1,4\r\n", -1,
			GPRS_MODE_AT, GPRS_MODE_AT,
			GPRS_TIME_LONG, GPRS_TRY_NORMAL, GPRS_CMD_NORMAL);
}
// 设置 SSL 版本
static void OnHttpCmd_QSSLCFG_VER(EOTBuffer* tBuffer)
{
	// 设置套件
	//0X0035 TLS_RSA_WITH_AES_256_CBC_SHA
	//0X002F TLS_RSA_WITH_AES_128_CBC_SHA
	//0X0005 TLS_RSA_WITH_RC4_128_SHA
	//0X0004 TLS_RSA_WITH_RC4_128_MD5
	//0X000A TLS_RSA_WITH_3DES_EDE_CBC_SHA
	//0X003D TLS_RSA_WITH_AES_256_CBC_SHA256
	//0XC002 TLS_ECDH_ECDSA_WITH_RC4_128_SHA
	//0XC003 TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA
	//0XC004 TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA
	//0XC005 TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA
	//0XC007 TLS_ECDHE_ECDSA_WITH_RC4_128_SHA
	//0XC008 TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA
	//0XC009 TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA
	//0XC00A TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA
	//0XC011 TLS_ECDHE_RSA_WITH_RC4_128_SHA
	//0XC012 TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA
	//0XC013 TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA
	//0XC014 TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA
	//0xC00C TLS_ECDH_RSA_WITH_RC4_128_SHA
	//0XC00D TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA
	//0XC00E TLS_ECDH_RSA_WITH_AES_128_CBC_SHA
	//0XC00F TLS_ECDH_RSA_WITH_AES_256_CBC_SHA
	//0XC023 TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256
	//0xC024 TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384
	//0xC025 TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA256
	//0xC026 TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA384
	//0XC027 TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256
	//0XC028 TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384
	//0xC029 TLS_ECDH_RSA_WITH_AES_128_CBC_SHA256
	//0XC02A TLS_ECDH_RSA_WITH_AES_256_CBC_SHA384
	//0XC02F TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256
	//0XC030 MBEDTLS_TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384
	//0XFFFF 支持所有加密套件

	EON_Gprs_SendCmdPut(
			EOE_HttpCmd_QSSLCFG_SUITE,
			(uint8_t*)"AT+QSSLCFG=\"ciphersuite\",1,0xFFFF\r\n", -1,
			GPRS_MODE_AT, GPRS_MODE_AT,
			GPRS_TIME_LONG, GPRS_TRY_NORMAL, GPRS_CMD_NORMAL);
}
// 设置 SSL 加密套件
static void OnHttpCmd_QSSLCFG_SUITE(EOTBuffer* tBuffer)
{
	//0 无身份验证模式
	//1 进行服务器身份验证
	//2 如果远程服务器要求，则进行服务器和客户端身份验证
	EON_Gprs_SendCmdPut(
			EOE_HttpCmd_QSSLCFG_LEVEL,
			(uint8_t*)"AT+QSSLCFG=\"seclevel\",1,0\r\n", -1,
			GPRS_MODE_AT, GPRS_MODE_AT,
			GPRS_TIME_LONG, GPRS_TRY_NORMAL, GPRS_CMD_NORMAL);
}
// 设置 SSL 验证级别
static void OnHttpCmd_QSSLCFG_LEVEL(EOTBuffer* tBuffer)
{
	EON_Gprs_SendCmdPut(
			EOE_HttpCmd_QHTTPURL,
			(uint8_t*)"AT+QHTTPURL=21,80\r\n", -1,
			GPRS_MODE_AT, GPRS_MODE_DATA,
			GPRS_TIME_LONG, GPRS_TRY_NORMAL, GPRS_CMD_NORMAL);
}


// 设置 URL
static void OnHttpCmd_QHTTPURL(EOTBuffer* tBuffer)
{
	if (strstr((const char*)tBuffer->buffer, "CONNECT\r\n") == NULL) return;
	tBuffer->buffer[tBuffer->length] = '\0';

	_T("%s", (const char*)tBuffer->buffer);

	// 清除数据
	EOS_Buffer_Clear(tBuffer);

	// 透传模式
	EON_Gprs_SendCmdPut(
			EOE_HttpCmd_QHTTPURL_D,
			//"https://www.baidu.com",
			(uint8_t*)"https://mail.sina.com", -1,
			GPRS_MODE_AT, GPRS_MODE_AT,
			GPRS_TIME_LONG, GPRS_TRY_ONCE, GPRS_CMD_NORMAL);
}
// 输入 URL
static void OnHttpCmd_QHTTPURL_D(EOTBuffer* tBuffer)
{
	// 获取数据长度
	EON_Gprs_SendCmdPut(
			EOE_HttpCmd_QHTTPGET,
			(uint8_t*)"AT+QHTTPGET=80\r\n", -1,
			GPRS_MODE_AT, GPRS_MODE_DATA,
			GPRS_TIME_LONG, GPRS_TRY_NORMAL, GPRS_CMD_NORMAL);
}

// GET
static void OnHttpCmd_QHTTPGET(EOTBuffer* tBuffer)
{
	// +QHTTPGET: 0,200,601710
	char sCmdText[SIZE_32];
	if (EOG_FindString((char*)tBuffer->buffer, tBuffer->length, (char*)sCmdText, "+QHTTPGET: ", "\r\n") == NULL) return;

	tBuffer->buffer[tBuffer->length] = '\0';
	_T("%s", tBuffer->buffer);

	char** ppArray = NULL;
	uint8_t nCount = 0;
	EOG_SplitString((char*)sCmdText, -1, ',', ppArray, &nCount);

	// 清除数据
	EOS_Buffer_Clear(tBuffer);

	if (nCount < 2)
	{
		return;
	}

	// 读取响应数据
	EON_Gprs_SendCmdPut(
			EOE_HttpCmd_QHTTPREAD,
			(uint8_t*)"AT+QHTTPREAD=80\r\n", -1,
			GPRS_MODE_AT, GPRS_MODE_DATA,
			GPRS_TIME_LONG, GPRS_TRY_ONCE, GPRS_CMD_NORMAL);
}
// POST
static void OnHttpCmd_QHTTPPOST(EOTBuffer* tBuffer)
{
	// 命令清除
	//EOB_Gprs_CmdPop();
}
// 读取响应
static void OnHttpCmd_QHTTPREAD(EOTBuffer* tBuffer)
{
	_Tmb(tBuffer->buffer, tBuffer->length);
	_T("%s", tBuffer->buffer);

	char* p = strstr((const char*)tBuffer->buffer, "+QHTTPREAD: 0\r\n");

	// 清除数据
	EOS_Buffer_Clear(tBuffer);

	if (p == NULL) return;

	HttpClose();
}

static int OnHttpCommand(EOTGprsCmd* pCmd, EOTBuffer* tBuffer)
{
	if (pCmd == NULL) return -1;
	int nCmdId = pCmd->id;

	switch (nCmdId)
	{
	// 设置 HTTP PDP
	case EOE_HttpCmd_QHTTPCFG_PDP:
		OnHttpCmd_QHTTPCFG_PDP(tBuffer);
		break;
	// 设置 HTTP 输出响应头
	case EOE_HttpCmd_QHTTPCFG_RESP:
		OnHttpCmd_QHTTPCFG_RESP(tBuffer);
		break;
	// 设置 HTTP SSL ID
	case EOE_HttpCmd_QHTTPCFG_SSL_ID:
		OnHttpCmd_QHTTPCFG_SSL_ID(tBuffer);
		break;
	// 设置 SSL 版本
	case EOE_HttpCmd_QSSLCFG_VER:
		OnHttpCmd_QSSLCFG_VER(tBuffer);
		break;
	// 设置 SSL 加密套件
	case EOE_HttpCmd_QSSLCFG_SUITE:
		OnHttpCmd_QSSLCFG_SUITE(tBuffer);
		break;
	// 设置 SSL 验证级别
	case EOE_HttpCmd_QSSLCFG_LEVEL:
		OnHttpCmd_QSSLCFG_LEVEL(tBuffer);
		break;

	// 设置 URL
	case EOE_HttpCmd_QHTTPURL:
		OnHttpCmd_QHTTPURL(tBuffer);
		break;
	// 输入 URL
	case EOE_HttpCmd_QHTTPURL_D:
		OnHttpCmd_QHTTPURL_D(tBuffer);
		break;

	// GET
	case EOE_HttpCmd_QHTTPGET:
		OnHttpCmd_QHTTPGET(tBuffer);
		break;
	// POST
	case EOE_HttpCmd_QHTTPPOST:
		OnHttpCmd_QHTTPPOST(tBuffer);
		break;

	// 读取响应
	case EOE_HttpCmd_QHTTPREAD:
		OnHttpCmd_QHTTPREAD(tBuffer);
		break;

	default:
		return -1;
	}

	return 0;
}
static int OnHttpData(uint8_t nChannel, EOTBuffer* tBuffer)
{
	return 0;
}
static void OnHttpError(EOTGprsCmd* pCmd, int nError)
{
	// 如果出现错误，则完成
	HttpClose();
}

static int OnHttpEvent(EOTBuffer* tBuffer)
{
	return 0;
}

int EON_Http_Init(void)
{
	EOS_List_Create(&s_ListHttpConnect);

	// 注入回调
	EON_Gprs_Callback(
			NULL,
			NULL,
			(EOFuncGprsError)OnHttpError,
			NULL,
			(EOFuncGprsCommand)OnHttpCommand,
			(EOFuncGprsData)OnHttpData,
			(EOFuncGprsEvent)OnHttpEvent);

	return 0;
}

int EON_Http_Open(EOTConnect* hConnect)
{
	s_ConnectID++;
	hConnect->channel = s_ConnectID;
	hConnect->last_tick = EOB_Tick_Get();

	EOS_List_Push(&s_ListHttpConnect, hConnect->channel, hConnect, sizeof(EOTConnect));

	return 0;
}

void EON_Http_Update(uint64_t tick)
{
	EOTListItem* itemConnect = EOS_List_Top(&s_ListHttpConnect);
	if (itemConnect == NULL) return;

	EOTConnect* tConnect = (EOTConnect*)(itemConnect->data);

	// 还未处理
	if (itemConnect->flag == HTTP_CONNECT_NONE)
	{
		itemConnect->flag = HTTP_CONNECT_SET;

		_T("连接HTTP");
//		EON_Gprs_SendCmdPut(EOE_GprsCmd_QIDEACT, (uint8_t*)"AT+QIDEACT=1\r\n", -1,
//				GPRS_MODE_AT, GPRS_MODE_AT, GPRS_TIME_LONG, GPRS_TRY_NORMAL, GPRS_CMD_RESET);
//		EON_Gprs_SendCmdPut(EOE_GprsCmd_QIDEACT, (uint8_t*)"AT+QIACT?\r\n", -1,
//				GPRS_MODE_AT, GPRS_MODE_AT, GPRS_TIME_LONG, GPRS_TRY_NORMAL, GPRS_CMD_RESET);
		EON_Gprs_SendCmdPut(
				EOE_HttpCmd_QHTTPCFG_PDP,
				(uint8_t*)"AT+QHTTPCFG=\"contextid\",1\r\n", -1,
				GPRS_MODE_AT, GPRS_MODE_AT,
				GPRS_TIME_LONG, GPRS_TRY_NORMAL, GPRS_CMD_NORMAL);
	}
	else if (itemConnect->flag == HTTP_CONNECT_DONE)
	{
		// 已经完成，移除
		EOS_List_Pop(&s_ListHttpConnect);
	}
	else
	{
		// 超时
		if (EOB_Tick_Check(&(tConnect->last_tick), HTTP_TIMEOUT))
		{
			itemConnect->flag = HTTP_CONNECT_DONE;
		}
	}
}
