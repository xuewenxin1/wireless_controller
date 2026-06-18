#include "upload.h"
#include "modbus.h"
#include "wifi.h"
#include "ErrorHistory.h"
#include "control.h"
#include "rtc.h"

#define UPLOAD_TIMESTAMP_TZ_SEC   (8UL * 3600UL)

extern u32 RTC_GetUnixLocal(void);

static bit s_timestamp_boot_done = 0;

static u8 Upload_UlToStr(u32 v, char *buf)
{
	char rev[10];
	u8 n = 0;
	u8 i;

	if(v == 0)
	{
		buf[0] = '0';
		buf[1] = '\0';
		return 1;
	}
	while(v && n < sizeof(rev))
	{
		rev[n++] = (char)('0' + (v % 10));
		v /= 10;
	}
	for(i = 0; i < n; i++)
		buf[i] = rev[n - 1 - i];
	buf[n] = '\0';
	return n;
}

static u8 Upload_ClampIndoorTemp(u16 v)
{
	if(v < 16)
		return 16;
	if(v > 32)
		return 32;
	return (u8)v;
}

static u8 Upload_DgusModeToDp116(u16 dgus)
{
	switch(dgus)
	{
		case 4: return 1;
		case 0: return 2;
		case 3: return 3;
		case 1: return 4;
		case 2: return 5;
		default: return 1;
	}
}

static u8 Upload_PackTimerRoomByte(void)
{
	u8 i;
	u16 sel;
	u8 count = 0;
	u8 single_room = 0;
	u8 mask = 0;

	for(i = 0; i < 6; i++)
	{
		read_dgus_vp((u32)(0x4860 + i), (u8 *)&sel, 1);
		if(sel)
		{
			count++;
			single_room = (u8)(i + 1);
			mask |= (u8)(1 << i);
		}
	}
	if(count == 1)
		return single_room;
	return mask;
}

void upload_electricity_statistics(void)
{
	u8 buf[16];
	u8 i;
	static u8 s_sent = 0;

	if(s_sent)
		return;
	s_sent = 1;
	for(i = 0; i < 16; i++)
		buf[i] = 0;
	mcu_dp_raw_update(DPID_ELECTRICITY_STATISTICS, buf, 16);
}

void upload_timestamp_from_rtc(void)
{
	char ts_buf[12];
	u32 ts_local;
	u32 ts_utc;

	ts_local = RTC_GetUnixLocal();
	if(ts_local < UPLOAD_TIMESTAMP_TZ_SEC)
		return;
	ts_utc = ts_local - UPLOAD_TIMESTAMP_TZ_SEC;
	Upload_UlToStr(ts_utc, ts_buf);
	mcu_dp_string_update(DPID_TIMESTAMP, (u8 *)ts_buf, (unsigned short)my_strlen((u8 *)ts_buf));
}

void upload_timestamp_boot_once(void)
{
	if(s_timestamp_boot_done)
		return;
	s_timestamp_boot_done = 1;
	upload_timestamp_from_rtc();
}

void upload_indoor_unit(void)
{
	u16 mode1 = 0;
	u8 i = 0;
	u8 num[36] = {0};
	static u8 s_shadow[36];
	static bit s_valid = 0;

	for(i = 0; i < 6; i++)
	{
		num[i * 6] = (u8)(i + 1);
		read_dgus_vp((u32)(0x4010 + i), (u8 *)&mode1, 1);
		num[i * 6 + 1] = (u8)mode1;
		read_dgus_vp((u32)(VP_INDOOR_ROOM_C_BASE + (u16)i * 2), (u8 *)&mode1, 1);
		if(mode1 < 16 || mode1 > 32)
			mode1 = 0;
		num[i * 6 + 2] = Upload_ClampIndoorTemp(mode1);
		read_dgus_vp((u32)(0x4700 + i), (u8 *)&mode1, 1);
		if(mode1 == 0)
			mode1 = 1;
		else if(mode1 == 1)
			mode1 = 4;
		else if(mode1 == 2)
			mode1 = 3;
		else if(mode1 == 3)
			mode1 = 2;
		else if(mode1 == 4)
			mode1 = 0;
		num[i * 6 + 3] = (u8)mode1;
		read_dgus_vp((u32)(0x4710 + i), (u8 *)&mode1, 1);
		num[i * 6 + 4] = (u8)mode1;
		read_dgus_vp((u32)(0x1020 + i), (u8 *)&mode1, 1);
		num[i * 6 + 5] = Upload_ClampIndoorTemp(mode1);
	}
	if(s_valid)
	{
		for(i = 0; i < 36; i++)
		{
			if(num[i] != s_shadow[i])
				break;
		}
		if(i >= 36)
			return;
	}
	for(i = 0; i < 36; i++)
		s_shadow[i] = num[i];
	s_valid = 1;
	mcu_dp_raw_update(DPID_INDOOR_UNIT, &num, 36);
}

