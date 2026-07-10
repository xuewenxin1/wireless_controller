/******************************************************************************

                  版权所有 (C), 2019, 北京迪文科技有限公司

 ******************************************************************************
  文 件 名   : control.c
  版 本 号   : V2.0
  作    者   : chengjing
  生成日期   : 2019年5月14日
  功能描述   : 串口函数
  修改历史   :
  1.日    期   : 
    作    者   : 
    修改内容   : 
******************************************************************************/

#include "control.h"
#include "unused_suppress.h"
#include "app_core.h"
#include "app_share.h"
#include "sync_link.h"
#include "stdlib.h"
#include "stdio.h"
#include "uart.h"
#include "sys.h"
#include "modbus.h"
#include "rtc.h"
#include "modbus.h"
#include "upload.h"
#include "wifi.h"
#include "ErrorHistory.h"
#include "upload.h"

extern u32 RTC_GetUnixLocal(void);
void Temp_Limit(unsigned short Addr, unsigned char TempType);
void Control_SanitizeSetTempIntVp(void);
void HomePage_UpdateModeAnimation(u8 mode);
void SyncSetTempCachesFromC(u16 temp_c);
extern void App_ModbusClearManualDefrost(void);
static void TempUnit_RefreshActiveSetFromMode(void);

/* 动画图标描述指针 SP=0x8000，SP+6/7/8 对应 ICON_Stop/Start/End（迪文协议） */
#define HOMEPAGE_ANIM_SP_BASE		0x8000
#define HOMEPAGE_ICON_STOP_VP		0x8006
#define HOMEPAGE_ICON_START_VP		0x8007
#define HOMEPAGE_ICON_END_VP		0x8008
#define HOMEPAGE_ANIM_CTRL_VP		0x4060
#define HOMEPAGE_ANIM_STOP		1
#define HOMEPAGE_ANIM_START		0

#define VP_FLOAT_C_BASE		0x4800U
#define VP_FLOAT_F_BASE		0x4810U

static u16 s_ext_settemp_c[4];

u32 SetTempIntVpByMode(u8 mode, u8 unit_f)
{
	if(mode < 1 || mode > 3)
		return 0;
	if(unit_f)
	{
		if(mode == 1)
			return VP_EXT_SETTEMP_FLOOR_F;
		if(mode == 2)
			return VP_EXT_SETTEMP_DHW_F;
		return VP_EXT_SETTEMP_POOL_F;
	}
	if(mode == 1)
		return VP_EXT_SETTEMP_FLOOR_C;
	if(mode == 2)
		return VP_EXT_SETTEMP_DHW_C;
	return VP_EXT_SETTEMP_POOL_C;
}

u32 Control_ExtSetTempDisplayVpByMode(u8 mode, u8 unit_f)
{
	return SetTempIntVpByMode(mode, unit_f);
}

void Control_MirrorExtSetTempDisplay(u8 mode)
{
	u16 temp_c;
	u16 temp_f;
	u32 disp_vp;

	if(mode < 1 || mode > 3)
		return;
	temp_c = SetTempIntRead(mode, 0);
	temp_f = SetTempIntRead(mode, 1);
	if(!temp_f && temp_c)
		temp_f = (u16)TempUnitTrans((signed short)temp_c, 'F');
	disp_vp = Control_ExtSetTempDisplayVpByMode(mode, 0);
	if(disp_vp)
		write_dgus_vp(disp_vp, (u8 *)&temp_c, 1);
	disp_vp = Control_ExtSetTempDisplayVpByMode(mode, 1);
	if(disp_vp && disp_vp != (u32)VP_TIMER_IN_OFF_EN_UI)
		write_dgus_vp(disp_vp, (u8 *)&temp_f, 1);
}

static u8 Control_ExtSetTempModeFromAddr(u16 addr, u8 *unit_f)
{
	u8 m;

	for(m = 1; m <= 3; m++)
	{
		if(addr == (u16)SetTempIntVpByMode(m, 0))
		{
			if(unit_f)
				*unit_f = 0;
			return m;
		}
		if(addr == (u16)SetTempIntVpByMode(m, 1))
		{
			if(unit_f)
				*unit_f = 1;
			return m;
		}
	}
	return 0;
}

#define SET_TEMP_MODE1_C_LO	30
#define SET_TEMP_MODE1_C_HI	60
#define SET_TEMP_MODE2_C_LO	30
#define SET_TEMP_MODE2_C_HI	50
#define SET_TEMP_MODE3_C_LO	15
#define SET_TEMP_MODE3_C_HI	40

static u16 s_lock_key_vp = 0;

static void Control_GetSetTempCLimit(u8 mode, u16 *lo, u16 *hi)
{
	switch(mode)
	{
		case 1:
			*lo = SET_TEMP_MODE1_C_LO;
			*hi = SET_TEMP_MODE1_C_HI;
			break;
		case 2:
			*lo = SET_TEMP_MODE2_C_LO;
			*hi = SET_TEMP_MODE2_C_HI;
			break;
		case 3:
			*lo = SET_TEMP_MODE3_C_LO;
			*hi = SET_TEMP_MODE3_C_HI;
			break;
		default:
			*lo = SET_TEMP_MODE1_C_LO;
			*hi = SET_TEMP_MODE1_C_HI;
			break;
	}
}

static u16 Control_ResolveModeSetTempC(u8 mode, u8 unit)
{
	u16 temp_c;

	temp_c = SetTempIntRead(mode, unit);
	if(unit && temp_c)
		temp_c = (u16)TempUnitTrans((signed short)temp_c, 'C');
	return Control_ClampSetTempCByMode(mode, temp_c);
}

u16 Control_ClampSetTempCByMode(u8 mode, u16 temp_c)
{
	u16 lo, hi;

	Control_GetSetTempCLimit(mode, &lo, &hi);
	if(temp_c < lo || temp_c > hi)
		return lo;
	return temp_c;
}

static void Control_ApplyChildLock(u16 locked)
{
	/* 0x2010=童锁状态；0x2030 由 HomePage_Icon 同步显示 */
	write_dgus_vp((u32)(0x2010), (u8 *)&locked, 1);
	if(!locked)
		HomePage(TRUE);
	HomePage_Icon();
}

static void Control_ClampModeSetTempVp(u8 mode)
{
	u16 temp_c;
	u16 temp_f;

	if(mode < 1 || mode > 3)
		return;
	temp_c = SetTempIntRead(mode, 0);
	temp_c = Control_ClampSetTempCByMode(mode, temp_c);
	temp_f = (u16)TempUnitTrans((signed short)temp_c, 'F');
	SetTempIntWrite(mode, 0, temp_c);
	SetTempIntWrite(mode, 1, temp_f);
}

void Control_LockKeyService(void)
{
	static bit s_lock_done;

	if(LockKeyCountEnalbe)
	{
		u16 lock_st;
		u16 one = 1;

		read_dgus_vp((u32)(0x2010), (u8 *)&lock_st, 1);
		if(lock_st == 1)
		{
			/* 按压期间 DGUS 会把 2030 写 0；只刷新 2010，勿写 2030（会误触取消长按） */
			write_dgus_vp((u32)(0x2010), (u8 *)&one, 1);
		}
		if(LockKeyCount >= 2000 && !s_lock_done)
		{
			u16 lock_st;
			u16 zero = 0;

			s_lock_done = 1;
			read_dgus_vp((u32)(0x2010), (u8 *)&lock_st, 1);
			lock_st = lock_st ? 0 : 1; /* 0解锁 1锁屏 */
			Control_ApplyChildLock(lock_st);
			Ready_To_Save_Report();
			App_UploadChildLock();
			LockKeyCountEnalbe = FALSE;
			LockKeyCount = 0;
			if(s_lock_key_vp >= 0x2030 && s_lock_key_vp <= 0x2032)
				write_dgus_vp((u32)s_lock_key_vp, (u8 *)&zero, 1);
			s_lock_key_vp = 0;
		}
	}
	else
		s_lock_done = 0;
}

u16	Defrosting = FALSE;
static u32 s_defrost_start_sec = 0;

void Control_ClearDefrostSession(void)
{
	s_defrost_start_sec = 0;
}

static void PrefillSetTempEditPage(void)
{
	u16 mode;
	u16 unit;
	u16 temp_c;
	u16 temp_f;

	read_dgus_vp((u32)(0x2002), (u8 *)&mode, 1);
	if(mode < 1 || mode > 3)
		mode = 1;
	temp_c = SetTempIntRead((u8)mode, 0);
	temp_f = SetTempIntRead((u8)mode, 1);
	temp_c = Control_ClampSetTempCByMode((u8)mode, temp_c);
	if(!temp_f && temp_c)
		temp_f = (u16)TempUnitTrans((signed short)temp_c, 'F');
	if(!temp_c && temp_f)
		temp_c = (u16)TempUnitTrans((signed short)temp_f, 'C');
	temp_c = Control_ClampSetTempCByMode((u8)mode, temp_c);
	temp_f = (u16)TempUnitTrans((signed short)temp_c, 'F');
	SetTempIntWrite((u8)mode, 0, temp_c);
	SetTempIntWrite((u8)mode, 1, temp_f);
	write_dgus_vp((u32)(0x2025), (u8 *)&temp_c, 1);
	write_dgus_vp((u32)(0x2026), (u8 *)&temp_f, 1);
	read_dgus_vp((u32)(0x2003), (u8 *)&unit, 1);
	if(!unit)
	{
		Page_Change_Handler(8);
		write_dgus_vp((u32)(0x2005), (u8 *)&temp_c, 1);
	}
	else
	{
		Page_Change_Handler(43);
		write_dgus_vp((u32)(0x2006), (u8 *)&temp_f, 1);
	}
}

static void Defrost_CheckExit(void)
{
	u16 defrost_en;
	u16 duration_min;
	float evap_c;
	float t3_c;

	if(!Defrosting)
		return;

	read_dgus_vp((u32)(0x201b), (u8 *)&defrost_en, 1);
	if(!defrost_en)
	{
		Defrosting = FALSE;
		return;
	}

	read_dgus_vp((u32)(0x3670), (u8 *)&duration_min, 1);
	if(!duration_min)
		read_dgus_vp((u32)(0x1012), (u8 *)&duration_min, 1);
	if(duration_min > 0 && s_defrost_start_sec > 0)
	{
		if(RTC_GetUnixLocal() - s_defrost_start_sec >= (u32)duration_min * 60UL)
			goto defrost_exit;
	}

	read_dgus_vp((u32)(0x1006), (u8 *)&evap_c, 2);
	read_dgus_vp((u32)(0x1014), (u8 *)&t3_c, 2);
	if(s_defrost_start_sec > 0 && RTC_GetUnixLocal() - s_defrost_start_sec < 60UL)
		return;
	if(evap_c > t3_c)
		goto defrost_exit;
	return;

defrost_exit:
	{
		u16 zero = 0;

		Defrosting = FALSE;
		s_defrost_start_sec = 0;
		App_ModbusClearManualDefrost();
		write_dgus_vp((u32)(0x201b), (u8 *)&zero, 1);
		App_ModbusTriggerUserWrite();
		App_UploadDefrost();
	}
}

