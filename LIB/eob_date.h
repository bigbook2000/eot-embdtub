/*
 * eob_date.h
 *
 *  Created on: Jun 30, 2023
 *      Author: hjx
 */

#ifndef EOB_DATE_H_
#define EOB_DATE_H_


#include <stdint.h>
#include <stdio.h>

// ��ݼ�ȥ
#define YEAR_ZERO 	2000

//
// RTC ����ʱ��

typedef struct _stEOTDate
{
	/**
	 * ���0-99
	 */
	uint8_t year;
	/**
	 * 1-12 // ��1��ʼ
	 */
	uint8_t month;
	/*
	 * 1-31
	 */
	uint8_t date;

	uint8_t hour;
	uint8_t minute;
	uint8_t second;
}
EOTDate;

uint8_t EOB_Date_GetWeek(int nYear, int nMonth, int nDay);
uint8_t EOB_Date_GetMonthDateCount(int nYear, int nMonth);

void EOB_Date_Get(EOTDate* tpDate);
void EOB_Date_Set(EOTDate* tpDate);

/**
 * ��������
 */
void EOB_Date_AddDate(EOTDate* tpDate, int8_t nYear, int8_t nMonth, int8_t nDay);
/**
 * ʱ������
 */
void EOB_Date_AddTime(EOTDate* tpDate, int8_t nHour, int8_t nMinute, int8_t nSecond);

#endif /* EOB_DATE_H_ */
