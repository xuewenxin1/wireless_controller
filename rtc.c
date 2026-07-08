#include "rtc.h"
#include "uart.h"
#include "upload.h"



u32 ulSystickCount = 0;

static u8 s_rtc_year;
static u8 s_rtc_month;
static u8 s_rtc_day;
static u8 s_rtc_hour;
static u8 s_rtc_min;
static u8 s_rtc_sec;
static u8 s_rtc_week;

/* VP3011 直接存 1~12 月；屏端不再做(年-2000)偏移，MCU 勿再减年偏移 */
u16 RTC_MonthFromDisplayVp(u16 year, u16 month_vp)
{
	(void)year;
	return month_vp;
}

/* 显示: 0x3010年 0x3011月 0x3012日 0x3013时 0x3014分
   编辑: 0x3070年 0x3072月 0x3073日 0x3074时 0x3075分(0x3071保留) */
void RTC_WriteDisplayTime(u16 year, u16 month, u16 day, u16 hour, u16 minute)
{
	write_dgus_vp((u32)0x3010, (u8 *)&year, 1);
	write_dgus_vp((u32)0x3011, (u8 *)&month, 1);
	write_dgus_vp((u32)0x3012, (u8 *)&day, 1);
	write_dgus_vp((u32)0x3013, (u8 *)&hour, 1);
	write_dgus_vp((u32)0x3014, (u8 *)&minute, 1);
}

void RTC_WriteEditTime(u16 year, u16 month, u16 day, u16 hour, u16 minute)
{
	write_dgus_vp((u32)0x3070, (u8 *)&year, 1);
	write_dgus_vp((u32)0x3072, (u8 *)&month, 1);
	write_dgus_vp((u32)0x3073, (u8 *)&day, 1);
	write_dgus_vp((u32)0x3074, (u8 *)&hour, 1);
	write_dgus_vp((u32)0x3075, (u8 *)&minute, 1);
}



//设置时间为2021.01.01 星期五 00:00:00
void init_rtc(void)
{
	unsigned char dat1, dat2;
	SetRTCSclOutput();
	//检查有没有掉电
	RTCi2cstart();
	RTCi2cbw(0x64);
	RTCi2cbw(0x1d);
	RTCi2cstop();
	RTCi2cstart();
	RTCi2cbw(0x65);
	dat2 = RTCi2cbr();
	RTCmack();
	RTCi2cbr();
	RTCmnak();
	RTCi2cstop();
	if ((dat2 & 0x02)==0x02)
	{
		//重新配置时间
		RTCi2cstart(); //10-16=RTC设置值 BCD格式
		RTCi2cbw(0x64);
		RTCi2cbw(0x30);
		RTCi2cbw(0x00);
		RTCi2cstop();
		RTCi2cstart();
		RTCi2cbw(0x64);
		RTCi2cbw(0x1c);
		RTCi2cbw(0x48);
		RTCi2cbw(0x00);
		RTCi2cbw(0x40);
		RTCi2cbw(0x10);
		RTCi2cstop();
		RTCi2cstart();
		RTCi2cbw(0x64);
		RTCi2cbw(0x10);
		RTCi2cbw(0x00); //秒
		RTCi2cbw(0x00); //分
		RTCi2cbw(0x00); //时
		RTCi2cbw(0x20); //星期
		RTCi2cbw(0x01); //日
		RTCi2cbw(0x01); //月
		RTCi2cbw(0x21); //年
		RTCi2cstop();
		RTCi2cstart();
		RTCi2cbw(0x64);
		RTCi2cbw(0x1e);
		RTCi2cbw(0x00);
		RTCi2cbw(0x10);
		RTCi2cstop();
	}
}

//void delay_ms(unsigned int n)
//{
//	// SysTick = n;
//	// while (SysTick);
//	int data mi, mj;
//	for (mi = 0; mi < n; mi++)
//		for (mj = 0; mj < 8601; mj++)
//			;
//}


void delay_us(unsigned int n)
{
	int data ui, uj;
	for (ui = 0; ui < n; ui++)
		for (uj = 0; uj < 7; uj++)
			;
}