u16 SetTempIntRead(u8 mode, u8 unit_f)
{
	u16 temp = 0;
	u32 vp = SetTempIntVpByMode(mode, unit_f);

	if(vp)
		read_dgus_vp(vp, (u8 *)&temp, 1);
	if(!unit_f && mode >= 1 && mode <= 3 && temp > 2)
		s_ext_settemp_c[mode] = temp;
	return temp;
}

void SetTempIntWrite(u8 mode, u8 unit_f, u16 temp)
{
	u32 vp = SetTempIntVpByMode(mode, unit_f);

	if(vp)
		write_dgus_vp(vp, (u8 *)&temp, 1);
	if(!unit_f && mode >= 1 && mode <= 3)
		s_ext_settemp_c[mode] = temp;
}

void SetTempFloatWriteC(u8 mode, float temp_c)
{
	u16 temp_f;
	u16 ic;

	ic = (u16)temp_c;
	temp_f = (u16)TempUnitTrans((signed short)ic, 'F');
	SetTempIntWrite(mode, 0, ic);
	SetTempIntWrite(mode, 1, temp_f);
}

static void TempUnit_RefreshFloatPair(u32 vp_c, u32 vp_f)
{
	float fc;
	float ff;

	read_dgus_vp(vp_c, (u8 *)&fc, 2);
	ff = (float)TempUnitTrans((signed short)fc, 'F');
	write_dgus_vp(vp_f, (u8 *)&ff, 2);
}

static void SyncIndoorSetTempVpPair(u8 index, u16 temp_c)
{
	u16 temp_f;

	if(index >= 6)
		return;
	temp_f = (u16)TempUnitTrans((signed short)temp_c, 'F');
	write_dgus_vp((u32)(0x1020 + index), (u8 *)&temp_c, 1);
	write_dgus_vp((u32)(VP_INDOOR_SET_F_BASE + index), (u8 *)&temp_f, 1);
}

static void SyncTimerTempVpPair(u32 vp_c, u32 vp_f, u16 temp_c)
{
	u16 temp_f;

	temp_f = (u16)TempUnitTrans((signed short)temp_c, 'F');
	write_dgus_vp(vp_c, (u8 *)&temp_c, 1);
	write_dgus_vp(vp_f, (u8 *)&temp_f, 1);
}

static u16 ReadTimerTempC(u32 vp_c, u32 vp_f)
{
	u16 unit;
	u16 temp_c;
	u16 temp_f;

	read_dgus_vp((u32)0x2003, (u8 *)&unit, 1);
	if(unit)
	{
		read_dgus_vp(vp_f, (u8 *)&temp_f, 1);
		return (u16)TempUnitTrans((signed short)temp_f, 'C');
	}
	read_dgus_vp(vp_c, (u8 *)&temp_c, 1);
	return temp_c;
}

/* 屏幕已写入 4850/4852/4854，单片机只读并触发 Modbus 写 */
static void ApplyExtSetTempFromDisplay(u8 mode, u16 temp_c)
{
	u16 temp_f;
	u16 active_mode;

	if(mode < 1 || mode > 3 || temp_c < 10)
		return;
	s_ext_settemp_c[mode] = temp_c;
	read_dgus_vp((u32)0x2002, (u8 *)&active_mode, 1);
	if(active_mode == mode)
	{
		temp_f = (u16)TempUnitTrans((signed short)temp_c, 'F');
		write_dgus_vp((u32)0x2005, (u8 *)&temp_c, 1);
		write_dgus_vp((u32)0x2006, (u8 *)&temp_f, 1);
	}
	Modbus_MarkSetTempDirty();
	Ready_To_Save_Report();
	App_UploadTempSet();
}

void HomePage_UpdateModeAnimation(u8 mode)
{
	u16 icon_start;
	u16 icon_end;
	u16 anim_val;

	if(mode > 4)
		mode = 4;
	switch(mode)
	{
		case 0:
			icon_start = 50;
			icon_end = 66;
		break;
		case 1:
			icon_start = 70;
			icon_end = 86;
		break;
		case 2:
			icon_start = 20;
			icon_end = 36;
		break;
		case 3:
			icon_start = 1;
			icon_end = 17;
		break;
		default:
			icon_start = 40;
			icon_end = 49;
		break;
	}
	/* 8006=停止图标 8007=起始帧 8008=结束帧 */
	write_dgus_vp((u32)HOMEPAGE_ICON_STOP_VP, (u8 *)&icon_end, 1);
	write_dgus_vp((u32)HOMEPAGE_ICON_START_VP, (u8 *)&icon_start, 1);
	write_dgus_vp((u32)HOMEPAGE_ICON_END_VP, (u8 *)&icon_end, 1);
	/* 控制 VP 写 V_Stop 再写 V_Start，重新启动动画 */
	anim_val = HOMEPAGE_ANIM_STOP;
	write_dgus_vp((u32)HOMEPAGE_ANIM_CTRL_VP, (u8 *)&anim_val, 1);
	anim_val = HOMEPAGE_ANIM_START;
	write_dgus_vp((u32)HOMEPAGE_ANIM_CTRL_VP, (u8 *)&anim_val, 1);
}

static void SyncHomeDisplayIfFirstIndoorModeChanged(u8 unit_idx)
{
	(void)unit_idx;
	HomePage_SyncIndoorDisplay();
}

#define WEEK_MON    (1<<0)
#define WEEK_TUE    (1<<1)
#define WEEK_WED    (1<<2)
#define WEEK_THU    (1<<3)
#define WEEK_FRI    (1<<4)
#define WEEK_SAT    (1<<5)
#define WEEK_SUN    (1<<6)

u16 APP_down_delay = 0;	

bit	LockKeyCountEnalbe;
u16	LockKeyCount = 0;
#define	HOMEPAGENUM 8
u8	HomePageID[HOMEPAGENUM] = {1, 30, 5, 6, 7, 10, 11, 12};
bit	TimerEnable = FALSE;
EXTERN_TIMER_T ExternTimer;
EXTERN_TIMER_T1 ExternTimer1;
//u8	TimerSet[4] = 0;//定时开时、定时开分、定时关时、定时关分F
u16	Language_Num = 23;
u8	SetTempOld_HeatC,SetTempOld_HeatF,SetTempOld_CoolC,SetTempOld_CoolF,SetTempOld_AutoC,SetTempOld_AutoF;
u8	SetTemp_CoolLower_C,SetTemp_CoolUpper_C,SetTemp_HeatLower_C,SetTemp_HeatUpper_C,SetTemp_CoolLower_F,SetTemp_CoolUpper_F,SetTemp_HeatLower_F,SetTemp_HeatUpper_F;
u16	Electric_Heating = FALSE;
bit	WiFi_Tuya_Reseting = FALSE;
bit	OTAReload_Delay = FALSE;
bit	Screensaver_Enable = TRUE;
u16	Screensaver_Count = 0;
u8	BeforeScreenSavePage = 0;

static u8 s_indoor_pwr_last[6];
static u8 s_indoor_pwr_last_valid = 0;

void Control_IndoorSwitchPollStep(void)
{
	u8 i;
	u16 pwr;

	if(App_ModbusIndoorWriteBusy())
	{
		for(i = 0; i < 6; i++)
		{
			read_dgus_vp((u32)(0x4010 + i), (u8 *)&pwr, 1);
			if(pwr > 1)
				pwr = 1;
			s_indoor_pwr_last[i] = (u8)pwr;
		}
		s_indoor_pwr_last_valid = 1;
		return;
	}

	for(i = 0; i < 6; i++)
	{
		read_dgus_vp((u32)(0x4010 + i), (u8 *)&pwr, 1);
		if(pwr > 1)
			pwr = 1;
		if(!s_indoor_pwr_last_valid)
		{
			s_indoor_pwr_last[i] = (u8)pwr;
			continue;
		}
		if((u8)pwr != s_indoor_pwr_last[i])
		{
			s_indoor_pwr_last[i] = (u8)pwr;
			printf("[SCR] indoor%u pwr=%u\r\n", (u16)(i + 1), (u16)pwr);
			App_ModbusTriggerIndoorUnitWrite(i, 0);
		}
	}
	s_indoor_pwr_last_valid = 1;
}

void Control_IndoorSwitchSyncFromVp(void)
{
	u8 i;
	u16 pwr;

	for(i = 0; i < 6; i++)
	{
		read_dgus_vp((u32)(0x4010 + i), (u8 *)&pwr, 1);
		if(pwr > 1)
			pwr = 1;
		s_indoor_pwr_last[i] = (u8)pwr;
	}
	s_indoor_pwr_last_valid = 1;
}

#if UNUSED_KEEP_CODE
void Control_HoldExtPowerPoll(u8 ticks)
{
	Control_BlockExtInput(ticks);
}

void Control_NotifyExtModeVp(u16 mode)
{
	mode = mode;
}

void Control_HandleExtPowerChange(u16 pwr)
{
	pwr = pwr;
}

void Control_HandleExtModeChange(u16 mode)
{
	mode = mode;
}

void Control_PollExtPowerVp(void)
{
}

void Control_RevertExtPowerVp(void)
{
}
#endif

typedef enum
{
	SrevicePassword = 0,
	FactoryPassword = 1
}PasswordType;

/*****************************************************************************
全局变量*/
//内部RAM
u8 value0F00[4] = {0};
char GetValue0F00(void);
u16 getDar1 = 0;
u16 key_dowm = 0;
u8 set_send_type = 0;
u16 Return_key_Set = 2;

typedef union{
  u16 all; 
	u8 temp[2];
	
}TEMP_DATA;

/*****************************************************************************
判断按键值是否有上传*/
char GetValue0F00(void)
{
	u8 cleanData[4]={0};
	read_dgus_vp(DHW_0F00,value0F00,2);
	if(0x5A == value0F00[0])
	{
		getDar1 = (value0F00[1]<<8) | value0F00[2];
		write_dgus_vp(DHW_0F00,cleanData,2);
		return 1;
	}
	return 0;
}

static u16 s_touch_intent_vp;
static bit s_touch_intent_pending;

static u8 Control_IsProtectedTimerVp(u16 vp)
{
	if(vp == VP_TIMER_IN_ON_EN || vp == VP_TIMER_IN_OFF_EN || vp == VP_TIMER_IN_OFF_EN_UI)
		return 1;
	if(vp == 0x4820 || vp == 0x4821 || vp == 0x4824 || vp == 0x4826)
		return 1;
	if(vp >= 0x1080 && vp <= 0x1087)
		return 1;
	if(vp == VP_TIMER_IN_MODE || vp == 0x4836 || vp == 0x3081)
		return 1;
	if(vp == VP_TIMER_EXT_TEMP_C || vp == VP_TIMER_EXT_TEMP_F
		|| vp == VP_TIMER_IN_TEMP_C || vp == VP_TIMER_IN_TEMP_F)
		return 1;
	if(vp >= 0x4500 && vp <= 0x4506)
		return 1;
	if(vp >= 0x4510 && vp <= 0x4516)
		return 1;
	return 0;
}