void upload_inside_time_open()
{
	u8 num[8] = {0};
	u16 mode1 = 0;
	u8 i = 0;
	static u8 s_shadow[8];
	static bit s_valid = 0;

	read_dgus_vp((u32)(0x4825), (u8 *)&mode1, 1);
	num[0] = (u8)mode1;
	read_dgus_vp((u32)(0x1084), (u8 *)&mode1, 1);
	num[1] = (u8)mode1;
	read_dgus_vp((u32)(0x1085), (u8 *)&mode1, 1);
	num[2] = (u8)mode1;

	for(i = 0; i < 7; i++)
	{
		read_dgus_vp((u32)(0x4510 + i), (u8 *)&mode1, 1);
		if(mode1)
			num[3] |= (u8)(1 << i);
	}
	read_dgus_vp((u32)(0x4838), (u8 *)&mode1, 1);
	num[4] = (u8)mode1;
	read_dgus_vp((u32)(0x4837), (u8 *)&mode1, 1);
	num[5] = Upload_DgusModeToDp116(mode1);
	read_dgus_vp((u32)(0x3081), (u8 *)&mode1, 1);
	num[6] = (u8)(mode1 + 1);
	num[7] = Upload_PackTimerRoomByte();
	if(s_valid)
	{
		for(i = 0; i < 8; i++)
		{
			if(num[i] != s_shadow[i])
				break;
		}
		if(i >= 8)
			return;
	}
	for(i = 0; i < 8; i++)
		s_shadow[i] = num[i];
	s_valid = 1;
	mcu_dp_raw_update(DPID_INSIDE_TIME_OPEN, &num, 8);
}

void upload_external_time_open()
{
	u8 num[6] = {0};
	u16 mode1 = 0;
	u8 i = 0;
	static u8 s_shadow[6];
	static bit s_valid = 0;

	read_dgus_vp((u32)(0x4821),(u8 *)&mode1,1);
	num[0] = mode1;
	read_dgus_vp((u32)(0x1080),(u8 *)&mode1,1);
	num[1] = mode1;
	read_dgus_vp((u32)(0x1081),(u8 *)&mode1,1);
	num[2] = mode1;
	
	for(i=1;i<7;i++)
	{
		read_dgus_vp((u32)(0x4501+i),(u8 *)&mode1,1);
		if(mode1 == 1)
			num[3] |= 1<<(i-1);
	}
	read_dgus_vp((u32)(0x4500),(u8 *)&mode1,1);
	if(mode1 == 1)
		num[3] = 1<<6;
	read_dgus_vp((u32)(0x4824),(u8 *)&mode1,1);
		num[4] =  mode1;
	read_dgus_vp((u32)(0x4826),(u8 *)&mode1,1);
	if(mode1 == 0)
		num[5] = 2;
	else if(mode1 == 1)
		num[5] = 3;
	else if(mode1 == 2)
		num[5] = 1;
	if(s_valid)
	{
		for(i = 0; i < 6; i++)
		{
			if(num[i] != s_shadow[i])
				break;
		}
		if(i >= 6)
			return;
	}
	for(i = 0; i < 6; i++)
		s_shadow[i] = num[i];
	s_valid = 1;
	mcu_dp_raw_update(DPID_EXTERNAL_TIME_OPEN,&num,6);
}

void upload_inside_time_close()
{
	u16 mode1 = 0;
	u8 num[3] = {0};
	u8 i;
	static u8 s_shadow[3];
	static bit s_valid = 0;

	read_dgus_vp((u32)(0x4827),(u8 *)&mode1,1);
	num[0] = mode1;
	read_dgus_vp((u32)(0x1086),(u8 *)&mode1,1);
	num[1] = mode1;
	read_dgus_vp((u32)(0x1087),(u8 *)&mode1,1);
	num[2] = mode1;
	if(s_valid && s_shadow[0] == num[0] && s_shadow[1] == num[1] && s_shadow[2] == num[2])
		return;
	for(i = 0; i < 3; i++)
		s_shadow[i] = num[i];
	s_valid = 1;
	mcu_dp_raw_update(DPID_INSIDE_TIME_CLOSE,&num,3);
}

void upload_external_time_close()
{
	u16 mode1 = 0;
	u8 num[3] = {0};
	u8 i;
	static u8 s_shadow[3];
	static bit s_valid = 0;

	read_dgus_vp((u32)(0x4820),(u8 *)&mode1,1);
	num[0] = mode1;
	read_dgus_vp((u32)(0x1082),(u8 *)&mode1,1);
	num[1] = mode1;
	read_dgus_vp((u32)(0x1083),(u8 *)&mode1,1);
	num[2] = mode1;
	if(s_valid && s_shadow[0] == num[0] && s_shadow[1] == num[1] && s_shadow[2] == num[2])
		return;
	for(i = 0; i < 3; i++)
		s_shadow[i] = num[i];
	s_valid = 1;
	mcu_dp_raw_update(DPID_EXTERNAL_TIME_CLOSE,&num,3);
}

