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
void Modbus_TriggerDefrostParamWrite(u16 vp);
void HomePage_UpdateModeAnimation(u8 mode);
void Modbus_RefreshCheckParamDisplay(void);
void SyncSetTempCachesFromC(u16 temp_c);

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
#define VP_INT_C_BASE		0x4808U
#define VP_INT_F_BASE		0x4818U

u32 SetTempIntVpByMode(u8 mode, u8 unit_f)
{
	if(mode < 1 || mode > 3)
		return 0;
	if(unit_f)
		return VP_INT_F_BASE + (u32)(mode - 1) * 2;
	return VP_INT_C_BASE + (u32)(mode - 1) * 2;
}

u16	Defrosting = FALSE;
static u32 s_defrost_start_sec = 0;

static void PrefillSetTempEditPage(void)
{
	u16 mode;
	u16 unit;
	u16 temp_c;
	u16 temp_f;

	read_dgus_vp((u32)(0x2002), (u8 *)&mode, 1);
	temp_c = SetTempIntRead((u8)mode, 0);
	temp_f = SetTempIntRead((u8)mode, 1);
	if(!temp_f && temp_c)
		temp_f = (u16)TempUnitTrans((signed short)temp_c, 'F');
	if(!temp_c && temp_f)
		temp_c = (u16)TempUnitTrans((signed short)temp_f, 'C');
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
	if(evap_c > t3_c)
		goto defrost_exit;
	return;

defrost_exit:
	{
		u16 zero = 0;

		Defrosting = FALSE;
		s_defrost_start_sec = 0;
		write_dgus_vp((u32)(0x201b), (u8 *)&zero, 1);
		mcu_dp_bool_update(DPID_DEFROST, 0);
		Modbus_Write_Ueser = TRUE;
	}
}

u16 SetTempIntRead(u8 mode, u8 unit_f)
{
	u16 temp = 0;
	u32 vp = SetTempIntVpByMode(mode, unit_f);

	if(vp)
		read_dgus_vp(vp, (u8 *)&temp, 1);
	return temp;
}