void rtc_config(unsigned char *prtc_set)
{
	unsigned char dat, dat1;
	RTCi2cstart(); //10-16=RTC设置值 BCD格式
	RTCi2cbw(0x64);
	RTCi2cbw(0x30);
	RTCi2cbw(0x00);
	RTCi2cstop();
	RTCi2cstart();
	RTCi2cbw(0x64);
	RTCi2cbw(0x1c);
	RTCi2cbw(0x48);
	RTCi2cbw(0x00);
	RTCi2cbw(0x40);
	RTCi2cbw(0x10);
	RTCi2cstop();
	RTCi2cstart();
	RTCi2cbw(0x64);
	RTCi2cbw(0x10);
	RTCi2cbw(prtc_set[0]); //秒
	RTCi2cbw(prtc_set[1]); //分
	RTCi2cbw(prtc_set[2]); //时
	RTCi2cbw(prtc_set[3]); //星期
	RTCi2cbw(prtc_set[4]); //日
	RTCi2cbw(prtc_set[5]); //月
	RTCi2cbw(prtc_set[6]); //年
	RTCi2cstop();
	RTCi2cstart();
	RTCi2cbw(0x64);
	RTCi2cbw(0x1e);
	RTCi2cbw(0x00);
	RTCi2cbw(0x10);
	RTCi2cstop();
}

//把RTC读取并处理，写到DGUS对应的变量空间，主程序中每0.5秒调用一次
void rdtime(void)
{
	unsigned char rtcdata[8];
	unsigned char i, n, m;
	SetRTCSclOutput();
	RTCi2cstart();
	RTCi2cbw(0x64);
	RTCi2cbw(0x10);
	RTCi2cstop();
	RTCi2cstart();
	RTCi2cbw(0x65);
	for (i = 6; i > 0; i--)
	{
		rtcdata[i] = RTCi2cbr();
		RTCmack();
	}
	rtcdata[0] = RTCi2cbr();

	RTCmnak();
	RTCi2cstop();
	for (i = 0; i < 3; i++) //年月日转换成HEX
	{
		n = rtcdata[i] / 16;
		m = rtcdata[i] % 16;
		rtcdata[i] = n * 10 + m;
	}
	for (i = 4; i < 7; i++) //时分秒转换成HEX
	{
		n = rtcdata[i] / 16;
		m = rtcdata[i] % 16;
		rtcdata[i] = n * 10 + m;
	}
	//星期处理
	n=0;
	m=rtcdata[3];			//bit     7654 3210
	for(i=0;i<7;i++)		//星期日  0000 0001
	{   					//星期一  0000 0010
		if(m&0x01)  break;	//星期二  0000 0100
							//星期三  0000 1000
		n++;				//星期四  0001 0000
		m=(m>>1);
	}
	rtcdata[3]=n;		//星期是  0-6   对应   星期日、星期一...星期六
	rtcdata[7] = 0;
	s_rtc_week = rtcdata[3];
	{
		/* 与0x009d相同: [年|月][日|时][分|秒] */
		u16 rtc_vp[3];
		rtc_vp[0] = ((u16)rtcdata[0] << 8) | rtcdata[1];
		rtc_vp[1] = ((u16)rtcdata[2] << 8) | rtcdata[4];
		rtc_vp[2] = ((u16)rtcdata[5] << 8) | rtcdata[6];
		write_dgus_vp(0x0010, (u8 *)rtc_vp, 3);
	}
	s_rtc_year = rtcdata[0];
	s_rtc_month = rtcdata[1];
	s_rtc_day = rtcdata[2];
	s_rtc_hour = rtcdata[4];
	s_rtc_min = rtcdata[5];
	s_rtc_sec = rtcdata[6];
	// uart2_send_string_len(rtcdata, 8);
	check_rtc_set();
}

//void readRtc(unsigned char *pBuf)
//{
//	unsigned char rtcdata[8];
//	unsigned char i;
//	SetRTCSclOutput();
//	RTCi2cstart();
//	RTCi2cbw(0x64);
//	RTCi2cbw(0x00);
//	RTCi2cstart();
//	RTCi2cbw(0x65);
//	for (i = 6; i > 0; i--)
//	{
//		rtcdata[i] = RTCi2cbr();
//		RTCmack();
//	}
//	rtcdata[0] = RTCi2cbr();
//	RTCmnak();
//	RTCi2cstop();
//	rtcdata[4] &= 0x7F;
//	for (i = 0; i < 7; i++) pBuf[i] = rtcdata[i];
//}

