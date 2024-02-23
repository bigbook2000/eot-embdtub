/*
 * Global.c
 *
 *  Created on: Jan 30, 2024
 *      Author: hjx
 */

#include <string.h>
#include <stdlib.h>

#include "cmsis_os.h"

#include "eos_inc.h"
#include "eob_debug.h"

#include "Global.h"

uint8_t F_DataType(char* sType)
{
	if (strcmp(sType, "int8") == 0)
	{
		return DATA_TYPE_INT8;
	}
	else if (strcmp(sType, "uint8") == 0)
	{
		return DATA_TYPE_UINT8;
	}
	else if (strcmp(sType, "int16") == 0)
	{
		return DATA_TYPE_INT16;
	}
	else if (strcmp(sType, "uint16") == 0)
	{
		return DATA_TYPE_UINT16;
	}
	else if (strcmp(sType, "int32") == 0)
	{
		return DATA_TYPE_INT32;
	}
	else if (strcmp(sType, "uint32") == 0)
	{
		return DATA_TYPE_UINT32;
	}
	else if (strcmp(sType, "float") == 0)
	{
		return DATA_TYPE_FLOAT;
	}
	else if (strcmp(sType, "double") == 0)
	{
		return DATA_TYPE_DOUBLE;
	}
	else
	{
		return DATA_TYPE_NONE;
	}
}