void SetTempIntWrite(u8 mode, u8 unit_f, u16 temp)
{
	u32 vp = SetTempIntVpByMode(mode, unit_f);

	if(vp)
		write_dgus_vp(vp, (u8 *)&temp, 1);
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

static void SyncIndoorRoomTempVpPair(u8 index, u16 temp_c)
{
	u16 temp_f;

	if(index >= 6)
		return;
	temp_f = (u16)TempUnitTrans((signed short)temp_c, 'F');
	write_dgus_vp((u32)(VP_INDOOR_ROOM_C_BASE + (u16)index * 2), (u8 *)&temp_c, 1);
	write_dgus_vp((u32)(VP_INDOOR_ROOM_F_BASE + (u16)index * 2), (u8 *)&temp_f, 1);
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

static void KeyAdjustSetTempForMode(u8 mode, s16 delta)
{
	u16 unit;
	u16 temp;
	u32 vp;

	read_dgus_vp((u32)0x2003, (u8 *)&unit, 1);
	vp = SetTempIntVpByMode(mode, (u8)unit);
	if(!vp)
		return;
	read_dgus_vp(vp, (u8 *)&temp, 1);
	temp = (u16)((s16)temp + delta);
	write_dgus_vp(vp, (u8 *)&temp, 1);
	Temp_Limit((unsigned short)vp, unit ? 'F' : 'C');
	read_dgus_vp(vp, (u8 *)&temp, 1);
	if(unit)
		SyncSetTempCachesFromC((u16)TempUnitTrans((signed short)temp, 'C'));
	else
		SyncSetTempCachesFromC(temp);
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

#define WEEK_MON    (1<<0)
#define WEEK_TUE    (1<<1)
#define WEEK_WED    (1<<2)
#define WEEK_THU    (1<<3)
#define WEEK_FRI    (1<<4)
#define WEEK_SAT    (1<<5)
#define WEEK_SUN    (1<<6)

u16 key_down_delay = 0;	
u16 APP_down_delay = 0;	

bit	LockKeyCountEnalbe;
u16	LockKeyCount = 0;
u8	SetPara_Addr;
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
bit	Screensaver_Enable = FALSE;
u16	Screensaver_Count = 0;
u8	BeforeScreenSavePage = 0;

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
u16 GetPageID(void)
{
	u16 usPID;
	read_dgus_vp(PIC_NOW,(u8 *)&usPID,1);
	return usPID;
}

void Control_Function1(void)
{
	unsigned short	LockFlag;
	unsigned short	Control_u16temp;
	signed short	Control_s16temp;
	u8 i;
	read_dgus_vp((u32)(0x2010),(u8 *)&LockFlag,1);
	key_dowm = GetValue0F00();	//按键点击返回
	if(key_dowm)
	{
		Screensaver_Count = 0;
		switch(getDar1)
		{
			case	0x2000://返回主页F
				HomePage(TRUE);
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
					HomePage(TRUE);
				else
					Page_Change_Handler(BeforeScreenSavePage);//打断屏保回到原有界面F
			break;
			case 0x3224:
				read_dgus_vp((u32)(0x3224),(u8 *)&Control_u16temp,1);
				if(Control_u16temp == 1 || Control_u16temp == 2)
				{
					ErrorHistory_PageChange(Control_u16temp);
					Control_u16temp = 0;
					write_dgus_vp((u32)(0x3224), (u8 *)&Control_u16temp, 1);
				}
			break;
			case	0x2001://内机模式F
				Modbus_Write_Ueser = TRUE;
				Ready_To_Save_Report();
			break;
			case	0x2002:{//外机模式F
				u16 unit;
				u16 temp_c;
				u16 power_mode;
				Modbus_Write_Ueser = TRUE;
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
				read_dgus_vp((u32)(0x2003),(u8 *)&unit,1);
				if(Control_u16temp == 1)
				{
					temp_c = SetTempIntRead(1, (u8)unit);
					if(!temp_c)
						temp_c = SetTempIntRead(1, 0);
					if(temp_c)
						SyncSetTempCachesFromC(temp_c);
				}
				else if(Control_u16temp == 2)
				{
					temp_c = SetTempIntRead(2, (u8)unit);
					if(!temp_c)
						temp_c = SetTempIntRead(2, 0);
					if(temp_c)
						SyncSetTempCachesFromC(temp_c);
				}
				else if(Control_u16temp == 3)
				{
					temp_c = SetTempIntRead(3, (u8)unit);
					if(!temp_c)
						temp_c = SetTempIntRead(3, 0);
					if(temp_c)
						SyncSetTempCachesFromC(temp_c);
				}
				Ready_To_Save_Report();
			}
			break;
			case	0x2003://温标F
				UnitChangePro();
				HomePage(TRUE);
				Ready_To_Save_Report();
			break;
			case 0x4021:{
				u16 mode;
				read_dgus_vp((u32)(0x4021),(u8 *)&Control_u16temp,1);
				if(Control_u16temp > 1)
				{
					break;
				}
				read_dgus_vp((u32)(0x2002),(u8 *)&mode,1);
				if(Control_u16temp == 0)
				{
					mode = 0;
					write_dgus_vp((u32)(0x2002),(u8 *)&mode,1);
				}
				else if(mode == 0)
				{
					mode = 1;
					write_dgus_vp((u32)(0x2002),(u8 *)&mode,1);
				}
				Modbus_Write_Ueser = TRUE;
				Ready_To_Save_Report();
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
				upload_inside_time_open();
			break;
			case 0x3084:
				upload_external_time_open();
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
				upload_indoor_unit();
				if(unit_sel >= 1 && unit_sel <= 6)
					Modbus_TriggerIndoorUnitWrite((u8)(unit_sel - 1), 0);
			}
			break;
			case 0x4850:
				read_dgus_vp((u32)(0x2002),(u8 *)&Control_u16temp,1);
				if(Control_u16temp == 1)
					KeyAdjustSetTempForMode(1, 1);
			break;
			case 0x4851:
				read_dgus_vp((u32)(0x2002),(u8*)&Control_u16temp,1);
				if(Control_u16temp == 1)
					KeyAdjustSetTempForMode(1, -1);
			break;
			case 0x4852:
				read_dgus_vp((u32)(0x2002),(u8*)&Control_u16temp,1);
				if(Control_u16temp == 2)
					KeyAdjustSetTempForMode(2, -1);
			break;
			case 0x4853:
				read_dgus_vp((u32)(0x2002),(u8*)&Control_u16temp,1);
				if(Control_u16temp == 2)
					KeyAdjustSetTempForMode(2, 1);
			break;
			case 0x4854:
				read_dgus_vp((u32)(0x2002),(u8*)&Control_u16temp,1);
				if(Control_u16temp == 3)
					KeyAdjustSetTempForMode(3, 1);
			break;
			case 0x4855:
				read_dgus_vp((u32)(0x2002),(u8*)&Control_u16temp,1);
				if(Control_u16temp == 3)
					KeyAdjustSetTempForMode(3, -1);
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
					mcu_dp_value_update(DPID_TEMP_SET,SaveSetTemp_u16tempC);
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
					mcu_dp_value_update(DPID_TEMP_SET,SaveSetTemp_u16tempC);
					SetTempIntWrite((u8)save_mode, 0, SaveSetTemp_u16tempC);
					SetTempIntWrite((u8)save_mode, 1, SaveSetTemp_u16temp);
					SetTempFloatWriteC((u8)save_mode, (float)SaveSetTemp_u16tempC);
					Page_Change_Handler(30);
				}
				Modbus_Write_Ueser = TRUE;
				Ready_To_Save_Report();
			}
			break;
			case	0x2021://设定模式F
				read_dgus_vp((u32)(0x2021),(u8 *)&Control_u16temp,1);
				SetModeProcess(Control_u16temp);
				Modbus_Write_Ueser = TRUE;
				Ready_To_Save_Report();
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
				read_dgus_vp((u32)(0x2018),(u8 *)&Control_u16temp,1);
				Language_Num = Control_u16temp;
				Language_Change((unsigned char)Control_u16temp);
				Ready_To_Save_Report();
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
				Modbus_StartManualDefrost();
				mcu_dp_bool_update(DPID_DEFROST, 1);
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
				mcu_dp_enum_update(DPID_RELAY_STATUS,Enable_Memory);
				write_dgus_vp((u32)(0x3508),(u8 *)&Enable_Memory,1);
				ErrorHistory_TryMigrateFlash();
				Ready_To_Save_Report();
			}
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
						read_dgus_vp((u32)(0x3051),(u8 *)&Control_u16temp,1); 
						write_dgus_vp((u32)(0x1081),(u8 *)&Control_u16temp,1); 
						upload_external_time_open();
					}
					Page_Change_Handler(12);
				}
				else if(Control_u16temp == 2){
					read_dgus_vp((u32)(0x1019),(u8 *)&Control_u16temp,1); 
					if(Control_u16temp == 1)
					{
						read_dgus_vp((u32)(0x3050),(u8 *)&Control_u16temp,1); 
						write_dgus_vp((u32)(0x1082),(u8 *)&Control_u16temp,1); 
						read_dgus_vp((u32)(0x3051),(u8 *)&Control_u16temp,1); 
						write_dgus_vp((u32)(0x1083),(u8 *)&Control_u16temp,1); 
						upload_external_time_close();
					}
					Page_Change_Handler(11);
				}
				else if(Control_u16temp == 3){
					read_dgus_vp((u32)(0x1019),(u8 *)&Control_u16temp,1); 
					if(Control_u16temp == 1)
					{
						read_dgus_vp((u32)(0x3050),(u8 *)&Control_u16temp,1); 
						write_dgus_vp((u32)(0x1084),(u8 *)&Control_u16temp,1); 
						read_dgus_vp((u32)(0x3051),(u8 *)&Control_u16temp,1); 
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
						read_dgus_vp((u32)(0x3051),(u8 *)&Control_u16temp,1); 
						write_dgus_vp((u32)(0x1087),(u8 *)&Control_u16temp,1);
						upload_inside_time_close();						
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
			break;
			case	0x2030://2030、2031、2032长按童锁
				read_dgus_vp((u32)(0x2030),(u8 *)&Control_u16temp,1);
				if(Control_u16temp)
				{
					if(LockKeyCount >= 2000)
					{
						read_dgus_vp((u32)(0x2010),(u8 *)&Control_u16temp,1);
						Control_u16temp = (Control_u16temp) ? (FALSE) : (TRUE);
						write_dgus_vp((u32)(0x2010),(u8*)&Control_u16temp,1);
						mcu_dp_bool_update(DPID_CHILD_LOCK,Control_u16temp); 
					}
					LockKeyCountEnalbe = FALSE;
					LockKeyCount = 0;
				}
				else
				{
					LockKeyCountEnalbe = TRUE;
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
				upload_indoor_unit();
				Modbus_TriggerIndoorUnitWrite(0, 1);
				Modbus_TriggerGroupControlWrite();
				Ready_To_Save_Report();
				Page_Change_Handler(2);
			}
			break;
			case 0x3090:
				read_dgus_vp((u32)(0x3090),(u8*)&Control_u16temp,1);
				write_dgus_vp((u32)(0x2002),(u8*)&Control_u16temp,1);
				Modbus_Write_Ueser = TRUE;
				Ready_To_Save_Report();
			break;
			case	0x2035://历史故障
				ErrorHistory_ResetPage();
				Page_Change_Handler(45);
				ErrorHistoryDisplay();
			break;
			case	0x2036://确认是否清空历史故障F
				read_dgus_vp((u32)(0x2036),(u8 *)&Control_u16temp,1);
				if(Control_u16temp == 1)
				{
					ClearErrorHistory();
					ErrorHistoryDisplay();
					Control_u16temp = 0;
					write_dgus_vp((u32)(0x2036),(u8 *)&Control_u16temp,1);
				}
			break;
			case 0x3660:
			case 0x3670:
			case 0x3680:
			case VP_DEFROST_T3_C:
			case VP_DEFROST_T3_F:
				Modbus_TriggerDefrostParamWrite(getDar1);
				Ready_To_Save_Report();
			break;
			case 0x4010:
			case 0x4011:
			case 0x4012:
			case 0x4013:
			case 0x4014:
			case 0x4015:
				if(getDar1 >= 0x4010)
					Modbus_TriggerIndoorUnitWrite((u8)(getDar1 - 0x4010), 0);
				Ready_To_Save_Report();
				upload_indoor_unit();
			break;
			case 0x4700:
			case 0x4701:
			case 0x4702:
			case 0x4703:
			case 0x4704:
			case 0x4705:
				if(getDar1 >= 0x4700)
					Modbus_TriggerIndoorUnitWrite((u8)(getDar1 - 0x4700), 0);
				Ready_To_Save_Report();
				upload_indoor_unit();
			break;
			case 0x4710:
			case 0x4711:
			case 0x4712:
			case 0x4713:
			case 0x4714:
			case 0x4715:
				if(getDar1 >= 0x4710)
					Modbus_TriggerIndoorUnitWrite((u8)(getDar1 - 0x4710), 0);
				Ready_To_Save_Report();
				upload_indoor_unit();
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
					Modbus_TriggerIndoorUnitWrite((u8)(getDar1 - 0x1020), 0);
				Ready_To_Save_Report();
				upload_indoor_unit();
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
				Modbus_TriggerIndoorUnitWrite(idx, 0);
				Ready_To_Save_Report();
				upload_indoor_unit();
			}
			break;
			case	0x2037://resetwifi
				printf("[WIFI] reset req\r\n");
				mcu_reset_wifi();
				Page_Change_Handler(22);
				WiFi_Tuya_Reseting = TRUE;
			break;
//			case 0x4826:
//				
//			break;
			case 0x4821:
			case 0x4826:
			case 0x4824:
				upload_external_time_open();
				Ready_To_Save_Report();
			break;
			case VP_TIMER_EXT_TEMP_F:
			{
				u16 temp_f;
				u16 temp_c;

				read_dgus_vp((u32)getDar1, (u8 *)&temp_f, 1);
				temp_c = (u16)TempUnitTrans((signed short)temp_f, 'C');
				write_dgus_vp(VP_TIMER_EXT_TEMP_C, (u8 *)&temp_c, 1);
				upload_external_time_open();
				Ready_To_Save_Report();
			}
			break;
			case 0x4825:
				Modbus_ApplyIndoorTimerVpToUnits(1, 0);
				upload_inside_time_open();
				Ready_To_Save_Report();
			break;
			case VP_TIMER_IN_TEMP_F:
			{
				u16 temp_f;
				u16 temp_c;

				read_dgus_vp((u32)getDar1, (u8 *)&temp_f, 1);
				temp_c = (u16)TempUnitTrans((signed short)temp_f, 'C');
				write_dgus_vp(VP_TIMER_IN_TEMP_C, (u8 *)&temp_c, 1);
				Modbus_ApplyIndoorTimerVpToUnits(1, 0);
				upload_inside_time_open();
				Ready_To_Save_Report();
			}
			break;
			case 0x4827:
				Modbus_ApplyIndoorTimerVpToUnits(0, 1);
				upload_inside_time_close();
				Ready_To_Save_Report();
			break;
			case 0x4820:
				upload_external_time_close();
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
				read_dgus_vp((u32)(0x2030),(u8 *)&Control_u16temp,1);
				if(Control_u16temp)
					Page_Change_Handler(61);
				else 
					Page_Change_Handler(52);
			break;
		}
		if((getDar1 >= 0x4800)&&(getDar1 <= (0x4800+SET_PARAMETER_NUM-1)))
		{
			unsigned char para_idx;

			if(getDar1 == 0x4800 || getDar1 == 0x4802 || getDar1 == 0x4804 || getDar1 == 0x4806
			|| getDar1 == 0x4810 || getDar1 == 0x4812 || getDar1 == 0x4814 || getDar1 == 0x4816)
				;
			else if(getDar1 == 0x4808 || getDar1 == 0x480a || getDar1 == 0x480c)
			{
				Temp_Limit(getDar1, 'C');
				Ready_To_Save_Report();
			}
			else if(getDar1 == 0x4818 || getDar1 == 0x481a || getDar1 == 0x481c)
			{
				Temp_Limit(getDar1, 'F');
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
			SetPara_Addr = para_idx;
			Modbus_Write_Set = TRUE;
			Send_Count = 200;
			Ready_To_Save_Report();
			}
		}
	}
}
//void	PowerOnOffSwitch(void)
//{
//	unsigned short	PowerOnOff_Power;
//	
//	read_dgus_vp((u32)(0x2004),(u8 *)&PowerOnOff_Power,1);
//	if(!PowerOnOff_Power)
//		PowerOnOff_Power = 1;
//	else
//		PowerOnOff_Power = 0;
//	write_dgus_vp((u32)(0x2004),(u8*)&PowerOnOff_Power,1);
//}
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
		read_dgus_vp((u32)(0x1101 + (u16)first_one * 0x20),(u8 *)&HomePage_Power,1);
		if(HomePage_Power > 4)
		{
			read_dgus_vp((u32)(0x4700 + first_one),(u8 *)&HomePage_Power,1);
			if(HomePage_Power > 4)
				HomePage_Power = 0;
		}
	}
	write_dgus_vp((u32)(0x2001),(u8 *)&HomePage_Power,1);
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
		HomePage_SyncIndoorDisplay();
		return;
	}

	read_dgus_vp((u32)(0x2003),(u8 *)&HomePage_TempType,1);
	if(!HomePage_TempType)
		Page_Change_Handler(1);
	else
		Page_Change_Handler(30);
	HomePage_SyncIndoorDisplay();
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
	else if(Addr == 0x4808)
	{
		lo = 30;
		hi = 60;
	}
	else if(Addr == 0x480a)
	{
		lo = 30;
		hi = 50;
	}
	else if(Addr == 0x480c)
	{
		lo = 15;
		hi = 40;
	}
	else if(Addr == 0x4818)
	{
		lo = (unsigned short)TempUnitTrans(30, 'F');
		hi = (unsigned short)TempUnitTrans(60, 'F');
	}
	else if(Addr == 0x481a)
	{
		lo = (unsigned short)TempUnitTrans(30, 'F');
		hi = (unsigned short)TempUnitTrans(50, 'F');
	}
	else if(Addr == 0x481c)
	{
		lo = (unsigned short)TempUnitTrans(15, 'F');
		hi = (unsigned short)TempUnitTrans(40, 'F');
	}
	else
		return;
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
	else if(Addr == 0x4808 || Addr == 0x480a || Addr == 0x480c)
		SetTempIntWrite((u8)((Addr - 0x4808) / 2 + 1), 1,
			(u16)TempUnitTrans((signed short)Temp_Limit_Temp, 'F'));
	else if(Addr == 0x4818 || Addr == 0x481a || Addr == 0x481c)
		SetTempIntWrite((u8)((Addr - 0x4818) / 2 + 1), 0,
			(u16)TempUnitTrans((signed short)Temp_Limit_Temp, 'C'));
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
	RTC_WriteDisplayTime(year_full, month, day, hour, minute);
}