unsigned char BCD(unsigned char dat)
{
	return ((dat / 10) << 4) | (dat % 10);
}

//unsigned char IBCD(unsigned char dat)
//{
//	return (dat >> 4) * 10 + (dat & 0x0f);
//}

unsigned char rtc_get_week(unsigned char year, unsigned char month, unsigned char day)
{
	unsigned int tmp, mon, y;
	unsigned char week;
	if ((month == 1) || (month == 2))
	{
		mon = month + 12;
		y = year - 1;
	}
	else
	{
		mon = month;
		y = year;
	}
	tmp = y + (y / 4) + (((mon + 1) * 26) / 10) + day - 36;
	week = tmp % 7;
	return week;
}

void check_rtc_set(void)
{
	u8 rtc_parm[8] = 0;
	u8 rtc_set[8] = 0;
	read_dgus_vp((u32)(0x009c), (u8 *)&rtc_parm[0], 4);
	if ((rtc_parm[0] == 0x5A) && (rtc_parm[1] == 0xA5)) //启动配置//用户更改不了时间，先注释掉
	{
		rtc_set[6] = BCD(rtc_parm[2]);
		rtc_set[5] = BCD(rtc_parm[3]);
		rtc_set[4] = BCD(rtc_parm[4]);
		rtc_set[3] = (u8)(1<<rtc_get_week(rtc_parm[2], rtc_parm[3], rtc_parm[4]));
		rtc_set[2] = BCD(rtc_parm[5]);
		rtc_set[1] = BCD(rtc_parm[6]);
		rtc_set[0] = BCD(rtc_parm[7]);
		rtc_config(rtc_set);
		{
			u16 yfull = rtc_parm[2];

			if(yfull < 100)
				yfull += 2000;
			RTC_WriteDisplayTime(yfull, rtc_parm[3], rtc_parm[4], rtc_parm[5], rtc_parm[6]);
			RTC_WriteEditTime(yfull, rtc_parm[3], rtc_parm[4], rtc_parm[5], rtc_parm[6]);
		}
		rtc_parm[0] = 0;
		rtc_parm[1] = 0;
		write_dgus_vp((u32)(0x009c), rtc_parm, 1);
		upload_timestamp_from_rtc();
	}
}

//SD2058 I2C驱动

void RTCi2cstart(void)
{
	SetRTCSdaOutput();
	RTC_SDA = 1;
	RTC_SCL = 1;
	delay_us(15);
	RTC_SDA = 0;
	delay_us(15);
	RTC_SCL = 0;
	delay_us(15);
}

void RTCi2cstop(void)
{
	SetRTCSdaOutput();
	RTC_SDA = 0;
	RTC_SCL = 1;
	delay_us(15);
	RTC_SDA = 1;
	delay_us(15);
	SetRTCSdaInput();
}

void RTCmack(void)
{
	SetRTCSdaOutput();
	RTC_SDA = 0;
	delay_us(5);
	RTC_SCL = 1;
	delay_us(5);
	RTC_SCL = 0;
	delay_us(5);
}

void RTCmnak(void)
{
	SetRTCSdaOutput();
	RTC_SDA = 1;
	delay_us(5);
	RTC_SCL = 1;
	delay_us(5);
	RTC_SCL = 0;
	delay_us(5);
}

void RTCcack(void)
{
	unsigned char i;
	SetRTCSdaInput();
	RTC_SDA = 1;
	delay_us(5);
	RTC_SCL = 1;
	delay_us(5);
	for (i = 0; i < 50; i++)
	{
		if (!RTC_SDA)
		{
			break;
		}
		delay_us(5);
	}
	RTC_SCL = 0;
	delay_us(5);
	SetRTCSdaOutput();
}

