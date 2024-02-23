/*
 * eob_date.c
 *
 *  Created on: May 9, 2023
 *      Author: hjx
 */

#include "eob_date.h"

#include "stm32f4xx_ll_rtc.h"

#include <time.h>

// 月修正数据表
const uint8_t TABLE_WEEK[12] = {0,3,3,6,1,4,6,2,5,0,3,5};
// 计算星期
unsigned char EOB_Date_GetWeek(int nYear, int nMonth, int nDay)
{
  int dateCount = 0;

  int yearH = nYear / 100;
	int yearL = nYear % 100;

  if (yearH > 19) yearL += 100;

  // 闰年数只算1900年之后
  dateCount = yearL + yearL / 4;
  dateCount = dateCount % 7;
  dateCount = dateCount + nDay + TABLE_WEEK[nMonth - 1];

  if ((yearL % 4) == 0 && nMonth < 3) dateCount--;

  return (dateCount % 7);
}

unsigned char EOB_Date_GetMonthDateCount(int nYear, int nMonth)
{
	if (nMonth == 2)
	{
		if (((nYear % 4) == 0 && (nYear % 100) != 0) || ((nYear % 400) == 0))
		{
			return 29;
		}
		else
		{
			return 28;
		}
	}
	else if (nMonth == 4 || nMonth == 6 || nMonth == 9 || nMonth == 11)
	{
		return 30;
	}

	return 31;
}

void EOB_Date_Get(EOTDate* tpDate)
{
	// 2000+
  tpDate->year = __LL_RTC_CONVERT_BCD2BIN(LL_RTC_DATE_GetYear(RTC));
	// 1-12
  tpDate->month = __LL_RTC_CONVERT_BCD2BIN(LL_RTC_DATE_GetMonth(RTC));
  tpDate->date = __LL_RTC_CONVERT_BCD2BIN(LL_RTC_DATE_GetDay(RTC));
  tpDate->hour = __LL_RTC_CONVERT_BCD2BIN(LL_RTC_TIME_GetHour(RTC));
  tpDate->minute = __LL_RTC_CONVERT_BCD2BIN(LL_RTC_TIME_GetMinute(RTC));
  tpDate->second = __LL_RTC_CONVERT_BCD2BIN(LL_RTC_TIME_GetSecond(RTC));
}

void EOB_Date_Set(EOTDate* tpDate)
{
//	LL_RTC_DATE_Config(RTC,
//		0, tpDate->date, tpDate->month, tpDate->year);
//
//	LL_RTC_TIME_Config(RTC,
//		LL_RTC_HOURFORMAT_24HOUR,
//		tpDate->hour, tpDate->minute, tpDate->second);

	LL_RTC_DateTypeDef rtcDate;
	LL_RTC_TimeTypeDef rtcTime;

	rtcDate.WeekDay = 0;
	rtcDate.Day = tpDate->date;
	rtcDate.Month = tpDate->month;
	rtcDate.Year = tpDate->year;

	rtcTime.TimeFormat = LL_RTC_HOURFORMAT_24HOUR;
	rtcTime.Seconds = tpDate->second;
	rtcTime.Minutes = tpDate->minute;
	rtcTime.Hours = tpDate->hour;

	LL_RTC_DATE_Init(RTC, LL_RTC_FORMAT_BIN, &rtcDate);
	LL_RTC_TIME_Init(RTC, LL_RTC_FORMAT_BIN, &rtcTime);

  //LL_RTC_BAK_SetRegister(RTC, LL_RTC_BKP_DR0, RTC_BKP_DATE_TIME_UPDTATED);
	printf("设置RTC: %02d-%02d-%02d %02d:%02d:%02d\r\n",
			tpDate->year, tpDate->month, tpDate->date,
			tpDate->hour, tpDate->minute, tpDate->second);
}

/**
 * 日期增减
 */
void EOB_Date_AddDate(EOTDate* tpDate, int8_t nYear, int8_t nMonth, int8_t nDay)
{
//	struct tm st = {0};
//
//	st.tm_year = YEAR_ZERO + tpDate->year;
//	st.tm_mon = tpDate->month;
//	st.tm_mday = tpDate->date;
//	st.tm_hour = tpDate->hour;
//	st.tm_min = tpDate->minute;
//	st.tm_sec = tpDate->second;
//
//	time_t tt = mktime(&st);
}

/**
 * 时间增减
 */
void EOB_Date_AddTime(EOTDate* tpDate, int8_t nHour, int8_t nMinute, int8_t nSecond)
{
	struct tm st = {0};

	st.tm_year = YEAR_ZERO + tpDate->year;
	st.tm_mon = tpDate->month - 1; // 取值0-11
	st.tm_mday = tpDate->date; // 1-31
	st.tm_hour = tpDate->hour;
	st.tm_min = tpDate->minute;
	st.tm_sec = tpDate->second;

//	printf("%d-%d-%d %d:%d:%d\r\n",
//			st.tm_year, st.tm_mon, st.tm_mday,
//			st.tm_hour, st.tm_min, st.tm_sec);

	time_t tt = mktime(&st);

	tt += nHour * 3600 + nMinute * 60 + nSecond;

	struct tm* tp = gmtime(&tt);

//	printf("%d-%d-%d %d:%d:%d\r\n",
//			tp->tm_year, tp->tm_mon, tp->tm_mday,
//			tp->tm_hour, tp->tm_min, tp->tm_sec);

	tpDate->year = tp->tm_year - YEAR_ZERO;
	tpDate->month = tp->tm_mon + 1; // 取值1-12
	tpDate->date = tp->tm_mday; // 1-31
	tpDate->hour = tp->tm_hour;
	tpDate->minute = tp->tm_min;
	tpDate->second = tp->tm_sec;
}