void Control_TouchPollStep(void)
{
	if(GetValue0F00())
	{
		s_touch_intent_vp = getDar1;
		s_touch_intent_pending = 1;
		Screensaver_Count = 0;
	}
}

void Control_IntentStep(void)
{
	unsigned short	LockFlag;
	unsigned short	Control_u16temp;
	signed short	Control_s16temp;
	u8 i;

	if(!s_touch_intent_pending)
		return;
	s_touch_intent_pending = 0;
	getDar1 = s_touch_intent_vp;
	key_dowm = 1;
	read_dgus_vp((u32)(0x2010),(u8 *)&LockFlag,1);
	switch(getDar1)
		{
			case	0x2000://返回主页F
				Modbus_FlushSetTempUserWrite();
				App_HomePage(TRUE);
			break;
			case 0x1001:
				if(!LockFlag)
				{
					read_dgus_vp((u32)(0x2003),(u8 *)&Control_u16temp,1);
					if(!Control_u16temp)
						Page_Change_Handler(2);
					else
						Page_Change_Handler(35);
				}
			break;
			case 0x1002:
				if(!LockFlag){
					read_dgus_vp((u32)(0x2003),(u8 *)&Control_u16temp,1);
					if(!Control_u16temp)
						Page_Change_Handler(5);
					else
						Page_Change_Handler(42);
				}
			break;
			case 0x1003:
				if(!LockFlag)
					Page_Change_Handler(4);
			break;
			case	0x2020://结束屏保F
				if(IsItHomePage(BeforeScreenSavePage))//进入屏保前在任一主页则打断屏保后重新判断主页F
					App_HomePage(TRUE);
				else
					Page_Change_Handler(BeforeScreenSavePage);//打断屏保回到原有界面F
			break;
			case 0x3224:
				read_dgus_vp((u32)(0x3224),(u8 *)&Control_u16temp,1);
				if(Control_u16temp == 1 || Control_u16temp == 2)
				{
					App_ErrorHistoryPageChange(Control_u16temp);
					Control_u16temp = 0;
					write_dgus_vp((u32)(0x3224), (u8 *)&Control_u16temp, 1);
				}
			break;
			case	0x2001://内机模式F
				if(Modbus_Read_Initial)
					break;
				App_ModbusTriggerUserWrite();
				Ready_To_Save_Report();
				App_UploadWorkState();
			break;
			case	0x2002:{//外机模式F
				u16 unit;
				u16 power_mode;
				if(Modbus_Read_Initial)
					break;
				App_ModbusTriggerUserWrite();
				read_dgus_vp((u32)(0x2002),(u8 *)&Control_u16temp,1);
				if(Control_u16temp > 3)
				{
					break;
				}
				if(Control_u16temp == 0)
				{
					power_mode = 0;
					write_dgus_vp((u32)(0x4021),(u8 *)&power_mode,1);
				}
				else
				{
					power_mode = 1;
					write_dgus_vp((u32)(0x4021),(u8 *)&power_mode,1);
				}
				Control_AckExtPowerVp(power_mode);
				read_dgus_vp((u32)(0x2003),(u8 *)&unit,1);
				if(Control_u16temp == 1)
					SyncSetTempCachesFromC(Control_ResolveModeSetTempC(1, (u8)unit));
				else if(Control_u16temp == 2)
					SyncSetTempCachesFromC(Control_ResolveModeSetTempC(2, (u8)unit));
				else if(Control_u16temp == 3)
					SyncSetTempCachesFromC(Control_ResolveModeSetTempC(3, (u8)unit));
				Ready_To_Save_Report();
				App_UploadMode();
				App_UploadSwitch();
				App_UploadTempSet();
			}
			break;
			case	0x2003://温标F
				UnitChangePro();
				App_HomePage(TRUE);
				Ready_To_Save_Report();
				App_UploadTempUnit();
				App_UploadTempSet();
				App_UploadTempCurrent();
			break;
			case 0x4021:{
				if(Control_IsExtInputBlocked() || App_ModbusUserWriteBusy())
					break;
				read_dgus_vp((u32)(0x4021),(u8 *)&Control_u16temp,1);
				if(Control_u16temp > 1)
					break;
				if(Control_u16temp == Control_GetExtPowerLastCmd())
					break;
				printf("[SCR] ext pwr=%u\r\n", (u16)Control_u16temp);
				SyncLink_ApplySwitch((u8)Control_u16temp, SYNC_SRC_SCR);
				Ready_To_Save_Report();
				App_UploadMode();
			}
			break;
			case	0x2011://进入模式切换F
				Page_Change_Handler(7);
				read_dgus_vp((u32)(0x2011),(u8 *)&Control_u16temp,1);
				if(Control_u16temp == 1){
					read_dgus_vp((u32)(0x4700),(u8 *)&Control_u16temp,1);
					write_dgus_vp((u32)(0x3000),(u8*)&Control_u16temp,1);
				
					Control_u16temp = 0;
					read_dgus_vp((u32)(0x4710),(u8 *)&Control_u16temp,1);
					write_dgus_vp((u32)(0x3005),(u8*)&Control_u16temp,1);
				}
				else if(Control_u16temp == 2){
					read_dgus_vp((u32)(0x4701),(u8 *)&Control_u16temp,1);
					write_dgus_vp((u32)(0x3000),(u8*)&Control_u16temp,1);
				
					read_dgus_vp((u32)(0x4711),(u8 *)&Control_u16temp,1);
					write_dgus_vp((u32)(0x3005),(u8*)&Control_u16temp,1);
				}
				else if(Control_u16temp == 3){
					read_dgus_vp((u32)(0x4702),(u8 *)&Control_u16temp,1);
					write_dgus_vp((u32)(0x3000),(u8*)&Control_u16temp,1);
				
					read_dgus_vp((u32)(0x4712),(u8 *)&Control_u16temp,1);
					write_dgus_vp((u32)(0x3005),(u8*)&Control_u16temp,1);
				}
				else if(Control_u16temp == 4){
					read_dgus_vp((u32)(0x4703),(u8 *)&Control_u16temp,1);
					write_dgus_vp((u32)(0x3000),(u8*)&Control_u16temp,1);
				
					read_dgus_vp((u32)(0x4713),(u8 *)&Control_u16temp,1);
					write_dgus_vp((u32)(0x3005),(u8*)&Control_u16temp,1);
				}
				else if(Control_u16temp == 5){
					read_dgus_vp((u32)(0x4704),(u8 *)&Control_u16temp,1);
					write_dgus_vp((u32)(0x3000),(u8*)&Control_u16temp,1);
				
					read_dgus_vp((u32)(0x4714),(u8 *)&Control_u16temp,1);
					write_dgus_vp((u32)(0x3005),(u8*)&Control_u16temp,1);
				}
				else if(Control_u16temp == 6){
					read_dgus_vp((u32)(0x4705),(u8 *)&Control_u16temp,1);
					write_dgus_vp((u32)(0x3000),(u8*)&Control_u16temp,1);
				
					read_dgus_vp((u32)(0x4715),(u8 *)&Control_u16temp,1);
					write_dgus_vp((u32)(0x3005),(u8*)&Control_u16temp,1);
				}
//				else
					
			break;
			case 0x1016:
				Page_Change_Handler(13);
			break;
			case 0x3060:{
				u8 i=0;
				u16 CF;
				read_dgus_vp((u32)(0x1016),(u8 *)&Control_u16temp,1);
				read_dgus_vp((u32)(0x2003),(u8 *)&CF,1);
				if(Control_u16temp == 1)
				{
					for(i = 0;i<6;i++){
						read_dgus_vp((u32)(0x3140+i),(u8 *)&Control_u16temp,1);
						write_dgus_vp((u32)(0x4500+i),(u8*)&Control_u16temp,1);
					}
					if(CF == 1)
						Page_Change_Handler(41);
					else
						Page_Change_Handler(12);
				}
				else{
					for(i = 0;i<6;i++){
						read_dgus_vp((u32)(0x3140+i),(u8 *)&Control_u16temp,1);
						write_dgus_vp((u32)(0x4510+i),(u8*)&Control_u16temp,1);
					}
					if(CF == 1)
						Page_Change_Handler(39);
					else
						Page_Change_Handler(17);
				}
			}
			break;
			case 0x3083:
				App_UploadInsideTimeOpen();
				Page_Change_Handler(16);
			break;
			case 0x3084:
				App_UploadExternalTimeOpen();
				Page_Change_Handler(11);
			break;
			case 0x3050:
				read_dgus_vp((u32)(0x1016),(u8 *)&Control_u16temp,1);
				if(Control_u16temp == 1){
					read_dgus_vp((u32)(0x2003),(u8 *)&Control_u16temp,1);
					if(Control_u16temp == 1)
						Page_Change_Handler(41);
					else
						Page_Change_Handler(12);
				}
				else if(Control_u16temp == 2){
					read_dgus_vp((u32)(0x2003),(u8 *)&Control_u16temp,1);
					if(Control_u16temp == 1)
						Page_Change_Handler(41);
					else
						Page_Change_Handler(17);
				}
			break;
			case 0x2013:
				Page_Change_Handler(6);
				read_dgus_vp((u32)(0x4700),(u8 *)&Control_u16temp,1);
				write_dgus_vp((u32)(0x3020),(u8 *)&Control_u16temp,1);
			
				read_dgus_vp((u32)(0x4710),(u8 *)&Control_u16temp,1);
				write_dgus_vp((u32)(0x3025),(u8 *)&Control_u16temp,1);
			break;
			case 0x2012:
			{
				u8 unit_sel;
				read_dgus_vp((u32)(0x2011),(u8 *)&Control_u16temp,1);
				unit_sel = (u8)Control_u16temp;
				if(Control_u16temp == 1){
					Page_Change_Handler(2);
					read_dgus_vp((u32)(0x3000),(u8 *)&Control_u16temp,1);
					write_dgus_vp((u32)(0x4700),(u8*)&Control_u16temp,1);
				
					read_dgus_vp((u32)(0x3005),(u8*)&Control_u16temp,1);
					write_dgus_vp((u32)(0x4710),(u8*)&Control_u16temp,1);
				}else if(Control_u16temp == 2){
					Page_Change_Handler(2);
					read_dgus_vp((u32)(0x3000),(u8 *)&Control_u16temp,1);
					write_dgus_vp((u32)(0x4701),(u8*)&Control_u16temp,1);
				
					read_dgus_vp((u32)(0x3005),(u8*)&Control_u16temp,1);
					write_dgus_vp((u32)(0x4711),(u8*)&Control_u16temp,1);
				}else if(Control_u16temp == 3){
					Page_Change_Handler(2);
					read_dgus_vp((u32)(0x3000),(u8 *)&Control_u16temp,1);
					write_dgus_vp((u32)(0x4702),(u8*)&Control_u16temp,1);
				
					read_dgus_vp((u32)(0x3005),(u8*)&Control_u16temp,1);
					write_dgus_vp((u32)(0x4712),(u8*)&Control_u16temp,1);
				}else if(Control_u16temp == 4){
					Page_Change_Handler(3);
					read_dgus_vp((u32)(0x3000),(u8 *)&Control_u16temp,1);
					write_dgus_vp((u32)(0x4703),(u8*)&Control_u16temp,1);
				
					read_dgus_vp((u32)(0x3005),(u8*)&Control_u16temp,1);
					write_dgus_vp((u32)(0x4713),(u8*)&Control_u16temp,1);
				}else if(Control_u16temp == 5){
					Page_Change_Handler(3);
					read_dgus_vp((u32)(0x3000),(u8 *)&Control_u16temp,1);
					write_dgus_vp((u32)(0x4704),(u8*)&Control_u16temp,1);
				
					read_dgus_vp((u32)(0x3005),(u8*)&Control_u16temp,1);
					write_dgus_vp((u32)(0x4714),(u8*)&Control_u16temp,1);
				}else if(Control_u16temp == 6){
					Page_Change_Handler(3);
					read_dgus_vp((u32)(0x3000),(u8 *)&Control_u16temp,1);
					write_dgus_vp((u32)(0x4705),(u8*)&Control_u16temp,1);
				
					read_dgus_vp((u32)(0x3005),(u8*)&Control_u16temp,1);
					write_dgus_vp((u32)(0x4715),(u8*)&Control_u16temp,1);
				}
				App_UploadIndoorUnitRequest();
				if(unit_sel >= 1 && unit_sel <= 6)
				{
					App_ModbusTriggerIndoorUnitWrite((u8)(unit_sel - 1), 0);
					SyncHomeDisplayIfFirstIndoorModeChanged((u8)(unit_sel - 1));
				}
			}
			break;
			case 0x4850: /* 热水：读 4850 温度 */
				read_dgus_vp((u32)getDar1, (u8 *)&Control_u16temp, 1);
				ApplyExtSetTempFromDisplay(2, Control_u16temp);
			break;
			case 0x4852: /* 地暖：读 4852 温度 */
				read_dgus_vp((u32)getDar1, (u8 *)&Control_u16temp, 1);
				ApplyExtSetTempFromDisplay(1, Control_u16temp);
			break;
			case 0x4854: /* 泳池：读 4854 温度 */
				read_dgus_vp((u32)getDar1, (u8 *)&Control_u16temp, 1);
				ApplyExtSetTempFromDisplay(3, Control_u16temp);
			break;
//			case	0x2012://进入功能设置F
//				if(!LockFlag)
//					Page_Change_Handler(16);
//			break;
			case	0x2015://进入设定温度设置F
				PrefillSetTempEditPage();
			break;
			case	0x2016:{//保存设定温度F
				
//				SaveSetTemperature();
				unsigned short	SaveSetTemp_u16tempC;
				unsigned short	SaveSetTemp_u16tempF;
				unsigned short	SaveSetTemp_u16temp;
				unsigned short	SaveSetTemp_u16mode;
				read_dgus_vp((u32)(0x2003),(u8 *)&SaveSetTemp_u16temp,1);//判断点击保存的温度温标F
				if(!SaveSetTemp_u16temp)
				{
					read_dgus_vp((u32)(0x2025),(u8 *)&SaveSetTemp_u16tempC,1);
					write_dgus_vp((u32)(0x2005),(u8*)&SaveSetTemp_u16tempC,1);
					SaveSetTemp_u16tempF = TempUnitTrans(SaveSetTemp_u16tempC,'F');
					write_dgus_vp((u32)(0x2006),(u8*)&SaveSetTemp_u16tempF,1);
					read_dgus_vp((u32)(0x2002),(u8 *)&SaveSetTemp_u16mode,1);
					SetTempIntWrite((u8)SaveSetTemp_u16mode, 0, SaveSetTemp_u16tempC);
					SetTempIntWrite((u8)SaveSetTemp_u16mode, 1, SaveSetTemp_u16tempF);
					SetTempFloatWriteC((u8)SaveSetTemp_u16mode, (float)SaveSetTemp_u16tempC);
					Page_Change_Handler(1);
				}
				else
				{
					u16 save_mode;
					read_dgus_vp((u32)(0x2026),(u8 *)&SaveSetTemp_u16temp,1);
					write_dgus_vp((u32)(0x2006),(u8*)&SaveSetTemp_u16temp,1);
					SaveSetTemp_u16tempC = (u16)TempUnitTrans((signed short)SaveSetTemp_u16temp,'C');
					write_dgus_vp((u32)(0x2005),(u8*)&SaveSetTemp_u16tempC,1);
					read_dgus_vp((u32)(0x2002),(u8 *)&save_mode,1);
					SetTempIntWrite((u8)save_mode, 0, SaveSetTemp_u16tempC);
					SetTempIntWrite((u8)save_mode, 1, SaveSetTemp_u16temp);
					SetTempFloatWriteC((u8)save_mode, (float)SaveSetTemp_u16tempC);
					Page_Change_Handler(30);
				}
				App_ModbusTriggerUserWrite();
				Ready_To_Save_Report();
				App_UploadTempSet();
			}
			break;
			case	0x2021://设定模式F
				read_dgus_vp((u32)(0x2021),(u8 *)&Control_u16temp,1);
				SetModeProcess(Control_u16temp);
				App_ModbusTriggerUserWrite();
				Ready_To_Save_Report();
				App_UploadWorkState();
			break;
//			case 0x482e:
//				read_dgus_vp((u32)(0x482e),(u8 *)&Control_u16temp,1);
//				printf("Control_u16temp = %d\r\n",Control_u16temp);
//			break;
//			case 0x4829:
//				read_dgus_vp((u32)(0x4829),(u8 *)&Control_u16temp,1);
//				printf("Control_u16temp = %d\r\n",Control_u16temp);
//			break;
//			case	0x2025://摄氏度缓存F
//				Temp_Limit(0x2025,'C');
//			break;
//			case	0x2026://华氏度缓存F
//				Temp_Limit(0x2026,'F');
//			break;
			case	0x2017://进入时间调整页面F
				Page_Change_Handler(15);
				RTCdisplay4change();
			break;
			case	0x2018://语言切换F
				App_ApplyLanguageFromTouchVp();
			break;
//			case	0x2019://设定参数输入密码F
//				read_dgus_vp((u32)(0x2019),(u8 *)&Control_u16temp,1);
//				if(Control_u16temp == 481)
//				{
//					Page_Change_Handler(50);
//					Modbus_Read_Set = TRUE;
//				}
//				else
//					Page_Change_Handler(49);
//			break;
//			case	0x201A://清零耗电量计数F
//				Modbus_Write_ClearPower = TRUE;
//			break;
			case	0x201B://手动除霜F
				s_defrost_start_sec = RTC_GetUnixLocal();
				App_ModbusStartManualDefrost();
				App_UploadDefrost();
			break;
//			case	0x201C://使能/禁用定时F
//				Timer_EnableOrDisable();
//				Ready_To_Save_Report();
//			break;
//			case	0x201D://进入设定定时页面F
//				TimerDisplay();
//				Page_Change_Handler(33);
//			break;
			case	0x201E://亮度调整F
				read_dgus_vp((u32)(0x201E),(u8 *)&Control_u16temp,1);
				write_dgus_vp((u32)(0x201E),(u8 *)&Control_u16temp,1);
				Brightness_Handler(Control_u16temp);
				Ready_To_Save_Report();
			break;
			case 0x3508:{
				u16 Enable_Memory = 0;
				read_dgus_vp((u32)(0x3508),(u8 *)&Enable_Memory,1);
				write_dgus_vp((u32)(0x3508),(u8 *)&Enable_Memory,1);
				App_ErrorHistoryTryMigrateFlash();
				Ready_To_Save_Report();
				App_UploadRelayStatus();
			}
			break;
			case 0x1103:
				Page_Change_Handler(61);
			break;
			case 0x3509:
				App_ErrorHistoryTryMigrateFlash();
				Ready_To_Save_Report();
				App_UploadRelayStatus();
				Page_Change_Handler(52);
			break;
			case 0x1018:
				Page_Change_Handler(14);
			break;
			case 0x1100:
				read_dgus_vp((u32)(0x2003),(u8 *)&Control_u16temp,1); 
				if(Control_u16temp == 0)
					Page_Change_Handler(11);
				else
					Page_Change_Handler(40);
			break;
			case 0x1101:
				read_dgus_vp((u32)(0x2003),(u8 *)&Control_u16temp,1); 
				if(Control_u16temp == 0)
					Page_Change_Handler(11);
				else
					Page_Change_Handler(40);
			break;
			case 0x1102:
				read_dgus_vp((u32)(0x2003),(u8 *)&Control_u16temp,1); 
				if(Control_u16temp == 0)
					Page_Change_Handler(16);
				else
					Page_Change_Handler(38);
			break;
			case 0x1019:
				read_dgus_vp((u32)(0x1018),(u8 *)&Control_u16temp,1); 
				if(Control_u16temp == 1){
					read_dgus_vp((u32)(0x1019),(u8 *)&Control_u16temp,1); 
					if(Control_u16temp == 1)
					{
						read_dgus_vp((u32)(0x3050),(u8 *)&Control_u16temp,1); 
						write_dgus_vp((u32)(0x1080),(u8 *)&Control_u16temp,1); 
						read_dgus_vp((u32)(0x3052),(u8 *)&Control_u16temp,1); 
						write_dgus_vp((u32)(0x1081),(u8 *)&Control_u16temp,1); 
						App_UploadExternalTimeOpen();
					}
					Page_Change_Handler(12);
				}
				else if(Control_u16temp == 2){
					read_dgus_vp((u32)(0x1019),(u8 *)&Control_u16temp,1); 
					if(Control_u16temp == 1)
					{
						read_dgus_vp((u32)(0x3050),(u8 *)&Control_u16temp,1); 
						write_dgus_vp((u32)(0x1082),(u8 *)&Control_u16temp,1); 
						read_dgus_vp((u32)(0x3052),(u8 *)&Control_u16temp,1); 
						write_dgus_vp((u32)(0x1083),(u8 *)&Control_u16temp,1); 
						App_UploadExternalTimeClose();
					}
					Page_Change_Handler(11);
				}
				else if(Control_u16temp == 3){
					read_dgus_vp((u32)(0x1019),(u8 *)&Control_u16temp,1); 
					if(Control_u16temp == 1)
					{
						read_dgus_vp((u32)(0x3050),(u8 *)&Control_u16temp,1); 
						write_dgus_vp((u32)(0x1084),(u8 *)&Control_u16temp,1); 
						read_dgus_vp((u32)(0x3052),(u8 *)&Control_u16temp,1); 
						write_dgus_vp((u32)(0x1085),(u8 *)&Control_u16temp,1); 
					}
					Page_Change_Handler(18);
				}
				else if(Control_u16temp == 4){
					read_dgus_vp((u32)(0x1019),(u8 *)&Control_u16temp,1); 
					if(Control_u16temp == 1)
					{
						read_dgus_vp((u32)(0x3050),(u8 *)&Control_u16temp,1); 
						write_dgus_vp((u32)(0x1086),(u8 *)&Control_u16temp,1); 
						read_dgus_vp((u32)(0x3052),(u8 *)&Control_u16temp,1); 
						write_dgus_vp((u32)(0x1087),(u8 *)&Control_u16temp,1);
						App_UploadInsideTimeClose();						
					}
					Page_Change_Handler(16);
				}
			break;
			case	0x201D://查看运行参数F
				read_dgus_vp((u32)(0x2003),(u8 *)&Control_u16temp,1);
				if(Control_u16temp)
					Page_Change_Handler(32);
				else
					Page_Change_Handler(50);
				Control_SanitizeSetTempIntVp();
			break;
			case	0x2030://2030/2031/2032 长按童锁（DGUS 按住写 0 到 2030）
			case	0x2031:
			case	0x2032:
				if(getDar1 == 0x2031)
				{
					if(!LockKeyCountEnalbe)
					{
						LockKeyCountEnalbe = TRUE;
						LockKeyCount = 0;
						s_lock_key_vp = 0x2030;
					}
				}
				else if(getDar1 == 0x2032)
				{
					u16 lock_st;

					if(LockKeyCount < 2000)
					{
						LockKeyCountEnalbe = FALSE;
						LockKeyCount = 0;
					}
					read_dgus_vp((u32)(0x2010), (u8 *)&lock_st, 1);
					write_dgus_vp((u32)(0x2030), (u8 *)&lock_st, 1);
				}
				else
				{
					read_dgus_vp((u32)0x2030,(u8 *)&Control_u16temp,1);
					if(Control_u16temp == 0)
					{
						if(!LockKeyCountEnalbe)
						{
							LockKeyCountEnalbe = TRUE;
							LockKeyCount = 0;
							s_lock_key_vp = 0x2030;
						}
					}
					/* 2030 变非 0 不在此取消；2032 松开且未够时长才取消，避免刷新 2030 打断解锁 */
				}
			break;
			case 0x4006:{
				unsigned short	SaveSetTemp_u16mode;
				unsigned short	SaveSetTemp_u16mode1;
				read_dgus_vp((u32)(0x3020),(u8*)&SaveSetTemp_u16mode,1);
				for(i=0;i<6;i++){
					write_dgus_vp((u32)(0x4700+i),(u8*)&SaveSetTemp_u16mode,1);
					Control_u16temp = 1;
					write_dgus_vp((u32)(0x4010+i),(u8*)&Control_u16temp,1);
				}
				
				read_dgus_vp((u32)(0x3025),(u8*)&SaveSetTemp_u16mode1,1);
				for(i=0;i<6;i++)
					write_dgus_vp((u32)(0x4710+i),(u8*)&SaveSetTemp_u16mode1,1);
				
				read_dgus_vp((u32)(0x1090),(u8*)&Control_u16temp,1);
				for(i=0;i<6;i++)
					write_dgus_vp((u32)(0x1020+i),(u8*)&Control_u16temp,1);
				App_UploadIndoorUnitRequest();
				App_ModbusTriggerGroupControlWrite();
				Ready_To_Save_Report();
				Page_Change_Handler(2);
			}
			break;
			case 0x3090:
				read_dgus_vp((u32)(0x3090),(u8*)&Control_u16temp,1);
				write_dgus_vp((u32)(0x2002),(u8*)&Control_u16temp,1);
				App_ModbusTriggerUserWrite();
				Ready_To_Save_Report();
				App_UploadMode();
			break;
			case	0x2035://历史故障
				App_ErrorHistoryTryMigrateFlash();
				App_ErrorHistoryResetPage();
				Page_Change_Handler(45);
				App_ErrorHistoryDisplay();
			break;
			case	0x2036://确认是否清空历史故障F
				read_dgus_vp((u32)(0x2036),(u8 *)&Control_u16temp,1);
				if(Control_u16temp == 1)
				{
					App_ClearErrorHistory();
					App_ErrorHistoryDisplay();
					Control_u16temp = 0;
					write_dgus_vp((u32)(0x2036),(u8 *)&Control_u16temp,1);
				}
			break;
			case 0x3660:
			case 0x3670:
			case 0x3680:
			case VP_DEFROST_T3_C:
			case VP_DEFROST_T3_F:
				App_ModbusTriggerDefrostParamWrite(getDar1);
				Ready_To_Save_Report();
				App_UploadDefrostFreq();
				App_UploadDefrostTime();
				App_UploadDefrostOutTemp();
			break;
			case 0x4010:
			case 0x4011:
			case 0x4012:
			case 0x4013:
			case 0x4014:
			case 0x4015:
				if(getDar1 >= 0x4010)
				{
					read_dgus_vp((u32)getDar1, (u8 *)&Control_u16temp, 1);
					printf("[SCR] indoor vp=%u pwr=%u\r\n", (u16)getDar1, (u16)Control_u16temp);
					App_ModbusTriggerIndoorUnitWrite((u8)(getDar1 - 0x4010), 0);
					Control_IndoorSwitchSyncFromVp();
					HomePage_SyncIndoorDisplay();
				}
				Ready_To_Save_Report();
				App_UploadIndoorUnitRequest();
			break;
			case 0x4700:
			case 0x4701:
			case 0x4702:
			case 0x4703:
			case 0x4704:
			case 0x4705:
				if(getDar1 >= 0x4700)
					App_ModbusTriggerIndoorUnitWrite((u8)(getDar1 - 0x4700), 0);
				HomePage_SyncIndoorDisplay();
				Ready_To_Save_Report();
				App_UploadIndoorUnitRequest();
			break;
			case 0x4710:
			case 0x4711:
			case 0x4712:
			case 0x4713:
			case 0x4714:
			case 0x4715:
				if(getDar1 >= 0x4710)
					App_ModbusTriggerIndoorUnitWrite((u8)(getDar1 - 0x4710), 0);
				Ready_To_Save_Report();
				App_UploadIndoorUnitRequest();
			break;
			case 0x1020:
			case 0x1021:
			case 0x1022:
			case 0x1023:
			case 0x1024:
			case 0x1025:
			case 0x2025:
			case 0x2026:
				Temp_Limit(getDar1, 'C');
				if(getDar1 >= 0x1020)
					App_ModbusTriggerIndoorUnitWrite((u8)(getDar1 - 0x1020), 0);
				Ready_To_Save_Report();
				App_UploadIndoorUnitRequest();
			break;
			case 0x1070:
			case 0x1071:
			case 0x1072:
			case 0x1073:
			case 0x1074:
			case 0x1075:
			{
				u8 idx;
				u16 temp_f;
				u16 temp_c;

				Temp_Limit(getDar1, 'F');
				idx = (u8)(getDar1 - VP_INDOOR_SET_F_BASE);
				read_dgus_vp((u32)getDar1, (u8 *)&temp_f, 1);
				temp_c = (u16)TempUnitTrans((signed short)temp_f, 'C');
				write_dgus_vp((u32)(0x1020 + idx), (u8 *)&temp_c, 1);
				App_ModbusTriggerIndoorUnitWrite(idx, 0);
				Ready_To_Save_Report();
				App_UploadIndoorUnitRequest();
			}
			break;
			case	0x2037://设置页返回 / WiFi 复位
				App_HandleSettingsBackOrWifiReset();
			break;
//			case 0x4826:
//				
//			break;
			case 0x4821:
			case 0x4826:
			case 0x4824:
			case 0x1080:
			case 0x1081:
				App_UploadExternalTimeOpen();
				Ready_To_Save_Report();
			break;
			case VP_TIMER_EXT_TEMP_F:
			{
				u16 temp_f;
				u16 temp_c;

				read_dgus_vp((u32)getDar1, (u8 *)&temp_f, 1);
				temp_c = (u16)TempUnitTrans((signed short)temp_f, 'C');
				write_dgus_vp(VP_TIMER_EXT_TEMP_C, (u8 *)&temp_c, 1);
				App_UploadExternalTimeOpen();
				Ready_To_Save_Report();
			}
			break;
			case 0x4825:
			case 0x1084:
			case 0x1085:
				App_ModbusApplyIndoorTimerVpToUnits(1, 0);
				App_UploadInsideTimeOpen();
				Ready_To_Save_Report();
			break;
			case VP_TIMER_IN_TEMP_F:
			{
				u16 temp_f;
				u16 temp_c;

				read_dgus_vp((u32)getDar1, (u8 *)&temp_f, 1);
				temp_c = (u16)TempUnitTrans((signed short)temp_f, 'C');
				write_dgus_vp(VP_TIMER_IN_TEMP_C, (u8 *)&temp_c, 1);
				App_ModbusApplyIndoorTimerVpToUnits(1, 0);
				App_UploadInsideTimeOpen();
				Ready_To_Save_Report();
			}
			break;
			case VP_TIMER_IN_OFF_EN_UI:
				DgusCopyTimerOffUiToProto();
				App_ModbusApplyIndoorTimerVpToUnits(0, 1);
				App_UploadInsideTimeClose();
				Ready_To_Save_Report();
			break;
			case 0x4827:
			case 0x1086:
			case 0x1087:
				App_ModbusApplyIndoorTimerVpToUnits(0, 1);
				DgusCopyTimerOffProtoToUi();
				App_UploadInsideTimeClose();
				Ready_To_Save_Report();
			break;
			case 0x4510:
			case 0x4511:
			case 0x4512:
			case 0x4513:
			case 0x4514:
			case 0x4515:
			case 0x4516:
			case 0x4836:
			case 0x4838:
			case 0x3081:
				App_ModbusApplyIndoorTimerVpToUnits(1, 0);
				App_UploadInsideTimeOpen();
				Ready_To_Save_Report();
			break;
			case 0x4837:
				App_UploadInsideTimeOpen();
				Ready_To_Save_Report();
			break;
			case 0x4820:
			case 0x1082:
			case 0x1083:
				App_UploadExternalTimeClose();
				Ready_To_Save_Report();
			break;
			case 0x4500:
			case 0x4501:
			case 0x4502:
			case 0x4503:
			case 0x4504:
			case 0x4505:
			case 0x4506:
				App_UploadExternalTimeOpen();
				Ready_To_Save_Report();
			break;
			case	0x2038://开关屏保功能F
				read_dgus_vp((u32)(0x2038),(u8 *)&Control_u16temp,1);
				if(Control_u16temp)
					Screensaver_Enable = TRUE;
				else
					Screensaver_Enable = FALSE;
				ScreenTimeOut_Set(!Screensaver_Enable);
				Ready_To_Save_Report();
			break;
			case	0x3070://时间调整页面年月日时分F
			case	0x3072:
			case	0x3073:
			case	0x3074:
			case	0x3075:
				Ready4WriteRTC();
			break;
			case 0x1200:
				App_ErrorHistoryTryMigrateFlash();
				Ready_To_Save_Report();
				App_UploadRelayStatus();
				Page_Change_Handler(52);
			break;
		}
		if((getDar1 >= 0x4800)&&(getDar1 <= (0x4800+SET_PARAMETER_NUM-1)))
		{
			unsigned char para_idx;

			if(getDar1 == 0x4800 || getDar1 == 0x4802 || getDar1 == 0x4804 || getDar1 == 0x4806
			|| getDar1 == 0x4810 || getDar1 == 0x4812 || getDar1 == 0x4814 || getDar1 == 0x4816)
				;
			else if(getDar1 == VP_EXT_SETTEMP_DHW_C
				|| getDar1 == VP_EXT_SETTEMP_FLOOR_C
				|| getDar1 == VP_EXT_SETTEMP_POOL_C
				|| getDar1 == VP_EXT_SETTEMP_DHW_F
				|| getDar1 == VP_EXT_SETTEMP_FLOOR_F
				|| getDar1 == VP_EXT_SETTEMP_POOL_F)
				;
			else if(Control_IsProtectedTimerVp(getDar1))
				;
			else
			{
				u8 unit_f = 0;
				u8 ext_mode;

				ext_mode = Control_ExtSetTempModeFromAddr(getDar1, &unit_f);
				if(ext_mode)
				{
					Temp_Limit(getDar1, unit_f ? 'F' : 'C');
					if(!unit_f)
					{
						Modbus_QueueSetTempUserWrite();
						App_UploadTempSet();
					}
					Ready_To_Save_Report();
				}
				else
				{
			read_dgus_vp((u32)(getDar1),(u8 *)&Control_s16temp,1);
			para_idx = (unsigned char)(getDar1 - 0x4800);
			if(SetParameterUpperLimit[para_idx] > SetParameterLowerLimit[para_idx])
			{
				if(Control_s16temp > SetParameterUpperLimit[para_idx])
					Control_s16temp = SetParameterUpperLimit[para_idx];
				else if(Control_s16temp < SetParameterLowerLimit[para_idx])
					Control_s16temp = SetParameterLowerLimit[para_idx];
			}
			write_dgus_vp((u32)(getDar1),(u8*)&Control_s16temp,1);
			Ready_To_Save_Report();
				}
			}
		}
}

