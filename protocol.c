/**********************************Copyright (c)**********************************
**                       版权所有 (C), 2015-2020, 涂鸦科技
**
**                             http://www.tuya.com
**
*********************************************************************************/
/**
 * @file    protocol.c
 * @author  涂鸦综合协议开发组
 * @version v2.5.6
 * @date    2020.12.16
 * @brief                
 *                       *******非常重要，一定要看哦！！！********
 *          1. 用户在此文件中实现数据下发/上报功能
 *          2. DP的ID/TYPE及数据处理函数都需要用户按照实际定义实现
 *          3. 当开始某些宏定义后需要用户实现代码的函数内部有#err提示,完成函数后请删除该#err
 */

/****************************** 免责声明 ！！！ *******************************
由于MCU类型和编译环境多种多样，所以此代码仅供参考，用户请自行把控最终代码质量，
涂鸦不对MCU功能结果负责。
******************************************************************************/

/******************************************************************************
                                移植须知:
1:MCU必须在while中直接调用mcu_api.c内的wifi_uart_service()函数
2:程序正常初始化完成后,建议不进行关串口中断,如必须关中断,关中断时间必须短,关中断会引起串口数据包丢失
3:请勿在中断/定时器中断内调用上报函数
******************************************************************************/

#include "wifi.h"
#include "uart.h"
#include "app_core.h"
#include "control.h"
#include "protocol.h"
#include "rtc.h"
#include "stdio.h"

extern u16 Defrosting;
#include <stdlib.h>
#ifdef WEATHER_ENABLE
/**
 * @var    weather_choose
 * @brief  天气数据参数选择数组
 * @note   用户可以自定义需要的参数，注释或者取消注释即可，注意更改
 */
const char *weather_choose[WEATHER_CHOOSE_CNT] = {
    "temp",
    "humidity",
    "condition",
    "pm25",
    /*"pressure",
    "realFeel",
    "uvi",
    "tips",
    "windDir",
    "windLevel",
    "windSpeed",
    "sunRise",
    "sunSet",
    "aqi",
    "so2 ",
    "rank",
    "pm10",
    "o3",
    "no2",
    "co",
    "conditionNum",*/
};
#endif


/******************************************************************************
                              第一步:初始化
1:在需要使用到wifi相关文件的文件中include "wifi.h"
2:在MCU初始化中调用mcu_api.c文件中的wifi_protocol_init()函数
3:将MCU串口单字节发送函数填入protocol.c文件中uart_transmit_output函数内,并删除#error
4:在MCU串口接收函数中调用mcu_api.c文件内的uart_receive_input函数,并将接收到的字节作为参数传入
5:单片机进入while循环后调用mcu_api.c文件内的wifi_uart_service()函数
******************************************************************************/

/******************************************************************************
                        1:dp数据点序列类型对照表
          **此为自动生成代码,如在开发平台有相关修改请重新下载MCU_SDK**         
******************************************************************************/
const DOWNLOAD_CMD_S download_cmd[] =
{
  /* 仅可下发 DP；只上报型写入此表会导致模块/云端不下发 */
  {DPID_SWITCH, DP_TYPE_BOOL},
  {DPID_MODE, DP_TYPE_ENUM},
  {DPID_CHILD_LOCK, DP_TYPE_BOOL},
  {DPID_TEMP_SET, DP_TYPE_VALUE},
  {DPID_TEMP_UNIT_CONVERT, DP_TYPE_ENUM},
  {DPID_DEFROST, DP_TYPE_BOOL},
  {DPID_RELAY_STATUS, DP_TYPE_ENUM},
  {DPID_TIMESTAMP, DP_TYPE_STRING},
  {DPID_INDOOR_UNIT, DP_TYPE_RAW},
  {DPID_HISTORY_FAULT_EMPTY, DP_TYPE_BOOL},
  {DPID_INSIDE_TIME_OPEN, DP_TYPE_RAW},
  {DPID_EXTERNAL_TIME_OPEN, DP_TYPE_RAW},
  {DPID_INSIDE_TIME_CLOSE, DP_TYPE_RAW},
  {DPID_EXTERNAL_TIME_CLOSE, DP_TYPE_RAW},
};



/******************************************************************************
                           2:串口单字节发送函数
请将MCU串口发送函数填入该函数内,并将接收到的数据作为参数传入串口发送函数
******************************************************************************/

/**
 * @brief  串口发送数据
 * @param[in] {value} 串口要发送的1字节数据
 * @return Null
 */
void uart_transmit_output(unsigned char value)
{
//    #error "请将MCU串口发送函数填入该函数,并删除该行"
	Uart2SendData(value);
/*
    //Example:
    extern void Uart_PutChar(unsigned char value);
    Uart_PutChar(value);	                                //串口发送函数
*/
}

/******************************************************************************
                           第二步:实现具体用户函数
1:APP下发数据处理
2:数据上报处理
******************************************************************************/

/******************************************************************************
                            1:所有数据上报处理
当前函数处理全部数据上报(包括可下发/可上报和只上报)
  需要用户按照实际情况实现:
  1:需要实现可下发/可上报数据点上报
  2:需要实现只上报数据点上报
此函数为MCU内部必须调用
用户也可调用此函数实现全部数据上报
******************************************************************************/

//自动化生成数据上报函数

/**
 * @brief  系统所有dp点信息上传,实现APP和muc数据同步
 * @param  Null
 * @return Null
 * @note   此函数SDK内部需调用，MCU必须实现该函数内数据上报功能，包括只上报和可上报可下发型数据
 */
unsigned char	SendCnt = 0;
void all_data_update(void)
{
	App_UploadStateQueryReply();
}

static unsigned char s_last_switch_dl = 0xFF;

void protocol_sync_switch_shadow(unsigned char val)
{
	s_last_switch_dl = val;
}