//I2C 写入1个字节
void RTCi2cbw(unsigned char dat)
{
	char i;
	SetRTCSdaOutput();
	for (i = 0; i < 8; i++)
	{
		if (dat & 0x80)
		{
			RTC_SDA = 1;
		}
		else
		{
			RTC_SDA = 0;
		}
		dat = (dat << 1);
		delay_us(5);
		RTC_SCL = 1;
		delay_us(5);
		RTC_SCL = 0;
		delay_us(5);
	}
	RTCcack();
}

//i2c 读取1个字节数据
unsigned char RTCi2cbr(void)
{
	unsigned char i;
	unsigned char dat;
	SetRTCSdaInput();
	for (i = 0; i < 8; i++)
	{
		delay_us(5);
		RTC_SCL = 1;
		delay_us(5);
		dat = (dat << 1);
		if (RTC_SDA)
		{
			dat = dat | 0x01;
		}
		else
		{
			dat = dat & 0xFE;
		}
		//dat=(dat<<1);
		RTC_SCL = 0;
		delay_us(5);
	}
	return (dat);
}
void RTCUpdate(void)
{
	static u32 ulUpdateTime = 0;
	u16 year, month, day, hour, minute;
	if(GetTimeDiff(GetSystickTime(),ulUpdateTime) > 500)
	{//每0.5秒调用一次F 
		ulUpdateTime = GetSystickTime();
		rdtime();
		year   = (u16)s_rtc_year + 2000;
		month  = s_rtc_month;
		day    = s_rtc_day;
		hour   = s_rtc_hour;
		minute = s_rtc_min;
		RTC_WriteDisplayTime(year, month, day, hour, minute);
	}
}

/***********************************************************************************************************************
* Function Name: GetSystickTime
* Description  : get system time
* Arguments    : None
* Return Value : sys time
***********************************************************************************************************************/
u32 GetSystickTime(void)
{
	return ulSystickCount;
}


/***********************************************************************************************************************
* Function Name: GetTimeDiff
* Description  : 
* Arguments    : 	currtime:	current sys time
*					lasttime:	last time
* Return Value : sys time diff
***********************************************************************************************************************/
u32 GetTimeDiff(u32 currtime,u32 lasttime)
{
	if(currtime >= lasttime)
	{
		return currtime - lasttime;
	}
	else
	{
		
		return 0xffffffff - lasttime + currtime;
	}
}

u8 rtc_get_weekday(void)
{
	return s_rtc_week;
}

static u8 rtc_is_leap(u16 year)
{
	return (u8)((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
}

static u8 rtc_days_in_month(u16 year, u8 month)
{
	static const u8 mdays[] = {31,28,31,30,31,30,31,31,30,31,30,31};

	if(month == 2 && rtc_is_leap(year))
		return 29;
	if(month >= 1 && month <= 12)
		return mdays[month - 1];
	return 30;
}

u32 RTC_GetUnixLocal(void)
{
	u32 days = 0;
	u16 year;
	u8 month;
	u16 full_year;

	full_year = (u16)s_rtc_year + 2000;
	for(year = 1970; year < full_year; year++)
		days += rtc_is_leap(year) ? 366UL : 365UL;
	for(month = 1; month < s_rtc_month; month++)
		days += rtc_days_in_month(full_year, month);
	if(s_rtc_day > 0)
		days += (u32)(s_rtc_day - 1);
	return days * 86400UL + (u32)s_rtc_hour * 3600UL + (u32)s_rtc_min * 60UL + (u32)s_rtc_sec;
}

u16 RTC_GetFullYear(void)
{
	return (u16)s_rtc_year + 2000;
}

u32 RTC_UnixFromLocalParts(u16 full_year, u8 month, u8 day, u8 hour, u8 minute, u8 second)
{
	u32 days = 0;
	u16 year;
	u8 m;

	if(full_year < 1970 || month < 1 || month > 12 || day < 1)
		return 0;
	for(year = 1970; year < full_year; year++)
		days += rtc_is_leap(year) ? 366UL : 365UL;
	for(m = 1; m < month; m++)
		days += rtc_days_in_month(full_year, m);
	if(day > rtc_days_in_month(full_year, month))
		day = rtc_days_in_month(full_year, month);
	days += (u32)(day - 1);
	return days * 86400UL + (u32)hour * 3600UL + (u32)minute * 60UL + (u32)second;
}


                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      