bit	IsItHomePage(unsigned char NowPage)
{
	bit	IsHomePage = FALSE;
	unsigned char	i;
	
	for(i=0;i<HOMEPAGENUM;i++)
	{
		if(NowPage == HomePageID[i])//判断当前页面是否为主页之一F
		{
			IsHomePage = TRUE;
			break;
		}
	}
	return	IsHomePage;
}

void HomePage_SyncIndoorDisplay(void)
{
	unsigned short	HomePage_Power = 0;
	u8 i = 0;
	u8 first_one = 0xFF;
	static u8 s_last_anim_mode = 0xFF;

	for(i = 0; i < 6; i++)
	{
		read_dgus_vp((u32)(0x4010 + i),(u8 *)&HomePage_Power,1);
		if(HomePage_Power == 1)
		{
			first_one = i;
			break;
		}
	}
	HomePage_Power = 0;
	if(first_one < 6)
	{
		read_dgus_vp((u32)(0x4700 + first_one),(u8 *)&HomePage_Power,1);
		if(HomePage_Power > 4)
			HomePage_Power = 0;
		write_dgus_vp((u32)(0x1101 + (u16)first_one * 0x20),(u8 *)&HomePage_Power,1);
	}
	write_dgus_vp((u32)(0x2001),(u8 *)&HomePage_Power,1);
	write_dgus_vp((u32)VP_HOME_INDOOR_MODE,(u8 *)&HomePage_Power,1);
	if(HomePage_Power != s_last_anim_mode)
	{
		HomePage_UpdateModeAnimation((u8)HomePage_Power);
		s_last_anim_mode = (u8)HomePage_Power;
	}
}