/******************************************************************************
                                WARNING!!!    
                            2:所有数据上报处理
自动化代码模板函数,具体请用户自行实现数据处理
******************************************************************************/
/*****************************************************************************
函数名称 : dp_download_switch_handle
功能描述 : 针对DPID_SWITCH的处理函数
输入参数 : value:数据源数据
        : length:数据长度
返回参数 : 成功返回:SUCCESS/失败返回:ERROR
使用说明 : 可下发可上报类型,需要在处理完数据后上报处理结果至app
*****************************************************************************/
static unsigned char dp_download_switch_handle(const unsigned char value[], unsigned short length)
{
    unsigned char ret;
    unsigned char switch_1;
    u16 control_mode;
	u16 external_switch = 0;
	static unsigned char s_last_switch = 0xFF;

    switch_1 = mcu_get_dp_download_bool(value,length);
	if(switch_1 == s_last_switch)
	{
		printf("[WIFI] dl switch=%bu dup\r\n", switch_1);
		mcu_dp_bool_update(DPID_SWITCH, switch_1);
		App_UploadShadowBool(DPID_SWITCH, switch_1);
		return SUCCESS;
	}
	s_last_switch = switch_1;
	printf("[WIFI] dl switch=%bu\r\n", switch_1);
	App_SyncLinkApplySwitch(switch_1, SYNC_SRC_APP);
	Ready_To_Save();
	read_dgus_vp((u32)(0x4021),(u8 *)&external_switch,1);
	read_dgus_vp((u32)(0x2002),(u8 *)&control_mode,1);
	printf("[WIFI] dl switch done vp4021=%u mode2002=%u\r\n",
		(u16)external_switch, (u16)control_mode);
	
    ret = mcu_dp_bool_update(DPID_SWITCH,switch_1);
	App_UploadShadowBool(DPID_SWITCH, switch_1);
    if(ret == SUCCESS)
        return SUCCESS;
    else
        return ERROR;
}
/*****************************************************************************
函数名称 : dp_download_mode_handle
功能描述 : 针对DPID_MODE的处理函数
输入参数 : value:数据源数据
        : length:数据长度
返回参数 : 成功返回:SUCCESS/失败返回:ERROR
使用说明 : 可下发可上报类型,需要在处理完数据后上报处理结果至app
*****************************************************************************/
static unsigned char dp_download_mode_handle(const unsigned char value[], unsigned short length)
{
    //示例:当前DP类型为ENUM
    unsigned char ret;
    unsigned char mode;
    u16 control_mode;
	u16 external_switch;
    mode = mcu_get_dp_download_enum(value,length);
    printf("[WIFI] dl mode=%bu\r\n", mode);
    switch(mode) {
        case 0:
			control_mode = 1;
			write_dgus_vp((u32)(0x2002),(u8*)&control_mode,1);
			read_dgus_vp((u32)(0x4021),(u8 *)&external_switch,1);
			if(external_switch == 0)
				App_SyncLinkApplySwitch(1, SYNC_SRC_APP);
			else
				App_ModbusTriggerUserWrite();
        break;
        
        case 1:
			control_mode = 2;
			write_dgus_vp((u32)(0x2002),(u8*)&control_mode,1);
			read_dgus_vp((u32)(0x4021),(u8 *)&external_switch,1);
			if(external_switch == 0)
				App_SyncLinkApplySwitch(1, SYNC_SRC_APP);
			else
				App_ModbusTriggerUserWrite();
        break;
        
        case 2:
			control_mode = 3;
			write_dgus_vp((u32)(0x2002),(u8*)&control_mode,1);
			read_dgus_vp((u32)(0x4021),(u8 *)&external_switch,1);
			if(external_switch == 0)
				App_SyncLinkApplySwitch(1, SYNC_SRC_APP);
			else
				App_ModbusTriggerUserWrite();
        break;
        
        case 3:
			App_SyncLinkApplySwitch(0, SYNC_SRC_APP);
        break;
        
        default:
			
        break;
    }
    Ready_To_Save();
	{
		u16 vp4021, vp2002;
		read_dgus_vp((u32)(0x4021),(u8 *)&vp4021,1);
		read_dgus_vp((u32)(0x2002),(u8 *)&vp2002,1);
		printf("[WIFI] dl mode done vp4021=%u mode2002=%u\r\n",
			(u16)vp4021, (u16)vp2002);
	}
    
    ret = mcu_dp_enum_update(DPID_MODE, mode);
	App_UploadShadowEnum(DPID_MODE, mode);
    if(ret == SUCCESS)
        return SUCCESS;
    else
        return ERROR;
}
/*****************************************************************************
函数名称 : dp_download_child_lock_handle
功能描述 : 针对DPID_CHILD_LOCK的处理函数
输入参数 : value:数据源数据
        : length:数据长度
返回参数 : 成功返回:SUCCESS/失败返回:ERROR
使用说明 : 可下发可上报类型,需要在处理完数据后上报处理结果至app
*****************************************************************************/
static unsigned char dp_download_child_lock_handle(const unsigned char value[], unsigned short length)
{
    //示例:当前DP类型为BOOL
    unsigned char ret;
    //0:off/1:on
    unsigned char child_lock;
    
    child_lock = mcu_get_dp_download_bool(value,length);
	write_dgus_vp((u32)(0x2010),(u8 *)&child_lock,1);
	if(!child_lock)
	{
		App_HomePage(TRUE);
		App_HomePageIcon();
	}
    ret = mcu_dp_bool_update(DPID_CHILD_LOCK,child_lock);
	App_UploadShadowBool(DPID_CHILD_LOCK, child_lock);
    if(ret == SUCCESS)
        return SUCCESS;
    else
        return ERROR;
}
/*****************************************************************************
函数名称 : dp_download_temp_set_handle
功能描述 : 针对DPID_TEMP_SET的处理函数
输入参数 : value:数据源数据
        : length:数据长度
返回参数 : 成功返回:SUCCESS/失败返回:ERROR
使用说明 : 可下发可上报类型,需要在处理完数据后上报处理结果至app
*****************************************************************************/
static unsigned char dp_download_temp_set_handle(const unsigned char value[], unsigned short length)
{
    unsigned char ret;
    unsigned long temp_set;
    unsigned long temp_set1;
    u16 unit;
    temp_set = mcu_get_dp_download_value(value,length);
    printf("[WIFI] dl temp_set=%lu\r\n", temp_set);
    /*
    //VALUE type data processing
    
    */
	write_dgus_vp((u32)(0x2005),(u8 *)&temp_set,1);
	read_dgus_vp((u32)(0x2002),(u8 *)&temp_set1,1);
	read_dgus_vp((u32)(0x2003),(u8 *)&unit,1);
	App_SetTempIntWrite((u8)temp_set1, (u8)unit, (u16)temp_set);
	App_SyncSetTempCachesFromC((u16)temp_set);
	App_ModbusTriggerUserWrite();
	Ready_To_Save();
    ret = mcu_dp_value_update(DPID_TEMP_SET,temp_set);
	App_UploadShadowValue(DPID_TEMP_SET, temp_set);
    if(ret == SUCCESS)
        return SUCCESS;
    else
        return ERROR;
}
/*****************************************************************************
函数名称 : dp_download_temp_unit_convert_handle
功能描述 : 针对DPID_TEMP_UNIT_CONVERT的处理函数
输入参数 : value:数据源数据
        : length:数据长度
返回参数 : 成功返回:SUCCESS/失败返回:ERROR
使用说明 : 可下发可上报类型,需要在处理完数据后上报处理结果至app
*****************************************************************************/
static unsigned char dp_download_temp_unit_convert_handle(const unsigned char value[], unsigned short length)
{
    //示例:当前DP类型为ENUM
    unsigned char ret;
    unsigned char temp_unit_convert;
    
    temp_unit_convert = mcu_get_dp_download_enum(value,length);
    switch(temp_unit_convert) {
        case 0:
        break;
        
        case 1:
        break;
        
        default:
    
        break;
    }
    write_dgus_vp((u32)(0x2003),(u8 *)&temp_unit_convert,1);
	App_UnitChangePro();
	App_HomePage(TRUE);
	Ready_To_Save();
    ret = mcu_dp_enum_update(DPID_TEMP_UNIT_CONVERT, temp_unit_convert);
	App_UploadShadowEnum(DPID_TEMP_UNIT_CONVERT, temp_unit_convert);
    if(ret == SUCCESS)
        return SUCCESS;
    else
        return ERROR;
}
/*****************************************************************************
函数名称 : dp_download_defrost_handle
功能描述 : 针对DPID_DEFROST的处理函数
输入参数 : value:数据源数据
        : length:数据长度
返回参数 : 成功返回:SUCCESS/失败返回:ERROR
使用说明 : 可下发可上报类型,需要在处理完数据后上报处理结果至app
*****************************************************************************/
static unsigned char dp_download_defrost_handle(const unsigned char value[], unsigned short length)
{
    //示例:当前DP类型为BOOL
    unsigned char ret;
    //0:off/1:on
    unsigned char defrost;
    
    defrost = mcu_get_dp_download_bool(value,length);
	if(defrost)
	{
		App_ModbusStartManualDefrost();
	}
	else
	{
		u16 zero = 0;
		write_dgus_vp((u32)(0x201b),(u8 *)&zero,1);
		Defrosting = FALSE;
		App_ModbusClearManualDefrost();
		App_ModbusTriggerUserWrite();
	}
    ret = mcu_dp_bool_update(DPID_DEFROST,defrost);
	App_UploadShadowBool(DPID_DEFROST, defrost);
    if(ret == SUCCESS)
        return SUCCESS;
    else
        return ERROR;
}
/*****************************************************************************
函数名称 : dp_download_relay_status_handle
功能描述 : 针对DPID_RELAY_STATUS的处理函数
输入参数 : value:数据源数据
        : length:数据长度
返回参数 : 成功返回:SUCCESS/失败返回:ERROR
使用说明 : 可下发可上报类型,需要在处理完数据后上报处理结果至app
*****************************************************************************/
static unsigned char dp_download_relay_status_handle(const unsigned char value[], unsigned short length)
{
    //示例:当前DP类型为ENUM
    unsigned char ret;
    unsigned char relay_status;
    
    relay_status = mcu_get_dp_download_enum(value,length);
    switch(relay_status) {
        case 0:
        break;
        
        case 1:
        break;
        
        default:
    
        break;
    }
    write_dgus_vp((u32)(0x3508),(u8 *)&relay_status,1);
	Ready_To_Save();
    ret = mcu_dp_enum_update(DPID_RELAY_STATUS, relay_status);
	App_UploadShadowEnum(DPID_RELAY_STATUS, relay_status);
    if(ret == SUCCESS)
        return SUCCESS;
    else
        return ERROR;
}
/*****************************************************************************
函数名称 : dp_download_timestamp_handle
功能描述 : 针对DPID_TIMESTAMP的处理函数
输入参数 : value:数据源数据
        : length:数据长度
返回参数 : 成功返回:SUCCESS/失败返回:ERROR
使用说明 : 可下发可上报类型,需要在处理完数据后上报处理结果至app
*****************************************************************************/
typedef struct {
    u8 year;
    u8  month;
    u8  day;
    u8  hour;
    u8  minute;
    u8  second;
} datetime_t;