void SyncSetTempCachesFromC(u16 temp_c)
{
	u16 temp_f;
	u16 mode;

	temp_f = (u16)TempUnitTrans((signed short)temp_c, 'F');
	write_dgus_vp((u32)0x2005, (u8 *)&temp_c, 1);
	write_dgus_vp((u32)0x2006, (u8 *)&temp_f, 1);
	read_dgus_vp((u32)0x2002, (u8 *)&mode, 1);
	if(mode >= 1 && mode <= 3)
	{
		SetTempIntWrite((u8)mode, 0, temp_c);
		SetTempIntWrite((u8)mode, 1, temp_f);
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
	temp_c = SetTempIntRead((u8)mode, (u8)unit);
	if(unit && temp_c)
		temp_c = (u16)TempUnitTrans((signed short)temp_c, 'C');
	if(temp_c)
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
		read_dgus_vp((u32)(0x1020 + i), (u8 *)&temp_c, 1);
		SyncIndoorSetTempVpPair(i, temp_c);
		read_dgus_vp((u32)(VP_INDOOR_ROOM_C_BASE + (u16)i * 2), (u8 *)&temp_c, 1);
		if(temp_c)
			SyncIndoorRoomTempVpPair(i, temp_c);
		WDT_RST();
	}

	read_dgus_vp((u32)VP_TIMER_EXT_TEMP_C, (u8 *)&temp_c, 1);
	if(temp_c)
		SyncTimerTempVpPair(VP_TIMER_EXT_TEMP_C, VP_TIMER_EXT_TEMP_F, temp_c);
	read_dgus_vp((u32)VP_TIMER_IN_TEMP_C, (u8 *)&temp_c, 1);
	if(temp_c)
		SyncTimerTempVpPair(VP_TIMER_IN_TEMP_C, VP_TIMER_IN_TEMP_F, temp_c);

	Modbus_RefreshCheckParamDisplay();
}