void	HomePage(bit TurnPage)
{
	unsigned short	HomePage_TempType = 0;

	if(!TurnPage)
	{
		if(!IsItHomePage(GetPageID()))
			return;
		App_HomePageSyncIndoorDisplay();
		return;
	}

	read_dgus_vp((u32)(0x2003),(u8 *)&HomePage_TempType,1);
	if(!HomePage_TempType)
		Page_Change_Handler(1);
	else
		Page_Change_Handler(30);
	App_HomePageSyncIndoorDisplay();
	TempUnit_RefreshActiveSetFromMode();
}

void	Temp_Limit(unsigned short Addr,unsigned char TempType)
{
	unsigned short Temp_Limit_Temp;
	unsigned short lo, hi;

	(void)TempType;
	read_dgus_vp((u32)(Addr),(u8 *)&Temp_Limit_Temp,1);
	if(Addr >= 0x1020 && Addr <= 0x1025)
	{
		lo = 16;
		hi = 32;
	}
	else if(Addr >= VP_INDOOR_SET_F_BASE && Addr <= (VP_INDOOR_SET_F_BASE + 5))
	{
		lo = (unsigned short)TempUnitTrans(16, 'F');
		hi = (unsigned short)TempUnitTrans(32, 'F');
	}
	else if(Addr == 0x2025)
	{
		lo = 16;
		hi = 32;
	}
	else if(Addr == 0x2026)
	{
		lo = (unsigned short)TempUnitTrans(16, 'F');
		hi = (unsigned short)TempUnitTrans(32, 'F');
	}
	else
	{
		u8 unit_f = 0;
		u8 mode = Control_ExtSetTempModeFromAddr(Addr, &unit_f);

		if(!mode)
			return;
		Control_GetSetTempCLimit(mode, &lo, &hi);
		if(unit_f)
		{
			lo = (unsigned short)TempUnitTrans((signed short)lo, 'F');
			hi = (unsigned short)TempUnitTrans((signed short)hi, 'F');
		}
	}
	if(Temp_Limit_Temp < lo)
		Temp_Limit_Temp = lo;
	else if(Temp_Limit_Temp > hi)
		Temp_Limit_Temp = hi;
	write_dgus_vp((u32)(Addr),(u8*)&Temp_Limit_Temp,1);
	if(Addr >= 0x1020 && Addr <= 0x1025)
		SyncIndoorSetTempVpPair((u8)(Addr - 0x1020), Temp_Limit_Temp);
	else if(Addr >= VP_INDOOR_SET_F_BASE && Addr <= (VP_INDOOR_SET_F_BASE + 5))
	{
		Temp_Limit_Temp = (u16)TempUnitTrans((signed short)Temp_Limit_Temp, 'C');
		SyncIndoorSetTempVpPair((u8)(Addr - VP_INDOOR_SET_F_BASE), Temp_Limit_Temp);
	}
	else if(Addr == 0x2025)
		SyncSetTempCachesFromC(Temp_Limit_Temp);
	else if(Addr == 0x2026)
		SyncSetTempCachesFromC((u16)TempUnitTrans((signed short)Temp_Limit_Temp, 'C'));
	else
	{
		u8 unit_f = 0;
		u8 mode = Control_ExtSetTempModeFromAddr(Addr, &unit_f);

		if(mode)
		{
			if(unit_f)
				SetTempIntWrite(mode, 0,
					(u16)TempUnitTrans((signed short)Temp_Limit_Temp, 'C'));
			else
			{
				SetTempIntWrite(mode, 0, Temp_Limit_Temp);
				SetTempIntWrite(mode, 1,
					(u16)TempUnitTrans((signed short)Temp_Limit_Temp, 'F'));
			}
			Control_MirrorExtSetTempDisplay(mode);
		}
	}
}