static u8 is_leap_year(u16 year)
{
    return ((year % 4 == 0 && year % 100 != 0) ||
            (year % 400 == 0));
}

static const u8 mdays[] = {
	31, 28, 31, 30, 31, 30,
	31, 31, 30, 31, 30, 31
};

void timestamp_to_ymdhms(u32 timestamp, datetime_t *dt)
{
    u32 sec = timestamp;
    u32 days, rem;
	u8 m,leap;
	u16 full_year;

    /* 1. 计算天数和当天秒数 */
    days = sec / 86400UL;
	printf("days = %lu\r\n",days);
    rem  = sec % 86400UL;

    /* 2. 时分秒 */
    dt->hour   = (u8)(rem / 3600UL);
    dt->minute = (u8)((rem % 3600UL) / 60UL);
    dt->second = (rem % 60UL);

    /* 3. 计算年份 */
    full_year = 1970;   // 粗略年份;
    while (1) {
        u16 ydays = is_leap_year(full_year) ? 366 : 365;
        if (days < ydays)
            break;
        days -= ydays;
        full_year++;
    }
	/* 转成 2000 年起的偏移 */
    dt->year = (u8)(full_year - 2000);
    /* 4. 月日 */
    leap = is_leap_year(full_year);
    dt->month = 0;

    for (m = 0; m < 12; m++) {
        u8 dim = mdays[m];
        if (m == 1 && leap)
            dim = 29;

        if (days < dim)
            break;

        days -= dim;
        dt->month++;
    }

    dt->month += 1;  // ? 转成人类月份 1~12
    dt->day    = (u8)(days + 1);
}
#define TIMEZONE_OFFSET_SEC   (8 * 3600UL)   // UTC+8
static unsigned char dp_download_timestamp_handle(const unsigned char value[], unsigned short length)
{
    //示例:当前DP类型为STRING
    unsigned char ret;
	unsigned short	RTCC2W_write[3] = {0};
	u8 rtc_parm[2] = 0;
	datetime_t dt = {0};
	u32 ts;
	u16 year;
	ts = strtoul((const char *)value, NULL, 10);
	ts += TIMEZONE_OFFSET_SEC;
	
	timestamp_to_ymdhms(ts, &dt);
	printf("time:%04d-%02d-%02d %02d:%02d:%02d\r\n",(u16)dt.year+2000,(u16)dt.month,(u16)dt.day,(u16)dt.hour,(u16)dt.minute,(u16)dt.second);
	year = (u16)dt.year + 2000;
	RTC_WriteDisplayTime(year, dt.month, dt.day, dt.hour, dt.minute);
	RTC_WriteEditTime(year, dt.month, dt.day, dt.hour, dt.minute);
	
	RTCC2W_write[0] = (dt.year << 8) | (dt.month & 0xff);
	RTCC2W_write[1] = (dt.day << 8) | (dt.hour & 0xff);
	RTCC2W_write[2] = (dt.minute << 8);
	write_dgus_vp((u32)(0x009d),(u8*)&RTCC2W_write,3);//向系统接口预先存下时间待按下确认按钮后直接在显示核写入RTC
	
	rtc_parm[0] = 0x5a;
	rtc_parm[1] = 0xa5;
	write_dgus_vp((u32)(0x009c), rtc_parm, 1);

    //There should be a report after processing the DP
    ret = mcu_dp_string_update(DPID_TIMESTAMP,value, length);
    if(ret == SUCCESS)
        return SUCCESS;
    else
        return ERROR;
}
/*****************************************************************************
函数名称 : dp_download_indoor_unit_handle
功能描述 : 针对DPID_INDOOR_UNIT的处理函数
输入参数 : value:数据源数据
        : length:数据长度
返回参数 : 成功返回:SUCCESS/失败返回:ERROR
使用说明 : 可下发可上报类型,需要在处理完数据后上报处理结果至app
*****************************************************************************/
static u16 mode_map_to_dgus(u8 cloud_mode)
{
    switch (cloud_mode) {
        case 0: return 4;  // 自动
        case 1: return 0;  // 制冷
        case 2: return 3;  // 制热
        case 3: return 2;  // 送风
        case 4: return 1;  // 除湿
        default: return 1;
    }
}
static unsigned char dp_download_indoor_unit_handle(const unsigned char value[], unsigned short length)
{
	u8 offset = 0;
	u8 idx;
	u8 i;
	u8 wr_list[6];
	u8 wr_cnt = 0;
	u8 unit_cnt = 0;
	u8 off_cmd_cnt = 0;
	u16 unit_id;
	u16 power;
	u16 set_temp;
	u16 mode,dgus_mode;
	u16 fan_speed;
	u16 room_temp;

	if(length >= 6)
		printf("[WIFI] dl indoor len=%u\r\n", (u16)length);

    while (offset + 6 <= length)
    {
        const unsigned char *p = &value[offset];
		u16 old_pwr, old_mode, old_temp, old_fan;

        unit_id   = p[0];
        power     = p[1];
        room_temp = p[2];
        mode      = p[3];
        fan_speed = p[4];
        set_temp  = p[5];

        if (unit_id < 1 || unit_id > 32) goto next;
        if (power > 1) goto next;
        if (set_temp < 16 || set_temp > 32) goto next;
        if (mode > 4) goto next;
        if (fan_speed > 4) goto next;

        dgus_mode = mode_map_to_dgus(mode);
        idx = unit_id - 1;
		if(idx >= 6)
			goto next;

		read_dgus_vp(0x4010 + idx, (u8 *)&old_pwr, 1);
		read_dgus_vp(0x4700 + idx, (u8 *)&old_mode, 1);
		read_dgus_vp(0x1020 + idx, (u8 *)&old_temp, 1);
		read_dgus_vp(0x4710 + idx, (u8 *)&old_fan, 1);

		unit_cnt++;
		if(power == 0)
			off_cmd_cnt++;

        write_dgus_vp(0x4010 + idx, (u8 *)&power,1);
        write_dgus_vp(0x1020 + idx, (u8 *)&set_temp, 1);
        {
            u16 set_f = (u16)App_TempUnitTrans((signed short)set_temp, 'F');
            write_dgus_vp(VP_INDOOR_SET_F_BASE + idx, (u8 *)&set_f, 1);
        }
        write_dgus_vp(0x4700 + idx, (u8 *)&dgus_mode, 1);
        write_dgus_vp(0x4710 + idx, (u8 *)&fan_speed, 1);
        {
            float room_c = (float)room_temp;
            float room_f = (float)App_TempUnitTrans((signed short)room_temp, 'F');

            write_dgus_vp(VP_INDOOR_ROOM_C_BASE + idx * 2, (u8 *)&room_c, 2);
            write_dgus_vp(VP_INDOOR_ROOM_F_BASE + idx * 2, (u8 *)&room_f, 2);
            write_dgus_vp(0x1100 + idx * 0x20, (u8 *)&room_c, 2);
        }

		if(old_pwr != power || old_mode != dgus_mode || old_temp != set_temp || old_fan != fan_speed)
		{
			printf("[WIFI] dl indoor id=%u pwr=%bu temp=%u mode=%bu\r\n",
				(u16)unit_id, (u8)power, (u16)set_temp, (u8)mode);
			for(i = 0; i < wr_cnt; i++)
			{
				if(wr_list[i] == idx)
					goto next;
			}
			if(wr_cnt < 6)
				wr_list[wr_cnt++] = idx;
		}

next:
        offset += 6;
    }

	if(unit_cnt)
	{
		u8 all_off = 1;
		u16 vp_pwr;

		App_HomePageSyncIndoorDisplay();
		for(i = 0; i < 6; i++)
		{
			read_dgus_vp((u32)(0x4010 + i), (u8 *)&vp_pwr, 1);
			if(vp_pwr != 0)
			{
				all_off = 0;
				break;
			}
		}
		if(all_off || off_cmd_cnt >= 2)
			App_ModbusTriggerGroupControlWriteOff();
		else
		{
			for(i = 0; i < wr_cnt; i++)
				App_ModbusTriggerIndoorUnitWrite(wr_list[i], 0);
		}
		App_ControlIndoorSwitchSyncFromVp();
		App_UploadIndoorUnitRequest();
		printf("[WIFI] dl indoor done units=%bu wr=%bu off=%bu\r\n",
			unit_cnt, wr_cnt, off_cmd_cnt);
	}
    return SUCCESS;
}
/*****************************************************************************
函数名称 : dp_download_history_fault_empty_handle
功能描述 : 针对DPID_HISTORY_FAULT_EMPTY的处理函数
输入参数 : value:数据源数据
        : length:数据长度
返回参数 : 成功返回:SUCCESS/失败返回:ERROR
使用说明 : 只下发类型,需要在处理完数据后上报处理结果至app
*****************************************************************************/
static unsigned char dp_download_history_fault_empty_handle(const unsigned char value[], unsigned short length)
{
    //示例:当前DP类型为BOOL
    unsigned char ret;
    //0:off/1:on
    unsigned char history_fault_empty;
    
    history_fault_empty = mcu_get_dp_download_bool(value,length);
    if(history_fault_empty == 0) {
        //bool off
    }else {
		App_ClearErrorHistory();
        //bool on
    }
  
    //There should be a report after processing the DP
    ret = mcu_dp_bool_update(DPID_HISTORY_FAULT_EMPTY,history_fault_empty);
    if(ret == SUCCESS)
        return SUCCESS;
    else
        return ERROR;
}
/*****************************************************************************
函数名称 : dp_download_inside_time_open_handle
功能描述 : 针对DPID_INSIDE_TIME_OPEN的处理函数
输入参数 : value:数据源数据
        : length:数据长度
返回参数 : 成功返回:SUCCESS/失败返回:ERROR
使用说明 : 可下发可上报类型,需要在处理完数据后上报处理结果至app
*****************************************************************************/
static unsigned char dp_download_inside_time_open_handle(const unsigned char value[], unsigned short length)
{
    //示例:当前DP类型为RAW
    unsigned char ret;
	unsigned char i;
	unsigned char week_mask;
	u16 switch1,hour,minute,week,temp,temp_mode,dgus_mode,fan_speed,room,set_val,room1;
    unsigned char *p = &value[0];
    // 1. 数据长度校验 (协议固定为 8 字节)
	if (length != 8) {
		return ERROR;
	}
	switch1 = p[0];
	// 2. 解析 value 并写入 DGUS VP 地址
	// (1) 启用标志 (0:禁用, 1:启用)
	write_dgus_vp(0x4825, (u8 *)&switch1, 1);

	// (2) 小时 (0-23)
	hour = p[1];
	write_dgus_vp(0x1084, (u8 *)&hour, 1);
	
	// (3) 分钟 (0-59)
	minute = p[2];
	write_dgus_vp(0x1085, (u8 *)&minute, 1);

	// (4) 重复模式 (BitMask 转 7个复选框)
	// 先清空目标区域，防止残留数据干扰
	week_mask = 0;
	week = p[3];
	// 遍历 7 天，根据 BitMask 写入 0x4510 ~ 0x4516
	
	for(i = 0; i < 7; i++) {
		// 检查 value[3] 的第 i 位是否为 1
		if ((week >> i) & 0x01) {
			set_val = 1;
			write_dgus_vp(0x4510 + i, (u8 *)&set_val, 1);
		}
	}

	// (5) 设定温度
	temp = p[4];
	write_dgus_vp(VP_TIMER_IN_TEMP_C, (u8 *)&temp, 1);
	{
		u16 temp_f = (u16)App_TempUnitTrans((signed short)temp, 'F');
		write_dgus_vp(VP_TIMER_IN_TEMP_F, (u8 *)&temp_f, 1);
	}

	// (6) 模式 (注意表格 0x01=自动, 0x02=制冷...)
	temp_mode = p[5];
    switch(temp_mode) {
        case 0x01: dgus_mode = 4; break; // 自动
        case 0x02: dgus_mode = 0; break; // 制冷
        case 0x03: dgus_mode = 3; break; // 制热
        case 0x04: dgus_mode = 1; break; // 除湿
        case 0x05: dgus_mode = 2; break; // 送风
        default:   dgus_mode = 4; break; // 默认自动
    }
	write_dgus_vp(0x4837, (u8 *)&dgus_mode, 1);

	// (7) 风速
	fan_speed = p[6];
	write_dgus_vp(0x3081, (u8 *)&fan_speed, 1);

	// (8) 房间号
	room = p[7];
	for(i=0;i<6;i++){
		if (room & (1 << i)) {
			room1 = 1;
			write_dgus_vp(0x4860+i, (u8 *)&room1, 1);
		}
	}
//	write_dgus_vp(0x4836, (u8 *)&room, 1);

	App_ModbusApplyIndoorTimerVpToUnits(1, 0);
	Ready_To_Save();

    //There should be a report after processing the DP
    ret = mcu_dp_raw_update(DPID_INSIDE_TIME_OPEN,value,length);
    if(ret == SUCCESS)
        return SUCCESS;
    else
        return ERROR;
}
/*****************************************************************************
函数名称 : dp_download_external_time_open_handle
功能描述 : 针对DPID_EXTERNAL_TIME_OPEN的处理函数
输入参数 : value:数据源数据
        : length:数据长度
返回参数 : 成功返回:SUCCESS/失败返回:ERROR
使用说明 : 可下发可上报类型,需要在处理完数据后上报处理结果至app
*****************************************************************************/
static u16 dp117_mode_to_dgus(u8 dp_mode)
{
	if(dp_mode >= 1 && dp_mode <= 3)
		return dp_mode;
	return 1;
}

