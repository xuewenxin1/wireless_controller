#include "upload.h"
#include "protocol.h"
#include "modbus.h"
#include "wifi.h"
#include "ErrorHistory.h"
#include "control.h"
#include "rtc.h"
#include "sys.h"

/* 从屏 VP 读取，shadow 对比有差异才上报 WiFi */

#define UPLOAD_TIMESTAMP_TZ_SEC   (8UL * 3600UL)

extern u32 RTC_GetUnixLocal(void);
extern u16 RTC_GetFullYear(void);
extern u32 RTC_UnixFromLocalParts(u16 full_year, u8 month, u8 day, u8 hour, u8 minute, u8 second);

static void Upload_YieldWiFi(void);

#if 0 /* 未调用 */
static bit s_timestamp_boot_done = 0;
#endif
static u8 s_invalidate_raw = 0;
static u8 s_invalidate_scalar = 0;
static u8 s_upload_boot_ready = 0;

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

static u8 Upload_DgusIndoorModeToDp(u16 dgus)
{
	switch(dgus)
	{
		case 0: return 1;
		case 1: return 4;
		case 2: return 3;
		case 3: return 2;
		case 4: return 0;
		default: return 1;
	}
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

#if 0 /* 未调用 */
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
	Upload_YieldWiFi();
}
#endif

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

#if 0 /* 未调用 */
void upload_timestamp_boot_once(void)
{
	if(s_timestamp_boot_done)
		return;
	s_timestamp_boot_done = 1;
	upload_timestamp_from_rtc();
}
#endif