void Control_SanitizeSetTempIntVp(void)
{
	Control_MirrorAllExtSetTempDisplay();
}

void	SetModeProcess(unsigned short	NewMode)//保存和读取各模式设定温度F
{
	write_dgus_vp((u32)(0x2001),(u8*)&NewMode,1);
}
void	RTCdisplay4change(void)
{
	unsigned short	RTCS2C_temp;
	unsigned short	year, month_vp, month;

	read_dgus_vp((u32)(0x3010),(u8 *)&year,1);
	write_dgus_vp((u32)(0x3070),(u8 *)&year,1);
	read_dgus_vp((u32)(0x3011),(u8 *)&month_vp,1);
	month = RTC_MonthFromDisplayVp(year, month_vp);
	write_dgus_vp((u32)(0x3072),(u8 *)&month,1);
	read_dgus_vp((u32)(0x3012),(u8 *)&RTCS2C_temp,1);
	write_dgus_vp((u32)(0x3073),(u8 *)&RTCS2C_temp,1);
	read_dgus_vp((u32)(0x3013),(u8 *)&RTCS2C_temp,1);
	write_dgus_vp((u32)(0x3074),(u8 *)&RTCS2C_temp,1);
	read_dgus_vp((u32)(0x3014),(u8 *)&RTCS2C_temp,1);
	write_dgus_vp((u32)(0x3075),(u8 *)&RTCS2C_temp,1);
}
void	Ready4WriteRTC(void)
{
	unsigned short	RTCC2W_write[3];
	u16 year_full, month, day, hour, minute, year;

	read_dgus_vp((u32)(0x3070),(u8 *)&year_full,1);
	read_dgus_vp((u32)(0x3072),(u8 *)&month,1);
	read_dgus_vp((u32)(0x3073),(u8 *)&day,1);
	read_dgus_vp((u32)(0x3074),(u8 *)&hour,1);
	read_dgus_vp((u32)(0x3075),(u8 *)&minute,1);
	if(year_full >= 2000)
		year = year_full - 2000;
	else
		year = year_full;
	RTCC2W_write[0] = (year << 8) | (month & 0xff);
	RTCC2W_write[1] = (day << 8) | (hour & 0xff);
	RTCC2W_write[2] = (minute << 8);
	write_dgus_vp((u32)(0x009d),(u8*)&RTCC2W_write,3);//向系统接口预先存下时间待按下确认按钮后直接在显示核写入RTC
}