static unsigned char dp_download_external_time_open_handle(const unsigned char value[], unsigned short length)
{
    //示例:当前DP类型为RAW
    unsigned char ret;
	u8 i;
	unsigned char *p = &value[0];
	u16 switch1,hour,min,mode,week,temp,set_val,dgus_mode;
    // (1) 启用标志
	switch1 = p[0];
	write_dgus_vp(0x4821, (u8 *)&switch1, 1);

	// (2) 小时
	hour = p[1];
	write_dgus_vp(0x1080, (u8 *)&hour, 1);

	// (3) 分钟
	min = p[2];
	write_dgus_vp(0x1081, (u8 *)&min, 1);

	// (4) 重复模式 (BitMask 转 7个复选框)
	// 先清空目标区域，防止残留数据干扰
	// 遍历 7 天，根据 BitMask 写入 0x4500 ~ 0x4506
	week = p[3];
	for(i = 0; i < 7; i++) {
		if((week >> i) & 0x01)
			set_val = 1;
		else
			set_val = 0;
		write_dgus_vp(0x4500 + i, (u8 *)&set_val, 1);
	}

	// (5) 设定温度  (6) 模式
	temp = p[4];
	write_dgus_vp(VP_TIMER_EXT_TEMP_C, (u8 *)&temp, 1);
	{
		u16 temp_f = (u16)App_TempUnitTrans((signed short)temp, 'F');
		write_dgus_vp(VP_TIMER_EXT_TEMP_F, (u8 *)&temp_f, 1);
	}

	mode = p[5];
	dgus_mode = dp117_mode_to_dgus((u8)mode);
	write_dgus_vp(0x4826, (u8 *)&dgus_mode, 1);
	Ready_To_Save();
    
    //There should be a report after processing the DP
    ret = mcu_dp_raw_update(DPID_EXTERNAL_TIME_OPEN,value,length);
    if(ret == SUCCESS)
        return SUCCESS;
    else
        return ERROR;
}
/*****************************************************************************
函数名称 : dp_download_inside_time_close_handle
功能描述 : 针对DPID_INSIDE_TIME_CLOSE的处理函数
输入参数 : value:数据源数据
        : length:数据长度
返回参数 : 成功返回:SUCCESS/失败返回:ERROR
使用说明 : 可下发可上报类型,需要在处理完数据后上报处理结果至app
*****************************************************************************/
static unsigned char dp_download_inside_time_close_handle(const unsigned char value[], unsigned short length)
{
    //示例:当前DP类型为RAW
    unsigned char ret;
	unsigned char *p = &value[0];
	u16 switch1,hour,min;

    // (1) 启用标志
	switch1 = p[0];
	write_dgus_vp(0x4827, (u8 *)&switch1, 1);

	// (2) 小时
	hour = p[1];
	write_dgus_vp(0x1086, (u8 *)&hour, 1);

	// (3) 分钟
	min = p[2];
	write_dgus_vp(0x1087, (u8 *)&min, 1);
    //There should be a report after processing the DP
    ret = mcu_dp_raw_update(DPID_INSIDE_TIME_CLOSE,value,length);
    if(ret == SUCCESS)
        return SUCCESS;
    else
        return ERROR;
}
/*****************************************************************************
函数名称 : dp_download_external_time_close_handle
功能描述 : 针对DPID_EXTERNAL_TIME_CLOSE的处理函数
输入参数 : value:数据源数据
        : length:数据长度
返回参数 : 成功返回:SUCCESS/失败返回:ERROR
使用说明 : 可下发可上报类型,需要在处理完数据后上报处理结果至app
*****************************************************************************/
static unsigned char dp_download_external_time_close_handle(const unsigned char value[], unsigned short length)
{
    //示例:当前DP类型为RAW
    unsigned char ret;
	unsigned char *p = &value[0];
	u16 switch1,hour,min;
    // (1) 启用标志
	switch1 = p[0];
	write_dgus_vp(0x4820, (u8 *)&switch1, 1);

	// (2) 小时
	hour = p[1];
	write_dgus_vp(0x1082, (u8 *)&hour, 1);

	// (3) 分钟
	min = p[2];
	write_dgus_vp(0x1083, (u8 *)&min, 1);
	Ready_To_Save();
    
    //There should be a report after processing the DP
    ret = mcu_dp_raw_update(DPID_EXTERNAL_TIME_CLOSE,value,length);
    if(ret == SUCCESS)
        return SUCCESS;
    else
        return ERROR;
}