signed short TempUnitTrans(signed short temp, unsigned char type)
{
	signed short Cache;

	if (type == 'C') // C=(F-32)*5/9
	{
		if (temp < -22) //-623
			temp = -22;
		else if (temp > 266) // 687
			temp = 266;
		Cache = (temp - 32) * 10 * 5 / 9;
	}
	else if (type == 'F') // F=C*9/5+32
	{
		if (temp < -30) //-364
			temp = -30;
		else if (temp > 130) // 364
			temp = 130;
		Cache = temp * 10 * 9 / 5 + 320;
	}

	Cache = (Cache > 0) ? ((Cache + 5) / 10) : ((Cache - 5) / 10);
	return Cache;
}
void	UnitChangePro(void)
{
	unsigned short	UnitChange_TempType = 0;
	
	read_dgus_vp((u32)(0x2003),(u8 *)&UnitChange_TempType,1);
	if(!UnitChange_TempType)
		mcu_dp_enum_update(DPID_TEMP_UNIT_CONVERT,0);
	else
		mcu_dp_enum_update(DPID_TEMP_UNIT_CONVERT,1);
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

static u8 Timer_IsWeekdayMatch(u8 rtc_week)
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

static void Timer_ApplyIndoorOn(void)
{
	u8 i;
	u16 power, mode, temp, fan;

	read_dgus_vp((u32)(0x4837), (u8 *)&mode, 1);
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
		Modbus_TriggerIndoorUnitWrite(i, 0);
	}
	upload_indoor_unit();
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
		Modbus_TriggerIndoorUnitWrite(i, 0);
	}
	upload_indoor_unit();
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
	
	if(ExternTimer.On_Enable && Timer_IsWeekdayMatch(rtc_week))
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
				Modbus_Write_Ueser = TRUE;
				Ready_To_Save_Report();
				s_last_ext_on = time_key;
			}
		}
		else if(s_last_ext_on == time_key)
			s_last_ext_on = 0xFFFF;
	}
	if(ExternTimer.Off_Enable && Timer_IsWeekdayMatch(rtc_week))
	{
		if((ExternTimer.OffHour==Timer_ReadRtc[0])&&(ExternTimer.OffMinute==Timer_ReadRtc[1]))
		{
			if(s_last_ext_off != time_key)
			{
				Timer_Power = FALSE;
				write_dgus_vp((u32)(0x4021),(u8*)&Timer_Power,1);
				Modbus_Write_Ueser = TRUE;
				Ready_To_Save_Report();
				s_last_ext_off = time_key;
			}
		}
		else if(s_last_ext_off == time_key)
			s_last_ext_off = 0xFFFF;
	}
	if(ExternTimer1.On_Enable && Timer_IsWeekdayMatch(rtc_week))
	{
		if((ExternTimer1.OnHour==Timer_ReadRtc[0])&&(ExternTimer1.OnMinute==Timer_ReadRtc[1]))
		{
			if(s_last_in_on != time_key)
			{
				Timer_ApplyIndoorOn();
				Ready_To_Save_Report();
				s_last_in_on = time_key;
			}
		}
		else if(s_last_in_on == time_key)
			s_last_in_on = 0xFFFF;
	}
	if(ExternTimer1.Off_Enable && Timer_IsWeekdayMatch(rtc_week))
	{
		if((ExternTimer1.OffHour==Timer_ReadRtc[0])&&(ExternTimer1.OffMinute==Timer_ReadRtc[1]))
		{
			if(s_last_in_off != time_key)
			{
				Timer_ApplyIndoorOff();
				Ready_To_Save_Report();
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
	unsigned short	HomePage_Icon_u16temp;
	static u8 s_last_home_mode = 0xFF;
	static u8 s_last_wifi_st = 0xFF;
	u8 cur_mode;
	u8 wifi_st;
	
	wifi_st = mcu_get_wifi_work_state();
	if(wifi_st <= 6 && wifi_st != s_last_wifi_st)
	{
		printf("[WIFI] state=%bu\r\n", wifi_st);
		s_last_wifi_st = wifi_st;
	}
	if(wifi_st > 6)
		wifi_st = 0;
	switch(wifi_st)
	{
		case	WIFI_CONNECTED:
			HomePage_Icon_u16temp = TRUE;
		break;
		default:
			HomePage_Icon_u16temp = FALSE;
		break;
	}
	write_dgus_vp((u32)(0x3043),(u8*)&HomePage_Icon_u16temp,1);//主页WiFi图标F
	
	HomePage_Icon_u16temp = FALSE;
	read_dgus_vp((u32)(0x4821),(u8*)&HomePage_Icon_u16temp,1);
	if(!HomePage_Icon_u16temp)
		read_dgus_vp((u32)(0x4820),(u8*)&HomePage_Icon_u16temp,1);
	if(!HomePage_Icon_u16temp)
		read_dgus_vp((u32)(0x4825),(u8*)&HomePage_Icon_u16temp,1);
	if(!HomePage_Icon_u16temp)
		read_dgus_vp((u32)(0x4827),(u8*)&HomePage_Icon_u16temp,1);
	write_dgus_vp((u32)(0x2004),(u8*)&HomePage_Icon_u16temp,1);//定时图标
	
	read_dgus_vp((u32)(0x2001),(u8*)&cur_mode,1);
	if(cur_mode != s_last_home_mode)
	{
		HomePage_UpdateModeAnimation(cur_mode);
		s_last_home_mode = cur_mode;
	}
	
	write_dgus_vp((u32)(0x3041),(u8*)&Defrosting,1);//主页除霜运行图标F
	
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
		printf("[WIFI] reset ok\r\n");
		Page_Change_Handler(23);
		WiFi_Tuya_Reseting = FALSE;
		Wifi_Resting_Pro_Count = 0;
		return;
	}

	Wifi_Resting_Pro_Count++;
	if(Wifi_Resting_Pro_Count >= 30)
	{
		printf("[WIFI] reset timeout\r\n");
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
			HomePage(TRUE);
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