void SyncSetTempCachesFromC(u16 temp_c)
{
	u16 temp_f;
	u16 mode;

	read_dgus_vp((u32)0x2002, (u8 *)&mode, 1);
	if(mode >= 1 && mode <= 3)
		temp_c = Control_ClampSetTempCByMode((u8)mode, temp_c);
	temp_f = (u16)TempUnitTrans((signed short)temp_c, 'F');
	write_dgus_vp((u32)0x2005, (u8 *)&temp_c, 1);
	write_dgus_vp((u32)0x2006, (u8 *)&temp_f, 1);
	if(mode >= 1 && mode <= 3)
	{
		SetTempIntWrite((u8)mode, 0, temp_c);
		SetTempIntWrite((u8)mode, 1, temp_f);
		Control_MirrorExtSetTempDisplay((u8)mode);
	}
}

static void TempUnit_RefreshActiveSetFromMode(void)
{
	u16 mode;
	u16 unit;
	u16 temp_c;

	read_dgus_vp((u32)0x2002, (u8 *)&mode, 1);
	read_dgus_vp((u32)0x2003, (u8 *)&unit, 1);
	if(mode < 1 || mode > 3)
		return;
	temp_c = Control_ResolveModeSetTempC((u8)mode, (u8)unit);
	SyncSetTempCachesFromC(temp_c);
}

void TempUnit_RefreshAll(void)
{
	u16 unit;
	u16 temp_c;
	u16 temp_f;
	u8 i;

	read_dgus_vp((u32)0x2003, (u8 *)&unit, 1);
	read_dgus_vp((u32)0x2005, (u8 *)&temp_c, 1);
	read_dgus_vp((u32)0x2006, (u8 *)&temp_f, 1);
	if(unit)
	{
		if(temp_c)
			temp_f = (u16)TempUnitTrans((signed short)temp_c, 'F');
		else if(temp_f)
			temp_c = (u16)TempUnitTrans((signed short)temp_f, 'C');
	}
	else
	{
		if(temp_f)
			temp_c = (u16)TempUnitTrans((signed short)temp_f, 'C');
		else if(temp_c)
			temp_f = (u16)TempUnitTrans((signed short)temp_c, 'F');
	}
	if(temp_c)
		SyncSetTempCachesFromC(temp_c);
	else if(temp_f)
	{
		temp_c = (u16)TempUnitTrans((signed short)temp_f, 'C');
		SyncSetTempCachesFromC(temp_c);
	}
	write_dgus_vp((u32)0x2025, (u8 *)&temp_c, 1);
	write_dgus_vp((u32)0x2026, (u8 *)&temp_f, 1);
	TempUnit_RefreshActiveSetFromMode();

	TempUnit_RefreshFloatPair(0x4800, 0x4810);
	TempUnit_RefreshFloatPair(0x4802, 0x4812);
	TempUnit_RefreshFloatPair(0x4804, 0x4814);
	TempUnit_RefreshFloatPair(0x4806, 0x4816);
	for(i = 1; i <= 3; i++)
	{
		Control_ClampModeSetTempVp(i);
		temp_c = SetTempIntRead(i, 0);
		if(temp_c)
		{
			temp_f = (u16)TempUnitTrans((signed short)temp_c, 'F');
			SetTempIntWrite(i, 1, temp_f);
		}
		WDT_RST();
	}
	TempUnit_RefreshFloatPair(0x1006, 0x1008);
	TempUnit_RefreshFloatPair(VP_DEFROST_T3_C, VP_DEFROST_T3_F);

	for(i = 0; i < 6; i++)
	{
		float room_c;

		read_dgus_vp((u32)(0x1020 + i), (u8 *)&temp_c, 1);
		SyncIndoorSetTempVpPair(i, temp_c);
		read_dgus_vp((u32)(VP_INDOOR_ROOM_C_BASE + (u16)i * 2), (u8 *)&room_c, 2);
		if(room_c > 0.0f)
			TempUnit_RefreshFloatPair(
				(u32)(VP_INDOOR_ROOM_C_BASE + (u16)i * 2),
				(u32)(VP_INDOOR_ROOM_F_BASE + (u16)i * 2));
		WDT_RST();
	}

	read_dgus_vp((u32)VP_TIMER_EXT_TEMP_C, (u8 *)&temp_c, 1);
	if(temp_c)
		SyncTimerTempVpPair(VP_TIMER_EXT_TEMP_C, VP_TIMER_EXT_TEMP_F, temp_c);
	read_dgus_vp((u32)VP_TIMER_IN_TEMP_C, (u8 *)&temp_c, 1);
	if(temp_c)
		SyncTimerTempVpPair(VP_TIMER_IN_TEMP_C, VP_TIMER_IN_TEMP_F, temp_c);

	Control_MirrorAllExtSetTempDisplay();
	App_ModbusRefreshCheckParamDisplay();
}

void	UnitChangePro(void)
{
	unsigned short	UnitChange_TempType = 0;
	
	read_dgus_vp((u32)(0x2003),(u8 *)&UnitChange_TempType,1);
	TempUnit_RefreshAll();
}
//void	SaveSetTemperature(void)
//{
//	
//}
//void	Timer_EnableOrDisable(void)
//{
//	unsigned char	i;
//	unsigned short	Timer_EnableOrDisable_u16temp;
//	TEMP_DATA	u16tou8;
//	
//	read_dgus_vp((u32)(0x201C),(u8 *)&Timer_EnableOrDisable_u16temp,1);
//	TimerEnable = (Timer_EnableOrDisable_u16temp) ? (TRUE) : (FALSE);//读取触发位置F
//	if(TimerEnable)
//	{
//		for(i=0;i<4;i++)//保存缓存区时间点F
//		{
//			read_dgus_vp((u32)(0x3020+i),(u8 *)&u16tou8.all,1);
//			TimerSet[i] = u16tou8.temp[1];
//		}
//		if((TimerSet[0] == TimerSet[2])&&(TimerSet[1] == TimerSet[3]))//开关时间点设置相同则不启用定时F
//		{
//			TimerEnable = FALSE;
//			Timer_EnableOrDisable_u16temp = 0;
//			write_dgus_vp((u32)(0x201C),(u8*)&Timer_EnableOrDisable_u16temp,1);
//		}
//		Page_Change_Handler(31);//点击保存键回上一页F
//	}
//}
//void	TimerDisplay(void)
//{
//	unsigned char	i;
//	TEMP_DATA	u8tou16 = {0};
//	
//	for(i=0;i<4;i++)//将当前存储的时间点显示F
//	{
//		u8tou16.temp[1] = TimerSet[i];
//		write_dgus_vp((u32)(0x3020+i),(u8*)&u8tou16.all,1);
//	}
//}

static void SyncTimerFromVp(void)
{
	TEMP_DATA u8tou16 = {0};

	DgusCopyTimerOffUiToProto();
	read_dgus_vp(0x4821, (u8 *)&u8tou16.all, 1);
	ExternTimer.On_Enable = u8tou16.temp[1];
	read_dgus_vp(0x1080, (u8 *)&u8tou16.all, 1);
	ExternTimer.OnHour = u8tou16.temp[1];
	read_dgus_vp(0x1081, (u8 *)&u8tou16.all, 1);
	ExternTimer.OnMinute = u8tou16.temp[1];
	read_dgus_vp(0x4820, (u8 *)&u8tou16.all, 1);
	ExternTimer.Off_Enable = u8tou16.temp[1];
	read_dgus_vp(0x1082, (u8 *)&u8tou16.all, 1);
	ExternTimer.OffHour = u8tou16.temp[1];
	read_dgus_vp(0x1083, (u8 *)&u8tou16.all, 1);
	ExternTimer.OffMinute = u8tou16.temp[1];
	read_dgus_vp(0x4826, (u8 *)&u8tou16.all, 1);
	ExternTimer.Mode = u8tou16.temp[1];
	ExternTimer.Temp = ReadTimerTempC(VP_TIMER_EXT_TEMP_C, VP_TIMER_EXT_TEMP_F);

	read_dgus_vp(0x4825, (u8 *)&u8tou16.all, 1);
	ExternTimer1.On_Enable = u8tou16.temp[1];
	read_dgus_vp(0x1084, (u8 *)&u8tou16.all, 1);
	ExternTimer1.OnHour = u8tou16.temp[1];
	read_dgus_vp(0x1085, (u8 *)&u8tou16.all, 1);
	ExternTimer1.OnMinute = u8tou16.temp[1];
	read_dgus_vp(0x4827, (u8 *)&u8tou16.all, 1);
	ExternTimer1.Off_Enable = u8tou16.temp[1];
	read_dgus_vp(0x1086, (u8 *)&u8tou16.all, 1);
	ExternTimer1.OffHour = u8tou16.temp[1];
	read_dgus_vp(0x1087, (u8 *)&u8tou16.all, 1);
	ExternTimer1.OffMinute = u8tou16.temp[1];
	read_dgus_vp(0x4836, (u8 *)&u8tou16.all, 1);
	ExternTimer1.Region = u8tou16.temp[1];
	read_dgus_vp(0x4837, (u8 *)&u8tou16.all, 1);
	ExternTimer1.Mode = u8tou16.temp[1];
	ExternTimer1.Temp = ReadTimerTempC(VP_TIMER_IN_TEMP_C, VP_TIMER_IN_TEMP_F);
}

static u8 Timer_IsIndoorWeekdayMatch(u8 rtc_week)
{
	u8 bit_idx, week_en;
	u16 week_mask = 0;
	u8 i;

	bit_idx = (rtc_week == 0) ? 6 : (u8)(rtc_week - 1);
	for(i = 0; i < 7; i++)
	{
		read_dgus_vp((u32)(0x4510 + i), (u8 *)&week_en, 1);
		if(week_en)
			week_mask |= (u16)(1 << i);
	}
	if(week_mask == 0)
		return 1;
	return (week_mask & (u16)(1 << bit_idx)) ? 1 : 0;
}

static u8 Timer_IsExternalWeekdayMatch(u8 rtc_week)
{
	u8 bit_idx, week_en;
	u16 week_mask = 0;
	u8 i;

	bit_idx = (rtc_week == 0) ? 6 : (u8)(rtc_week - 1);
	for(i = 0; i < 7; i++)
	{
		read_dgus_vp((u32)(0x4500 + i), (u8 *)&week_en, 1);
		if(week_en)
			week_mask |= (u16)(1 << i);
	}
	if(week_mask == 0)
		return 1;
	return (week_mask & (u16)(1 << bit_idx)) ? 1 : 0;
}

static void Timer_ApplyIndoorOn(void)
{
	u8 i;
	u16 power, mode, temp, fan;

	mode = 0;
	read_dgus_vp((u32)VP_TIMER_IN_MODE, (u8 *)&mode, 1);
	read_dgus_vp((u32)(0x4838), (u8 *)&temp, 1);
	read_dgus_vp((u32)(0x3081), (u8 *)&fan, 1);
	for(i = 0; i < 6; i++)
	{
		u16 sel = 0;
		read_dgus_vp((u32)(0x4860 + i), (u8 *)&sel, 1);
		if(!sel)
			continue;
		power = 1;
		write_dgus_vp((u32)(0x4010 + i), (u8 *)&power, 1);
		write_dgus_vp((u32)(0x4700 + i), (u8 *)&mode, 1);
		write_dgus_vp((u32)(0x1020 + i), (u8 *)&temp, 1);
		write_dgus_vp((u32)(0x4710 + i), (u8 *)&fan, 1);
		App_ModbusTriggerIndoorUnitWrite(i, 0);
	}
	App_UploadIndoorUnitRequest();
}