/******************************************************************************
                                WARNING!!!                     
此部分函数用户请勿修改!!
******************************************************************************/

/**
 * @brief  dp下发处理函数
 * @param[in] {dpid} dpid 序号
 * @param[in] {value} dp数据缓冲区地址
 * @param[in] {length} dp数据长度
 * @return dp处理结果
 * -           0(ERROR): 失败
 * -           1(SUCCESS): 成功
 * @note   该函数用户不能修改
 */
unsigned char dp_download_handle(unsigned char dpid,const unsigned char value[], unsigned short length)
{
    /*********************************
    当前函数处理可下发/可上报数据调用                    
    具体函数内需要实现下发数据处理
    完成用需要将处理结果反馈至APP端,否则APP会认为下发失败
    ***********************************/
    unsigned char ret = ERROR;
    switch(dpid) {
		case DPID_SWITCH:
            //外机电源处理函数
            ret = dp_download_switch_handle(value,length);
        break;
        case DPID_MODE:
            //外机模式处理函数
            ret = dp_download_mode_handle(value,length);
        break;
        case DPID_CHILD_LOCK:
            //童锁开关处理函数
            ret = dp_download_child_lock_handle(value,length);
        break;
        case DPID_TEMP_SET:
            //目标水温处理函数
            ret = dp_download_temp_set_handle(value,length);
        break;
        case DPID_TEMP_UNIT_CONVERT:
            //温标切换处理函数
            ret = dp_download_temp_unit_convert_handle(value,length);
        break;
        case DPID_DEFROST:
            //除霜处理函数
            ret = dp_download_defrost_handle(value,length);
        break;
        case DPID_RELAY_STATUS:
            //上电状态设置处理函数
            ret = dp_download_relay_status_handle(value,length);
        break;
        case DPID_TIMESTAMP:
            //设备时间-时间戳处理函数
            ret = dp_download_timestamp_handle(value,length);
        break;
        case DPID_INDOOR_UNIT:
            //内机处理函数
            ret = dp_download_indoor_unit_handle(value,length);
        break;
        case DPID_HISTORY_FAULT_EMPTY:
            //历史故障清空处理函数
            ret = dp_download_history_fault_empty_handle(value,length);
        break;
        case DPID_INSIDE_TIME_OPEN:
            //内机定时开机处理函数
            ret = dp_download_inside_time_open_handle(value,length);
        break;
        case DPID_EXTERNAL_TIME_OPEN:
            //外机定时开机处理函数
            ret = dp_download_external_time_open_handle(value,length);
        break;
        case DPID_INSIDE_TIME_CLOSE:
            //内机定时关机处理函数
            ret = dp_download_inside_time_close_handle(value,length);
        break;
        case DPID_EXTERNAL_TIME_CLOSE:
            //外机定时关机处理函数
            ret = dp_download_external_time_close_handle(value,length);
        break;

        
        default:
        break;
    }
    return ret;
}

/**
 * @brief  获取所有dp命令总和
 * @param[in] Null
 * @return 下发命令总和
 * @note   该函数用户不能修改
 */
unsigned char get_download_cmd_total(void)
{
    return(sizeof(download_cmd) / sizeof(download_cmd[0]));
}


/******************************************************************************
                                WARNING!!!                     
此代码为SDK内部调用,请按照实际dp数据实现函数内部数据
******************************************************************************/

#ifdef SUPPORT_MCU_FIRM_UPDATE
/**
 * @brief  升级包大小选择
 * @param[in] {package_sz} 升级包大小
 * @ref           0x00: 256byte (默认)
 * @ref           0x01: 512byte
 * @ref           0x02: 1024byte
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void upgrade_package_choose(unsigned char package_sz)
{
//    #error "请自行实现请自行实现升级包大小选择代码,完成后请删除该行"
    unsigned short send_len = 0;
    send_len = set_wifi_uart_byte(send_len, package_sz);
    wifi_uart_write_frame(UPDATE_START_CMD, MCU_TX_VER, send_len);
}

/**
 * @brief  MCU进入固件升级模式
 * @param[in] {value} 固件缓冲区
 * @param[in] {position} 当前数据包在于固件位置
 * @param[in] {length} 当前固件包长度(固件包长度为0时,表示固件包发送完成)
 * @return Null
 * @note   MCU需要自行实现该功能
 */