void upload_indoor_unit(void)
{
	u16 mode1 = 0;
	u8 i = 0;
	u8 num[36] = {0};
	float ftemp;
	static u8 s_shadow[36];
	static bit s_valid = 0;

	if(s_invalidate_raw)
		s_valid = 0;
	for(i = 0; i < 6; i++)
	{
		num[i * 6] = (u8)(i + 1);
		read_dgus_vp((u32)(0x4010 + i), (u8 *)&mode1, 1);
		num[i * 6 + 1] = (u8)mode1;
		read_dgus_vp((u32)(VP_INDOOR_ROOM_C_BASE + (u16)i * 2), (u8 *)&ftemp, 2);
		mode1 = (u16)ftemp;
		if(mode1 < 16 || mode1 > 32)
			mode1 = 0;
		num[i * 6 + 2] = Upload_ClampIndoorTemp(mode1);
		read_dgus_vp((u32)(0x4700 + i), (u8 *)&mode1, 1);
		num[i * 6 + 3] = Upload_DgusIndoorModeToDp(mode1);
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
	mcu_dp_raw_update(DPID_INDOOR_UNIT, num, 36);
	Upload_YieldWiFi();
}

void upload_inside_time_open()
{
	u8 num[8] = {0};
	u16 mode1 = 0;
	u8 i = 0;
	static u8 s_shadow[8];
	static bit s_valid = 0;

	if(s_invalidate_raw)
		s_valid = 0;
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
	mcu_dp_raw_update(DPID_INSIDE_TIME_OPEN, num, 8);
	Upload_YieldWiFi();
}

void upload_external_time_open()
{
	u8 num[6] = {0};
	u16 mode1 = 0;
	u8 i = 0;
	static u8 s_shadow[6];
	static bit s_valid = 0;

	if(s_invalidate_raw)
		s_valid = 0;
	read_dgus_vp((u32)(0x4821),(u8 *)&mode1,1);
	num[0] = (u8)mode1;
	read_dgus_vp((u32)(0x1080),(u8 *)&mode1,1);
	num[1] = (u8)mode1;
	read_dgus_vp((u32)(0x1081),(u8 *)&mode1,1);
	num[2] = (u8)mode1;

	for(i = 0; i < 7; i++)
	{
		read_dgus_vp((u32)(0x4500 + i), (u8 *)&mode1, 1);
		if(mode1)
			num[3] |= (u8)(1 << i);
	}
	read_dgus_vp((u32)(0x4824), (u8 *)&mode1, 1);
	num[4] = (u8)mode1;
	read_dgus_vp((u32)(0x4826), (u8 *)&mode1, 1);
	num[5] = (u8)mode1;
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
	mcu_dp_raw_update(DPID_EXTERNAL_TIME_OPEN, num, 6);
	Upload_YieldWiFi();
}

void upload_inside_time_close()
{
	u16 mode1 = 0;
	u8 num[3] = {0};
	u8 i;
	static u8 s_shadow[3];
	static bit s_valid = 0;

	if(s_invalidate_raw)
		s_valid = 0;
	read_dgus_vp((u32)(0x4827),(u8 *)&mode1,1);
	num[0] = (u8)mode1;
	read_dgus_vp((u32)(0x1086),(u8 *)&mode1,1);
	num[1] = (u8)mode1;
	read_dgus_vp((u32)(0x1087),(u8 *)&mode1,1);
	num[2] = (u8)mode1;
	if(s_valid && s_shadow[0] == num[0] && s_shadow[1] == num[1] && s_shadow[2] == num[2])
		return;
	for(i = 0; i < 3; i++)
		s_shadow[i] = num[i];
	s_valid = 1;
	mcu_dp_raw_update(DPID_INSIDE_TIME_CLOSE, num, 3);
	Upload_YieldWiFi();
}

void upload_external_time_close()
{
	u16 mode1 = 0;
	u8 num[3] = {0};
	u8 i;
	static u8 s_shadow[3];
	static bit s_valid = 0;

	if(s_invalidate_raw)
		s_valid = 0;
	read_dgus_vp((u32)(0x4820),(u8 *)&mode1,1);
	num[0] = (u8)mode1;
	read_dgus_vp((u32)(0x1082),(u8 *)&mode1,1);
	num[1] = (u8)mode1;
	read_dgus_vp((u32)(0x1083),(u8 *)&mode1,1);
	num[2] = (u8)mode1;
	if(s_valid && s_shadow[0] == num[0] && s_shadow[1] == num[1] && s_shadow[2] == num[2])
		return;
	for(i = 0; i < 3; i++)
		s_shadow[i] = num[i];
	s_valid = 1;
	mcu_dp_raw_update(DPID_EXTERNAL_TIME_CLOSE, num, 3);
	Upload_YieldWiFi();
}

void upload_check_status(void)
{
	u8 buf[MODBUS_CHECK_STATUS_LEN];
	u8 i;
	static u8 s_shadow[MODBUS_CHECK_STATUS_LEN];
	static bit s_valid = 0;

	if(s_invalidate_raw)
		s_valid = 0;
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
	Upload_YieldWiFi();
}

static void Upload_PackHistoryFaultRec(u8 *out5, u8 *rec)
{
	u32 ts;

	out5[0] = rec[4];
	if(rec[4] == 0)
	{
		out5[1] = 0;
		out5[2] = 0;
		out5[3] = 0;
		out5[4] = 0;
		return;
	}
	ts = RTC_UnixFromLocalParts(RTC_GetFullYear(), rec[0], rec[1], rec[2], rec[3], 0);
	if(ts >= UPLOAD_TIMESTAMP_TZ_SEC)
		ts -= UPLOAD_TIMESTAMP_TZ_SEC;
	out5[1] = (u8)ts;
	out5[2] = (u8)(ts >> 8);
	out5[3] = (u8)(ts >> 16);
	out5[4] = (u8)(ts >> 24);
}

static void Upload_PackHistoryFaultBlock(u8 *buf, u8 block)
{
	u8 rec;
	u8 idx;

	for(rec = 0; rec < 125; rec++)
		buf[rec] = 0;
	for(rec = 0; rec < 25; rec++)
	{
		idx = (u8)(block * 25 + rec);
		if(idx >= ERRORHISTORYNUM || ErrorHistory[idx][4] == 0)
			break;
		Upload_PackHistoryFaultRec(buf + rec * 5, ErrorHistory[idx]);
	}
}

void upload_history_faults(void)
{
	u8 buf[125];
	u8 block;
	u8 i;
	static u8 s_shadow[4][125];
	static u8 s_valid[4] = {0, 0, 0, 0};

	if(s_invalidate_raw)
	{
		s_valid[0] = 0;
		s_valid[1] = 0;
		s_valid[2] = 0;
		s_valid[3] = 0;
	}
	for(block = 0; block < 4; block++)
	{
		Upload_PackHistoryFaultBlock(buf, block);
		if(s_valid[block])
		{
			for(i = 0; i < 125; i++)
			{
				if(buf[i] != s_shadow[block][i])
					break;
			}
			if(i >= 125)
				continue;
		}
		for(i = 0; i < 125; i++)
			s_shadow[block][i] = buf[i];
		s_valid[block] = 1;
		mcu_dp_raw_update((u8)(DPID_HISTORY_FAULT_1 + block), buf, 125);
		Upload_YieldWiFi();
	}
}

#if 0 /* 未调用 */
void upload_all_timers(void)
{
	/* 禁止组合上报，各定时 DP 在 control.c 变更处单独上报 */
}
#endif

static u8 s_dp_switch = 0;
static u8 s_dp_mode = 0;
static u8 s_lock, s_unit, s_defrost, s_relay, s_work;
static u32 s_temp_set, s_temp_cur, s_temp_top, s_effluent, s_around;
static u32 s_fault, s_evap, s_def_freq, s_def_out, s_def_time;

static void Upload_YieldWiFi(void)
{
	WDT_RST();
	wifi_uart_rx_pull();
	if(s_upload_boot_ready)
		wifi_uart_request_download_parse();
}

void upload_shadow_bool(u8 dpid, u8 val)
{
	switch(dpid)
	{
	case DPID_SWITCH: s_dp_switch = val; break;
	case DPID_CHILD_LOCK: s_lock = val; break;
	case DPID_DEFROST: s_defrost = val; break;
	default: break;
	}
}

void upload_shadow_enum(u8 dpid, u8 val)
{
	switch(dpid)
	{
	case DPID_MODE: s_dp_mode = val; break;
	case DPID_TEMP_UNIT_CONVERT: s_unit = val; break;
	case DPID_WORK_STATE: s_work = val; break;
	case DPID_RELAY_STATUS: s_relay = val; break;
	default: break;
	}
}

void upload_shadow_value(u8 dpid, u32 val)
{
	switch(dpid)
	{
	case DPID_TEMP_SET: s_temp_set = val; break;
	case DPID_TEMP_CURRENT: s_temp_cur = val; break;
	case DPID_TEMP_TOP: s_temp_top = val; break;
	case DPID_EFFLUENT_TEMP: s_effluent = val; break;
	case DPID_AROUND_TEMP: s_around = val; break;
	case DPID_FAULT_VALUE: s_fault = val; break;
	case DPID_EVAPORATOR_TEMP: s_evap = val; break;
	case DPID_DEFROST_FREQUENCY: s_def_freq = val; break;
	case DPID_DEFROST_OUT_TEMP: s_def_out = val; break;
	case DPID_DEFROST_TIME: s_def_time = val; break;
	default: break;
	}
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
	if(!s_invalidate_scalar && *shadow == val)
		return;
	*shadow = val;
	mcu_dp_bool_update(dpid, val);
	Upload_YieldWiFi();
}

static void Upload_EnumIfChanged(u8 dpid, u8 val, u8 *shadow)
{
	if(!s_invalidate_scalar && *shadow == val)
		return;
	*shadow = val;
	mcu_dp_enum_update(dpid, val);
	Upload_YieldWiFi();
}

static void Upload_ValueIfChanged(u8 dpid, u32 val, u32 *shadow)
{
	if(!s_invalidate_scalar && *shadow == val)
		return;
	*shadow = val;
	mcu_dp_value_update(dpid, val);
	Upload_YieldWiFi();
}

static u8 Upload_GetUnit(void)
{
	u16 unit = 0;
	read_dgus_vp((u32)0x2003, (u8 *)&unit, 1);
	return (u8)unit;
}

void upload_dp_switch(void)
{
	u16 vp_u16;
	read_dgus_vp((u32)0x4021, (u8 *)&vp_u16, 1);
	Upload_BoolIfChanged(DPID_SWITCH, (u8)vp_u16, &s_dp_switch);
}

void upload_dp_mode(void)
{
	u16 vp_u16;
	u8 dp_val;
	read_dgus_vp((u32)0x2002, (u8 *)&vp_u16, 1);
	dp_val = Upload_ModeToDp101((u8)vp_u16);
	Upload_EnumIfChanged(DPID_MODE, dp_val, &s_dp_mode);
}

void upload_dp_temp_set(void)
{
	u16 vp_u16;
	read_dgus_vp((u32)0x2005, (u8 *)&vp_u16, 1);
	Upload_ValueIfChanged(DPID_TEMP_SET, (u32)vp_u16, &s_temp_set);
}

void upload_dp_temp_unit(void)
{
	u16 vp_u16;
	read_dgus_vp((u32)0x2003, (u8 *)&vp_u16, 1);
	Upload_EnumIfChanged(DPID_TEMP_UNIT_CONVERT, (u8)vp_u16, &s_unit);
}

void upload_dp_work_state(void)
{
	u16 vp_u16;
	u8 dp_val;
	read_dgus_vp((u32)0x2001, (u8 *)&vp_u16, 1);
	dp_val = Upload_WorkStateToDp((u8)vp_u16);
	Upload_EnumIfChanged(DPID_WORK_STATE, dp_val, &s_work);
}

void upload_dp_child_lock(void)
{
	u16 vp_u16;
	read_dgus_vp((u32)0x2010, (u8 *)&vp_u16, 1);
	Upload_BoolIfChanged(DPID_CHILD_LOCK, (u8)vp_u16, &s_lock);
}

void upload_dp_defrost(void)
{
	u16 vp_u16;
	read_dgus_vp((u32)0x201b, (u8 *)&vp_u16, 1);
	Upload_BoolIfChanged(DPID_DEFROST, (u8)vp_u16, &s_defrost);
}

void upload_dp_relay_status(void)
{
	u16 vp_u16;
	read_dgus_vp((u32)0x3508, (u8 *)&vp_u16, 1);
	Upload_EnumIfChanged(DPID_RELAY_STATUS, (u8)vp_u16, &s_relay);
}

void upload_dp_fault(void)
{
	u16 vp_u16;
	read_dgus_vp((u32)0x4607, (u8 *)&vp_u16, 1);
	Upload_ValueIfChanged(DPID_FAULT_VALUE, (u32)vp_u16, &s_fault);
}

void upload_dp_temp_current(void)
{
	u8 unit = Upload_GetUnit();
	float ft;
	u32 set_temp32;
	if(unit)
		read_dgus_vp((u32)0x4812, (u8 *)&ft, 2);
	else
		read_dgus_vp((u32)0x4802, (u8 *)&ft, 2);
	set_temp32 = (u32)(ft * 10.0f);
	Upload_ValueIfChanged(DPID_TEMP_CURRENT, set_temp32, &s_temp_cur);
}

void upload_dp_temp_top(void)
{
	u8 unit = Upload_GetUnit();
	float ft;
	u32 set_temp32;
	if(unit)
		read_dgus_vp((u32)0x4814, (u8 *)&ft, 2);
	else
		read_dgus_vp((u32)0x4804, (u8 *)&ft, 2);
	set_temp32 = (u32)(ft * 10.0f);
	Upload_ValueIfChanged(DPID_TEMP_TOP, set_temp32, &s_temp_top);
}

void upload_dp_effluent_temp(void)
{
	u8 unit = Upload_GetUnit();
	float ft;
	u32 set_temp32;
	if(unit)
		read_dgus_vp((u32)0x4816, (u8 *)&ft, 2);
	else
		read_dgus_vp((u32)0x4806, (u8 *)&ft, 2);
	set_temp32 = (u32)(ft * 10.0f);
	Upload_ValueIfChanged(DPID_EFFLUENT_TEMP, set_temp32, &s_effluent);
}

void upload_dp_around_temp(void)
{
	u8 unit = Upload_GetUnit();
	float ft;
	u32 set_temp32;
	if(unit)
		read_dgus_vp((u32)0x4810, (u8 *)&ft, 2);
	else
		read_dgus_vp((u32)0x4800, (u8 *)&ft, 2);
	set_temp32 = (u32)(ft * 10.0f);
	Upload_ValueIfChanged(DPID_AROUND_TEMP, set_temp32, &s_around);
}

void upload_dp_evaporator_temp(void)
{
	u8 unit = Upload_GetUnit();
	float ft;
	u32 set_temp32;
	if(unit)
		read_dgus_vp((u32)0x1008, (u8 *)&ft, 2);
	else
		read_dgus_vp((u32)0x1006, (u8 *)&ft, 2);
	set_temp32 = (u32)(ft * 10.0f);
	Upload_ValueIfChanged(DPID_EVAPORATOR_TEMP, set_temp32, &s_evap);
}

void upload_dp_defrost_freq(void)
{
	u16 vp_u16;
	read_dgus_vp((u32)0x3660, (u8 *)&vp_u16, 1);
	Upload_ValueIfChanged(DPID_DEFROST_FREQUENCY, (u32)vp_u16, &s_def_freq);
}

void upload_dp_defrost_out_temp(void)
{
	u8 unit = Upload_GetUnit();
	float ft;
	u32 set_temp32;
	if(unit)
		read_dgus_vp((u32)0x101a, (u8 *)&ft, 2);
	else
		read_dgus_vp((u32)0x1014, (u8 *)&ft, 2);
	set_temp32 = (u32)(ft * 10.0f);
	Upload_ValueIfChanged(DPID_DEFROST_OUT_TEMP, set_temp32, &s_def_out);
}

void upload_dp_defrost_time(void)
{
	u16 vp_u16;
	read_dgus_vp((u32)0x3670, (u8 *)&vp_u16, 1);
	Upload_ValueIfChanged(DPID_DEFROST_TIME, (u32)vp_u16, &s_def_time);
}

void upload_set_boot_ready(u8 ready)
{
	s_upload_boot_ready = ready ? 1 : 0;
	if(s_upload_boot_ready)
	{
		upload_history_faults();
		upload_check_status();
		upload_external_time_open();
		upload_external_time_close();
		upload_inside_time_open();
		upload_inside_time_close();
	}
}

u8 upload_is_boot_ready(void)
{
	return s_upload_boot_ready;
}

void upload_state_query_reply(void)
{
	/* cmd=8 每次只应答 1 个 DP，禁止组合上报 */
	static u8 s_cmd8_idx = 0;

	s_invalidate_scalar = 1;
	switch(s_cmd8_idx)
	{
	case 0: upload_dp_switch(); break;
	case 1: upload_dp_mode(); break;
	case 2: upload_dp_temp_set(); break;
	case 3: upload_dp_temp_unit(); break;
	case 4: upload_dp_child_lock(); break;
	case 5: upload_dp_defrost(); break;
	case 6: upload_dp_relay_status(); break;
	case 7: upload_dp_work_state(); break;
	case 8: upload_check_status(); break;
	case 9: upload_history_faults(); break;
	case 10: upload_dp_fault(); break;
	default: upload_dp_temp_current(); break;
	}
	s_invalidate_scalar = 0;
	s_cmd8_idx++;
	if(s_cmd8_idx > 11)
		s_cmd8_idx = 0;
}

#if 0 /* 未调用 */
void upload_modbus_poll_report(void)
{
	/* 已改为 Modbus 解析处单条上报，此处不再批量扫描 */
}
#endif