static void Timer_ApplyIndoorOff(void)
{
	u8 i;
	u16 power = 0;

	for(i = 0; i < 6; i++)
	{
		u16 sel = 0;
		read_dgus_vp((u32)(0x4860 + i), (u8 *)&sel, 1);
		if(!sel)
			continue;
		write_dgus_vp((u32)(0x4010 + i), (u8 *)&power, 1);
		App_ModbusTriggerIndoorUnitWrite(i, 0);
	}
	App_UploadIndoorUnitRequest();
}

void	TimerRunning(void)
{
	unsigned char	Timer_ReadRtc[2];
	unsigned short	Timer_Power;
	u8 rtc_week;
	u16 time_key;
	static u16 s_last_ext_on = 0xFFFF;
	static u16 s_last_ext_off = 0xFFFF;
	static u16 s_last_in_on = 0xFFFF;
	static u16 s_last_in_off = 0xFFFF;
	
	read_dgus_vp((u32)(0x0012),(u8 *)&Timer_ReadRtc,1);//读当前时间F
	SyncTimerFromVp();
	rtc_week = rtc_get_weekday();
	time_key = (u16)(((u16)Timer_ReadRtc[0] << 8) | Timer_ReadRtc[1]);
	
	if(ExternTimer.On_Enable && Timer_IsExternalWeekdayMatch(rtc_week))
	{
		if((ExternTimer.OnHour==Timer_ReadRtc[0])&&(ExternTimer.OnMinute==Timer_ReadRtc[1]))
		{
			if(s_last_ext_on != time_key)
			{
				Timer_Power = TRUE;
				write_dgus_vp((u32)(0x4021),(u8*)&Timer_Power,1);
				write_dgus_vp((u32)(0x2002),(u8*)&ExternTimer.Mode,1);
				if(ExternTimer.Mode == 1)
					SetTempIntWrite(1, 0, ExternTimer.Temp);
				else if(ExternTimer.Mode == 2)
					SetTempIntWrite(2, 0, ExternTimer.Temp);
				else if(ExternTimer.Mode == 3)
					SetTempIntWrite(3, 0, ExternTimer.Temp);
				SyncSetTempCachesFromC(ExternTimer.Temp);
				App_ModbusTriggerUserWrite();
				Ready_To_Save_Report();
				App_UploadSwitch();
				App_UploadMode();
				App_UploadTempSet();
				s_last_ext_on = time_key;
			}
		}
		else if(s_last_ext_on == time_key)
			s_last_ext_on = 0xFFFF;
	}
	if(ExternTimer.Off_Enable && Timer_IsExternalWeekdayMatch(rtc_week))
	{
		if((ExternTimer.OffHour==Timer_ReadRtc[0])&&(ExternTimer.OffMinute==Timer_ReadRtc[1]))
		{
			if(s_last_ext_off != time_key)
			{
				Timer_Power = FALSE;
				write_dgus_vp((u32)(0x4021),(u8*)&Timer_Power,1);
				App_ModbusTriggerUserWrite();
				Ready_To_Save_Report();
				App_UploadSwitch();
				s_last_ext_off = time_key;
			}
		}
		else if(s_last_ext_off == time_key)
			s_last_ext_off = 0xFFFF;
	}
	if(ExternTimer1.On_Enable && Timer_IsIndoorWeekdayMatch(rtc_week))
	{
		if((ExternTimer1.OnHour==Timer_ReadRtc[0])&&(ExternTimer1.OnMinute==Timer_ReadRtc[1]))
		{
			if(s_last_in_on != time_key)
			{
				Timer_ApplyIndoorOn();
				Ready_To_Save_Report();
				App_UploadIndoorUnitRequest();
				s_last_in_on = time_key;
			}
		}
		else if(s_last_in_on == time_key)
			s_last_in_on = 0xFFFF;
	}
	if(ExternTimer1.Off_Enable && Timer_IsIndoorWeekdayMatch(rtc_week))
	{
		if((ExternTimer1.OffHour==Timer_ReadRtc[0])&&(ExternTimer1.OffMinute==Timer_ReadRtc[1]))
		{
			if(s_last_in_off != time_key)
			{
				Timer_ApplyIndoorOff();
				Ready_To_Save_Report();
				App_UploadIndoorUnitRequest();
				s_last_in_off = time_key;
			}
		}
		else if(s_last_in_off == time_key)
			s_last_in_off = 0xFFFF;
	}
	Defrost_CheckExit();
}
void	HomePage_Icon(void)
{
	unsigned short	icon_vp;
	u16 timer_en;
	u16 lock_st;
	u16 lock_disp;
	static u8 s_last_home_mode = 0xFF;
	u8 cur_mode;
	u8 wifi_st;
	
	read_dgus_vp((u32)(0x2010), (u8 *)&lock_st, 1);
	if(!LockKeyCountEnalbe)
	{
		lock_disp = lock_st ? 1 : 0;
		write_dgus_vp((u32)(0x2030), (u8 *)&lock_disp, 1);
	}
	
	wifi_st = mcu_get_wifi_work_state();
	if(wifi_st > 6)
		wifi_st = 0;
	icon_vp = (wifi_st == WIFI_CONNECTED || wifi_st == WIFI_CONN_CLOUD) ? 1 : 0;
	write_dgus_vp((u32)(0x3043),(u8*)&icon_vp,1);//主页WiFi图标：0隐藏 1显示
	
	timer_en = 0;
	read_dgus_vp((u32)(0x4821),(u8*)&icon_vp,1);
	if(icon_vp)
		timer_en = 1;
	if(!timer_en)
	{
		read_dgus_vp((u32)(0x4820),(u8*)&icon_vp,1);
		if(icon_vp)
			timer_en = 1;
	}
	if(!timer_en)
	{
		if(DgusVpIsEnabled(VP_TIMER_IN_ON_EN))
			timer_en = 1;
	}
	if(!timer_en)
	{
		if(DgusReadTimerOffEnable())
			timer_en = 1;
	}
	icon_vp = timer_en ? 1 : 0;
	write_dgus_vp((u32)(0x2004),(u8*)&icon_vp,1);//定时图标：0隐藏 1显示
	
	read_dgus_vp((u32)(0x2001),(u8*)&cur_mode,1);
	if(cur_mode != s_last_home_mode)
	{
		HomePage_UpdateModeAnimation(cur_mode);
		s_last_home_mode = cur_mode;
	}
	
	icon_vp = Defrosting ? 1 : 0;
	write_dgus_vp((u32)(0x3041),(u8*)&icon_vp,1);//除霜图标：0隐藏 1显示
	
}
void	Wifi_Resting_Pro(void)
{
	static unsigned char Wifi_Resting_Pro_Count = 0;
		
	if(!WiFi_Tuya_Reseting)
	{
		Wifi_Resting_Pro_Count = 0;
		return;
	}

	if(mcu_get_reset_wifi_flag())
	{
		Page_Change_Handler(23);
		WiFi_Tuya_Reseting = FALSE;
		Wifi_Resting_Pro_Count = 0;
		return;
	}

	Wifi_Resting_Pro_Count++;
	if(Wifi_Resting_Pro_Count >= 30)
	{
		Page_Change_Handler(21);
		WiFi_Tuya_Reseting = FALSE;
		Wifi_Resting_Pro_Count = 0;
	}
}
void	OTAReload_DelayPage(void)
{
	static unsigned char	OTAReloadDelayCount = 0;
	
	if(OTAReload_Delay)//OTA传输完成后F
	{
		if(OTAReloadDelayCount < 15)//等待15s
			OTAReloadDelayCount++;
		else
		{
			OTAReload_Delay = FALSE;
			OTAReloadDelayCount = 0;
			App_HomePage(TRUE);
			Screensaver_Count = 0;
		}
	}
}
void	ScreenSaver_Pro(void)
{
	static unsigned char	ScreenSaveType_Old;
	unsigned char	i,ScreenSaveType;
	unsigned short	ScreenSaver_Pro_SetMode,ScreenSaver_Pro_Power,ScreenSaver_Pro_Error,ScreenSaver_Pro_Temp,ScreenSaver_Pro_TempA;
	
	if((!TuyaOTAState)&&(!OTAReload_Delay))//OTA过程中和OTA完成后等待时,不进入屏幕保护F
	{
		if((Screensaver_Count >= SCREENSAVETIME) && (Screensaver_Enable))//无操作超时F
		{
			if(GetPageID() != 66)
			{
				BeforeScreenSavePage = GetPageID();//存入屏保前所在页面F
				Page_Change_Handler(66);//进入播放屏保页面F
			}
			read_dgus_vp((u32)(0x2004),(u8 *)&ScreenSaver_Pro_Power,1);
			read_dgus_vp((u32)(0x2001),(u8 *)&ScreenSaver_Pro_SetMode,1);
			read_dgus_vp((u32)(0x4607),(u8*)&ScreenSaver_Pro_Error,1);
			if(ScreenSaver_Pro_Error)//有故障F
				ScreenSaveType = 4;
			else if(!ScreenSaver_Pro_Power)//关机F
				ScreenSaveType = 0;
			else
			{
				switch(ScreenSaver_Pro_SetMode)
					{
					case 1:
						ScreenSaveType = 1;
					break;
					case 2:
						ScreenSaveType = 2;
					break;
					case 3:
					default:
						ScreenSaveType = 3;
					break;
					}
			}
			if(ScreenSaveType_Old != ScreenSaveType)//屏保样式有变F
			{
				for(i=0;i<5;i++)
				{
					if(ScreenSaveType != i)//对非新样式F
					{
						ScreenSaver_Pro_Temp = 0xFF00;
						ScreenSaver_Pro_TempA = 0;
//						write_dgus_vp((u32)(0x5000 + 0x10 * i),(u8*)&ScreenSaver_Pro_Temp,1);//对描述指针写FF00隐藏动画F
						write_dgus_vp((u32)(0x3060 + i * 2),(u8*)&ScreenSaver_Pro_TempA,1);//对变量地址写0停止播放F
					}
					else//对新样式F
					{
						ScreenSaver_Pro_Temp = i * 2 + 0x3060;
						ScreenSaver_Pro_TempA = 1;
//						write_dgus_vp((u32)(0x5000 + 0x10 * i),(u8*)&ScreenSaver_Pro_Temp,1);//对描述指针写变量地址显示动画F
						write_dgus_vp((u32)(ScreenSaver_Pro_Temp),(u8*)&ScreenSaver_Pro_TempA,1);//对变量地址写1开始播放F
					}
				}
				ScreenSaveType_Old = ScreenSaveType;
			}
		}
	}
}