unsigned char OTAdataindex[256] = 0;
unsigned long	PackageNum = 0;
bit	OTADataUpdate = FALSE;
unsigned char mcu_firm_update_handle(const unsigned char value[],unsigned long position,unsigned short length)
{
//    #error "请自行完成MCU固件升级代码,完成后请删除该行"
	unsigned short i;
	static unsigned long Oldposittion = 0xffffffff;
    if(length == 0) {
        //固件数据发送完成
      
    }else {
        //固件数据处理
			PackageNum = position>>8;
      for(i=0;i<256;i++)
			{
				OTAdataindex[i] = value[i];
			}
			if(Oldposittion != position)
			{
				Oldposittion = position;
				OTADataUpdate = TRUE;
			}
    }
    
    return SUCCESS;
}
#endif

#ifdef SUPPORT_GREEN_TIME
/**
 * @brief  获取到的格林时间
 * @param[in] {time} 获取到的格林时间数据
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_greentime(unsigned char time[])
{
//    #error "请自行完成相关代码,并删除该行"
    /*
    time[0] 为是否获取时间成功标志，为 0 表示失败，为 1表示成功
    time[1] 为年份，0x00 表示 2000 年
    time[2] 为月份，从 1 开始到12 结束
    time[3] 为日期，从 1 开始到31 结束
    time[4] 为时钟，从 0 开始到23 结束
    time[5] 为分钟，从 0 开始到59 结束
    time[6] 为秒钟，从 0 开始到59 结束
    */
    if(time[0] == 1) {
        //正确接收到wifi模块返回的格林数据
        
    }else {
        //获取格林时间出错,有可能是当前wifi模块未联网
    }
}
#endif

#ifdef SUPPORT_MCU_RTC_CHECK
/**
 * @brief  MCU校对本地RTC时钟
 * @param[in] {time} 获取到的格林时间数据
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_write_rtctime(unsigned char time[])
{
//    #error "请自行完成RTC时钟写入代码,并删除该行"
    /*
    Time[0] 为是否获取时间成功标志，为 0 表示失败，为 1表示成功
    Time[1] 为年份，0x00 表示 2000 年
    Time[2] 为月份，从 1 开始到12 结束
    Time[3] 为日期，从 1 开始到31 结束
    Time[4] 为时钟，从 0 开始到23 结束
    Time[5] 为分钟，从 0 开始到59 结束
    Time[6] 为秒钟，从 0 开始到59 结束
    Time[7] 为星期，从 1 开始到 7 结束，1代表星期一
   */
    if(time[0] == 1) {
        //正确接收到wifi模块返回的本地时钟数据
     
    }else {
        //获取本地时钟数据出错,有可能是当前wifi模块未联网
    }
}
#endif

#ifdef WIFI_TEST_ENABLE
/**
 * @brief  wifi功能测试反馈
 * @param[in] {result} wifi功能测试结果
 * @ref       0: 失败
 * @ref       1: 成功
 * @param[in] {rssi} 测试成功表示wifi信号强度/测试失败表示错误类型
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void wifi_test_result(unsigned char result,unsigned char rssi)
{
//    #error "请自行实现wifi功能测试成功/失败代码,完成后请删除该行"
    if(result == 0) {
        //测试失败
        if(rssi == 0x00) {
            //未扫描到名称为tuya_mdev_test路由器,请检查
        }else if(rssi == 0x01) {
            //模块未授权
        }
    }else {
        //测试成功
        //rssi为信号强度(0-100, 0信号最差，100信号最强)
    }
}
#endif

#ifdef WEATHER_ENABLE
/**
* @brief  mcu打开天气服务
 * @param  Null
 * @return Null
 */
void mcu_open_weather(void)
{
    int i = 0;
    char buffer[13] = {0};
    unsigned char weather_len = 0;
    unsigned short send_len = 0;
    
    weather_len = sizeof(weather_choose) / sizeof(weather_choose[0]);
      
    for(i=0;i<weather_len;i++) {
        buffer[0] = sprintf(buffer+1,"w.%s",weather_choose[i]);
        send_len = set_wifi_uart_buffer(send_len, (unsigned char *)buffer, buffer[0]+1);
    }
    
//    #error "请根据提示，自行完善打开天气服务代码，完成后请删除该行"
    /*
    //当获取的参数有和时间有关的参数时(如:日出日落)，需要搭配t.unix或者t.local使用，需要获取的参数数据是按照格林时间还是本地时间
    buffer[0] = sprintf(buffer+1,"t.unix"); //格林时间   或使用  buffer[0] = sprintf(buffer+1,"t.local"); //本地时间
    send_len = set_wifi_uart_buffer(send_len, (unsigned char *)buffer, buffer[0]+1);
    */
    
    buffer[0] = sprintf(buffer+1,"w.date.%d",WEATHER_FORECAST_DAYS_NUM);
    send_len = set_wifi_uart_buffer(send_len, (unsigned char *)buffer, buffer[0]+1);
    
    wifi_uart_write_frame(WEATHER_OPEN_CMD, MCU_TX_VER, send_len);
}