void upload_check_status(void)
{
	u8 buf[MODBUS_CHECK_STATUS_LEN];
	u8 i;
	static u8 s_shadow[MODBUS_CHECK_STATUS_LEN];
	static bit s_valid = 0;

	Modbus_GetCheckStatus(buf, MODBUS_CHECK_STATUS_LEN);
	if(s_valid)
	{
		for(i = 0; i < MODBUS_CHECK_STATUS_LEN; i++)
		{
			if(buf[i] != s_shadow[i])
				break;
		}
		if(i >= MODBUS_CHECK_STATUS_LEN)
			return;
	}
	for(i = 0; i < MODBUS_CHECK_STATUS_LEN; i++)
		s_shadow[i] = buf[i];
	s_valid = 1;
	mcu_dp_raw_update(DPID_CHECK_STATUS, buf, MODBUS_CHECK_STATUS_LEN);
}

void upload_history_faults(void)
{
	u8 buf[125];
	u8 block, rec, idx;

	for(block = 0; block < 4; block++)
	{
		for(rec = 0; rec < 125; rec++)
			buf[rec] = 0;
		for(rec = 0; rec < 25; rec++)
		{
			idx = (u8)(block * 25 + rec);
			if(idx >= ERRORHISTORYNUM || ErrorHistory[idx][4] == 0)
				break;
			buf[rec * 5] = ErrorHistory[idx][4];
			buf[rec * 5 + 1] = ErrorHistory[idx][0];
			buf[rec * 5 + 2] = ErrorHistory[idx][1];
			buf[rec * 5 + 3] = ErrorHistory[idx][2];
			buf[rec * 5 + 4] = ErrorHistory[idx][3];
		}
		if(block == 0)
			mcu_dp_raw_update(DPID_HISTORY_FAULT_1, buf, 125);
		else if(block == 1)
			mcu_dp_raw_update(DPID_HISTORY_FAULT_2, buf, 125);
		else if(block == 2)
			mcu_dp_raw_update(DPID_HISTORY_FAULT_3, buf, 125);
		else
			mcu_dp_raw_update(DPID_HISTORY_FAULT_4, buf, 125);
	}
}

void upload_all_timers(void)
{
	upload_inside_time_open();
	upload_external_time_open();
	upload_inside_time_close();
	upload_external_time_close();
}

static u8 s_report_force = 0;
static u8 s_report_valid = 0;
static u8 s_upload_pending = 0;

void upload_request_report(void)
{
	s_upload_pending = 1;
}

void upload_report_poll(void)
{
	if(!s_upload_pending)
		return;
	s_upload_pending = 0;
	upload_report_changed();
}

static u8 Upload_ModeToDp101(u8 mode)
{
	switch(mode)
	{
		case 1: return 0;
		case 2: return 1;
		case 3: return 2;
		default: return 3;
	}
}

static u8 Upload_WorkStateToDp(u8 mode)
{
	switch(mode)
	{
		case 0: return 1;
		case 1: return 3;
		case 2: return 4;
		case 3: return 2;
		case 4: return 0;
		default: return 1;
	}
}

static void Upload_BoolIfChanged(u8 dpid, u8 val, u8 *shadow)
{
	if(s_report_valid && *shadow == val)
		return;
	*shadow = val;
	mcu_dp_bool_update(dpid, val);
}

static void Upload_EnumIfChanged(u8 dpid, u8 val, u8 *shadow)
{
	if(s_report_valid && *shadow == val)
		return;
	*shadow = val;
	mcu_dp_enum_update(dpid, val);
}

static void Upload_ValueIfChanged(u8 dpid, u32 val, u32 *shadow)
{
	if(s_report_valid && *shadow == val)
		return;
	*shadow = val;
	mcu_dp_value_update(dpid, val);
}

void upload_report_force_sync(void)
{
	s_report_force = 1;
}

//void upload_report_changed(void)
//{
//	u8 update_data[2];
//	u8 unit, dp_val;
//	u8 force;
//	u32 set_temp32;
//	float ft;

//	static u8 s_switch, s_mode, s_lock, s_unit, s_defrost, s_relay, s_work;
//	static u32 s_temp_set, s_temp_cur, s_temp_top, s_effluent, s_around;
//	static u32 s_fault, s_evap, s_def_freq, s_def_out, s_def_time;

//	force = s_report_force;
//	s_report_force = 0;
//	if(force)
//	{
//		s_report_valid = 0;
//	}