/**
 * @brief  打开天气功能返回用户自处理函数
 * @param[in] {res} 打开天气功能返回结果
 * @ref       0: 失败
 * @ref       1: 成功
 * @param[in] {err} 错误码
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void weather_open_return_handle(unsigned char res, unsigned char err)
{
//    #error "请自行完成打开天气功能返回数据处理代码,完成后请删除该行"
    unsigned char err_num = 0;
    
    if(res == 1) {
        //打开天气返回成功
    }else if(res == 0) {
        //打开天气返回失败
        //获取错误码
        err_num = err; 
    }
}

/**
 * @brief  天气数据用户自处理函数
 * @param[in] {name} 参数名
 * @param[in] {type} 参数类型
 * @ref       0: int 型
 * @ref       1: string 型
 * @param[in] {data} 参数值的地址
 * @param[in] {day} 哪一天的天气  0:表示当天 取值范围: 0~6
 * @ref       0: 今天
 * @ref       1: 明天
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void weather_data_user_handle(char *name, unsigned char type, const unsigned char *data, char day)
{
//    #error "这里仅给出示例，请自行完善天气数据处理代码,完成后请删除该行"
    int value_int;
    char value_string[50];//由于有的参数内容较多，这里默认为50。您可以根据定义的参数，可以适当减少该值
    
    my_memset(value_string, '\0', 50);
    
    //首先获取数据类型
    if(type == 0) { //参数是INT型
        value_int = data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3];
    }else if(type == 1) {
        my_strcpy(value_string, data);
    }
    
    //注意要根据所选参数类型来获得参数值！！！
    if(my_strcmp(name, "temp") == 0) {
        printf("day:%d temp value is:%d\r\n", day, value_int);          //int 型
    }else if(my_strcmp(name, "humidity") == 0) {
        printf("day:%d humidity value is:%d\r\n", day, value_int);      //int 型
    }else if(my_strcmp(name, "pm25") == 0) {
        printf("day:%d pm25 value is:%d\r\n", day, value_int);          //int 型
    }else if(my_strcmp(name, "condition") == 0) {
        printf("day:%d condition value is:%s\r\n", day, value_string);  //string 型
    }
}
#endif

#ifdef MCU_DP_UPLOAD_SYN
/**
 * @brief  状态同步上报结果
 * @param[in] {result} 结果
 * @ref       0: 失败
 * @ref       1: 成功
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void get_upload_syn_result(unsigned char result)
{
//    #error "请自行完成状态同步上报结果代码,并删除该行"
      
    if(result == 0) {
        //同步上报出错
    }else {
        //同步上报成功
    }
}
#endif

#ifdef GET_WIFI_STATUS_ENABLE
/**
 * @brief  获取 WIFI 状态结果
 * @param[in] {result} 指示 WIFI 工作状态
 * @ref       0x00: wifi状态 1 smartconfig 配置状态
 * @ref       0x01: wifi状态 2 AP 配置状态
 * @ref       0x02: wifi状态 3 WIFI 已配置但未连上路由器
 * @ref       0x03: wifi状态 4 WIFI 已配置且连上路由器
 * @ref       0x04: wifi状态 5 已连上路由器且连接到云端
 * @ref       0x05: wifi状态 6 WIFI 设备处于低功耗模式
 * @ref       0x06: wifi状态 7 WIFI 设备处于smartconfig&AP配置状态
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void get_wifi_status(unsigned char result)
{
//  #error "请自行完成获取 WIFI 状态结果代码,并删除该行"
 
    switch(result) {
        case 0:
            //wifi工作状态1
        break;
    
        case 1:
            //wifi工作状态2
        break;
        
        case 2:
            //wifi工作状态3
        break;
        
        case 3:
            //wifi工作状态4
        break;
        
        case 4:
            //wifi工作状态5
        break;
        
        case 5:
            //wifi工作状态6
        break;
      
        case 6:
            //wifi工作状态7
        break;
        
        default:break;
    }
}
#endif

#ifdef WIFI_STREAM_ENABLE
/**
 * @brief  流服务发送结果
 * @param[in] {result} 结果
 * @ref       0x00: 成功
 * @ref       0x01: 流服务功能未开启
 * @ref       0x02: 流服务器未连接成功
 * @ref       0x03: 数据推送超时
 * @ref       0x04: 传输的数据长度错误
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void stream_trans_send_result(unsigned char result)
{
//    #error "这里仅给出示例，请自行完善流服务发送结果处理代码,完成后请删除该行"
    switch(result) {
        case 0x00:
            //成功
        break;
        
        case 0x01:
            //流服务功能未开启
        break;
        
        case 0x02:
            //流服务器未连接成功
        break;
        
        case 0x03:
            //数据推送超时
        break;
        
        case 0x04:
            //传输的数据长度错误
        break;
        
        default:break;
    }
}

/**
 * @brief  多地图流服务发送结果
 * @param[in] {result} 结果
 * @ref       0x00: 成功
 * @ref       0x01: 失败
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void maps_stream_trans_send_result(unsigned char result)
{
//    #error "这里仅给出示例，请自行完善多地图流服务发送结果处理代码,完成后请删除该行"
    switch(result) {
        case 0x00:
            //成功
        break;
        
        case 0x01:
            //失败
        break;
        
        default:break;
    }
}
#endif

#ifdef WIFI_CONNECT_TEST_ENABLE
/**
 * @brief  路由信息接收结果通知
 * @param[in] {result} 模块是否成功接收到正确的路由信息
 * @ref       0x00: 失败
 * @ref       0x01: 成功
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void wifi_connect_test_result(unsigned char result)
{
//    #error "请自行实现wifi功能测试成功/失败代码,完成后请删除该行"
    if(result == 0) {
        //路由信息接收失败，请检查发出的路由信息包是否是完整的JSON数据包
    }else {
        //路由信息接收成功，产测结果请注意WIFI_STATE_CMD指令的wifi工作状态
    }
}
#endif

#ifdef GET_MODULE_MAC_ENABLE
/**
 * @brief  获取模块mac结果
 * @param[in] {mac} 模块 MAC 数据
 * @ref       mac[0]: 为是否获取mac成功标志，0x00 表示成功，0x01 表示失败
 * @ref       mac[1]~mac[6]: 当获取 MAC地址标志位如果mac[0]为成功，则表示模块有效的MAC地址
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_mac(unsigned char mac[])
{
//    #error "请自行完成mac获取代码,并删除该行"
    /*
    mac[0]为是否获取mac成功标志，0x00 表示成功，为0x01表示失败
    mac[1]~mac[6]:当获取 MAC地址标志位如果mac[0]为成功，则表示模块有效的MAC地址
   */
   
    if(mac[0] == 1) {
        //获取mac出错
    }else {
        //正确接收到wifi模块返回的mac地址
    
//			write_dgus_vp((u32)(0x3543),(u8*)&mac[1],3);	//写入mac地址F

		}
}
#endif

#ifdef GET_IR_STATUS_ENABLE
/**
 * @brief  获取红外状态结果
 * @param[in] {result} 指示红外状态
 * @ref       0x00: 红外状态 1 正在发送红外码
 * @ref       0x01: 红外状态 2 发送红外码结束
 * @ref       0x02: 红外状态 3 红外学习开始
 * @ref       0x03: 红外状态 4 红外学习结束
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void get_ir_status(unsigned char result)
{
//    #error "请自行完成红外状态代码,并删除该行"
    switch(result) {
        case 0:
            //红外状态 1
        break;
      
        case 1:
            //红外状态 2
        break;
          
        case 2:
            //红外状态 3
        break;
          
        case 3:
            //红外状态 4
        break;
          
        default:break;
    }
    
    wifi_uart_write_frame(GET_IR_STATUS_CMD, MCU_TX_VER, 0);
}
#endif

#ifdef IR_TX_RX_TEST_ENABLE
/**
 * @brief  红外进入收发产测结果通知
 * @param[in] {result} 模块是否成功接收到正确的信息
 * @ref       0x00: 失败
 * @ref       0x01: 成功
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void ir_tx_rx_test_result(unsigned char result)
{
//    #error "请自行实现红外进入收发产测功能测试成功/失败代码,完成后请删除该行"
    if(result == 0) {
        //红外进入收发产测成功
    }else {
        //红外进入收发产测失败，请检查发出的数据包
    }
}
#endif

#ifdef FILE_DOWNLOAD_ENABLE
/**
 * @brief  文件下载包大小选择
 * @param[in] {package_sz} 文件下载包大小
 * @ref       0x00: 256 byte (默认)
 * @ref       0x01: 512 byte
 * @ref       0x02: 1024 byte
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void file_download_package_choose(unsigned char package_sz)
{
//    #error "请自行实现请自行实现文件下载包大小选择代码,完成后请删除该行"
    unsigned short send_len = 0;
    send_len = set_wifi_uart_byte(send_len, package_sz);
    wifi_uart_write_frame(FILE_DOWNLOAD_START_CMD, MCU_TX_VER, send_len);
}

/**
 * @brief  文件包下载模式
 * @param[in] {value} 数据缓冲区
 * @param[in] {position} 当前数据包在于文件位置
 * @param[in] {length} 当前文件包长度(长度为0时,表示文件包发送完成)
 * @return 数据处理结果
 * -           0(ERROR): 失败
 * -           1(SUCCESS): 成功
 * @note   MCU需要自行实现该功能
 */
unsigned char file_download_handle(const unsigned char value[],unsigned long position,unsigned short length)
{
//    #error "请自行完成文件包下载代码,完成后请删除该行"
    if(length == 0) {
        //文件包数据发送完成
        
    }else {
        //文件包数据处理
      
    }
    
    return SUCCESS;
}
#endif

#ifdef MODULE_EXPANDING_SERVICE_ENABLE
/**
 * @brief  打开模块时间服务通知结果
 * @param[in] {value} 数据缓冲区
 * @param[in] {length} 数据长度
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void open_module_time_serve_result(const unsigned char value[], unsigned short length)
{
//    #error "请自行实现模块时间服务通知结果代码,完成后请删除该行"
    unsigned char sub_cmd = value[0];
    
    switch(sub_cmd) {
        case 0x01: { //子命令  打开模块时间服务通知
            if(0x02 != length) {
                //数据长度错误
                return;
            }
            
            if(value[1] == 0) {
                //服务开启成功
            }else {
                //服务开启失败
            }
        }
        break;
        
        case 0x02: {  //子命令  模块时间服务通知
            if(0x09 != length) {
                //数据长度错误
                return;
            }
            
            unsigned char time_type = value[1]; //0x00:格林时间  0x01:本地时间
            unsigned char time_data[7];
            
            my_memcpy(time_data, value + 2, length - 2);
            /*
            Data[0]为年份, 0x00表示2000年
            Data[1]为月份，从1开始到12结束
            Data[2]为日期，从1开始到31结束
            Data[3]为时钟，从0开始到23结束
            Data[4]为分钟，从0开始到59结束
            Data[5]为秒钟，从0开始到15结束
            Data[6]为星期，从1开始到7结束，1代表星期一
            */
            
            //在此处添加时间数据处理代码，time_type为时间类型
            
            unsigned short send_len = 0;
            send_len = set_wifi_uart_byte(send_len,sub_cmd);
            wifi_uart_write_frame(MODULE_EXTEND_FUN_CMD, MCU_TX_VER, send_len);
        }
        break;
        
        case 0x03: {  //子命令  主动请求天气服务数据
            if(0x02 != length) {
                //数据长度错误
                return;
            }
            
            if(value[1] == 0) {
                //成功
            }else {
                //失败
            }
        }
        break;
        
        case 0x04: {  //子命令  打开模块重置状态通知
            if(0x02 != length) {
                //数据长度错误
                return;
            }
            
            if(value[1] == 0) {
                //成功
            }else {
                //失败
            }
        }
        break;
        
        case 0x05: {  //子命令  模块重置状态通知
            if(0x02 != length) {
                //数据长度错误
                return;
            }
            
            switch(value[1]) {
                case 0x00:
                    //模块本地重置
                    
                break;
                case 0x01:
                    //APP远程重置
                    
                break;
                case 0x02:
                    //APP恢复出厂重置
                    
                break;
                default:break;
            }
            
            unsigned short send_len = 0;
            send_len = set_wifi_uart_byte(send_len, sub_cmd);
            wifi_uart_write_frame(MODULE_EXTEND_FUN_CMD, MCU_TX_VER, send_len);
        }
        break;
        
        default:break;
    }
}
#endif

#ifdef BLE_RELATED_FUNCTION_ENABLE
/**
 * @brief  蓝牙功能性测试结果
 * @param[in] {value} 数据缓冲区
 * @param[in] {length} 数据长度
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void BLE_test_result(const unsigned char value[], unsigned short length)
{
//    #error "请自行实现蓝牙功能性测试结果代码,完成后请删除该行"
    unsigned char sub_cmd = value[0];
    
    if(0x03 != length) {
        //数据长度错误
        return;
    }
    
    if(0x01 != sub_cmd) {
        //子命令错误
        return;
    }
    
    unsigned char result = value[1];
    unsigned char rssi = value[2];
        
    if(result == 0) {
        //测试失败
        if(rssi == 0x00) {
            //未扫描到名称为 ty_mdev蓝牙信标,请检查
        }else if(rssi == 0x01) {
            //模块未授权
        }
    }else if(result == 0x01) {
        //测试成功
        //rssi为信号强度(0-100, 0信号最差，100信号最强)
    }
}
#endif

#ifdef VOICE_MODULE_PROTOCOL_ENABLE
/**
 * @brief  获取语音状态码结果
 * @param[in] {result} 语音状态码
 * @ref       0x00: 空闲
 * @ref       0x01: mic静音状态
 * @ref       0x02: 唤醒
 * @ref       0x03: 正在录音
 * @ref       0x04: 正在识别
 * @ref       0x05: 识别成功
 * @ref       0x06: 识别失败
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void get_voice_state_result(unsigned char result)
{
//    #error "请自行实现获取语音状态码结果处理代码,完成后请删除该行"
    switch(result) {
        case 0:
            //空闲
        break;
    
        case 1:
            //mic静音状态
        break;
        
        case 2:
            //唤醒
        break;
        
        case 3:
            //正在录音
        break;
        
        case 4:
            //正在识别
        break;
    
        case 5:
            //识别成功
        break;
        
        case 6:
            //识别失败
        break;
        
      default:break;
    }
}

/**
 * @brief  MIC静音设置结果
 * @param[in] {result} 语音状态码
 * @ref       0x00: mic 开启
 * @ref       0x01: mic 静音
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void set_voice_MIC_silence_result(unsigned char result)
{
//    #error "请自行实现MIC静音设置处理代码,完成后请删除该行"
    if(result == 0) {
        //mic 开启
    }else {
        //mic 静音
    }
}

/**
 * @brief  speaker音量设置结果
 * @param[in] {result} 音量值
 * @ref       0~10: 音量范围
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void set_speaker_voice_result(unsigned char result)
{
//    #error "请自行实现speaker音量设置结果处理代码,完成后请删除该行"
    
}

/**
 * @brief  音频产测结果
 * @param[in] {result} 音频产测状态
 * @ref       0x00: 关闭音频产测
 * @ref       0x01: mic1音频环路测试
 * @ref       0x02: mic2音频环路测试
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void voice_test_result(unsigned char result)
{
//    #error "请自行实现音频产测结果处理代码,完成后请删除该行"
    if(result == 0x00) {
        //关闭音频产测
    }else if(result == 0x01) {
        //mic1音频环路测试
    }else if(result == 0x02) {
        //mic2音频环路测试
    }
}

/**
 * @brief  唤醒产测结果
 * @param[in] {result} 唤醒返回值
 * @ref       0x00: 唤醒成功
 * @ref       0x01: 唤醒失败(10s超时失败)
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void voice_awaken_test_result(unsigned char result)
{
//    #error "请自行实现唤醒产测结果处理代码,完成后请删除该行"
    if(result == 0x00) {
        //唤醒成功
    }else if(result == 0x01) {
        //唤醒失败
    }
}

/**
 * @brief  语音模组扩展功能
 * @param[in] {value} 数据缓冲区
 * @param[in] {length} 数据长度
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void voice_module_extend_fun(const unsigned char value[], unsigned short length)
{
    unsigned char sub_cmd = value[0];
    unsigned char play;
    unsigned char bt_play;
    unsigned short send_len = 0;
  
    switch(sub_cmd) {
        case 0x00: { //子命令  MCU功能设置
            if(0x02 != length) {
                //数据长度错误
                return;
            }
            
            if(value[1] == 0) {
                //成功
            }else {
                //失败
            }
        }
        break;
        
        case 0x01: {  //子命令  状态通知
            if(0x02 > length) {
                //数据长度错误
                return;
            }
            
            unsigned char play = 0xff;
            unsigned char bt_play = 0xff;
            
            const char *str_buff = (const char *)&value[1];
            const char *str_result = NULL;
            
            str_result = strstr(str_buff,"play") + my_strlen("play") + 2;
            if(NULL == str_result) {
                //数据错误
                goto ERR_EXTI;
            }
            
            if(0 == memcmp(str_result, "true", my_strlen("true"))) {
                play = 1;
            }else if(0 == memcmp(str_result, "false", my_strlen("false"))) {
                play = 0;
            }else {
                //数据错误
                goto ERR_EXTI;
            }
            
            str_result = strstr(str_buff,"bt_play") + my_strlen("bt_play") + 2;
            if(NULL == str_result) {
                //数据错误
                goto ERR_EXTI;
            }
            
            if(0 == memcmp(str_result, "true", my_strlen("true"))) {
                bt_play = 1;
            }else if(0 == memcmp(str_result, "false", my_strlen("false"))) {
                bt_play = 0;
            }else {
                //数据错误
                goto ERR_EXTI;
            }
            
//            #error "请自行实现语音模组状态通知处理代码,完成后请删除该行"
            //MCU设置暂仅支持”播放/暂停” ”蓝牙开关”
            //play    播放/暂停功能  1(播放) / 0(暂停)
            //bt_play 蓝牙开关功能   1(开)   / 0(关)
            
            
            
            send_len = 0;
            send_len = set_wifi_uart_byte(send_len, sub_cmd);
            send_len = set_wifi_uart_byte(send_len, 0x00);
            wifi_uart_write_frame(MODULE_EXTEND_FUN_CMD, MCU_TX_VER, send_len);
        }
        break;

        default:break;
    }
    
    return;

ERR_EXTI:
    send_len = 0;
    send_len = set_wifi_uart_byte(send_len, sub_cmd);
    send_len = set_wifi_uart_byte(send_len, 0x01);
    wifi_uart_write_frame(MODULE_EXTEND_FUN_CMD, MCU_TX_VER, send_len);
    return;
}
#endif




                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       