//	read_dgus_vp((u32)0x4021, (u8 *)&update_data[0], 1);
//	Upload_BoolIfChanged(DPID_SWITCH, update_data[1], &s_switch);

//	read_dgus_vp((u32)0x2002, (u8 *)&update_data[0], 1);
//	dp_val = Upload_ModeToDp101(update_data[1]);
//	Upload_EnumIfChanged(DPID_MODE, dp_val, &s_mode);

//	read_dgus_vp((u32)0x2010, (u8 *)&update_data[0], 1);
//	Upload_BoolIfChanged(DPID_CHILD_LOCK, update_data[1], &s_lock);

//	read_dgus_vp((u32)0x2005, (u8 *)&update_data[0], 1);
//	set_temp32 = (u32)update_data[1] * 10UL;
//	Upload_ValueIfChanged(DPID_TEMP_SET, set_temp32, &s_temp_set);

//	read_dgus_vp((u32)0x2003, (u8 *)&update_data[0], 1);
//	unit = update_data[1];
//	Upload_EnumIfChanged(DPID_TEMP_UNIT_CONVERT, unit, &s_unit);

//	read_dgus_vp((u32)0x201b, (u8 *)&update_data[0], 1);
//	Upload_BoolIfChanged(DPID_DEFROST, update_data[1], &s_defrost);

//	if(unit)
//		read_dgus_vp((u32)0x4812, (u8 *)&ft, 2);
//	else
//		read_dgus_vp((u32)0x4802, (u8 *)&ft, 2);
//	set_temp32 = (u32)(ft * 10.0f);
//	Upload_ValueIfChanged(DPID_TEMP_CURRENT, set_temp32, &s_temp_cur);

//	read_dgus_vp((u32)0x2001, (u8 *)&update_data[0], 1);
//	dp_val = Upload_WorkStateToDp(update_data[1]);
//	Upload_EnumIfChanged(DPID_WORK_STATE, dp_val, &s_work);

//	if(unit)
//		read_dgus_vp((u32)0x4814, (u8 *)&ft, 2);
//	else
//		read_dgus_vp((u32)0x4804, (u8 *)&ft, 2);
//	set_temp32 = (u32)(ft * 10.0f);
//	Upload_ValueIfChanged(DPID_TEMP_TOP, set_temp32, &s_temp_top);

//	if(unit)
//		read_dgus_vp((u32)0x4816, (u8 *)&ft, 2);
//	else
//		read_dgus_vp((u32)0x4806, (u8 *)&ft, 2);
//	set_temp32 = (u32)(ft * 10.0f);
//	Upload_ValueIfChanged(DPID_EFFLUENT_TEMP, set_temp32, &s_effluent);

//	if(unit)
//		read_dgus_vp((u32)0x4810, (u8 *)&ft, 2);
//	else
//		read_dgus_vp((u32)0x4800, (u8 *)&ft, 2);
//	set_temp32 = (u32)(ft * 10.0f);
//	Upload_ValueIfChanged(DPID_AROUND_TEMP, set_temp32, &s_around);

//	read_dgus_vp((u32)0x3508, (u8 *)&update_data[0], 1);
//	Upload_EnumIfChanged(DPID_RELAY_STATUS, update_data[1], &s_relay);

//	read_dgus_vp((u32)0x4607, (u8 *)&update_data[0], 1);
//	set_temp32 = update_data[1];
//	Upload_ValueIfChanged(DPID_FAULT_VALUE, set_temp32, &s_fault);

//	if(unit)
//		read_dgus_vp((u32)0x1008, (u8 *)&ft, 2);
//	else
//		read_dgus_vp((u32)0x1006, (u8 *)&ft, 2);
//	set_temp32 = (u32)(ft * 10.0f);
//	Upload_ValueIfChanged(DPID_EVAPORATOR_TEMP, set_temp32, &s_evap);

//	read_dgus_vp((u32)0x3660, (u8 *)&update_data[0], 1);
//	set_temp32 = update_data[1];
//	Upload_ValueIfChanged(DPID_DEFROST_FREQUENCY, set_temp32, &s_def_freq);

//	if(unit)
//		read_dgus_vp((u32)0x101a, (u8 *)&ft, 2);
//	else
//		read_dgus_vp((u32)0x1014, (u8 *)&ft, 2);
//	set_temp32 = (u32)(ft * 10.0f);
//	Upload_ValueIfChanged(DPID_DEFROST_OUT_TEMP, set_temp32, &s_def_out);

//	read_dgus_vp((u32)0x3670, (u8 *)&update_data[0], 1);
//	set_temp32 = update_data[1];
//	Upload_ValueIfChanged(DPID_DEFROST_TIME, set_temp32, &s_def_time);

//	if(!s_report_valid || force)
//		upload_electricity_statistics();
//	s_report_valid = 1;
//}