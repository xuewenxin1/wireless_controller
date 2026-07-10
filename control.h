#ifndef __CONTROL_H
#define __CONTROL_H


#include "sys.h"

#define VP_INDOOR_ROOM_C_BASE	0x1030U
#define VP_INDOOR_ROOM_F_BASE	0x1040U
#define VP_INDOOR_SET_F_BASE	0x1070U
#define VP_EXT_SETTEMP_FLOOR_C	0x4852U
#define VP_EXT_SETTEMP_FLOOR_F	0x4842U
#define VP_EXT_SETTEMP_DHW_C	0x4850U
#define VP_EXT_SETTEMP_DHW_F	0x4846U
#define VP_EXT_SETTEMP_POOL_C	0x4854U
#define VP_EXT_SETTEMP_POOL_F	0x4844U
#define VP_DEFROST_T3_C		0x1014U
#define VP_DEFROST_T3_F		0x101aU
#define VP_TIMER_EXT_TEMP_C	0x4824U
#define VP_TIMER_EXT_TEMP_F	0x4830U
#define VP_TIMER_IN_TEMP_C	0x4838U
#define VP_TIMER_IN_TEMP_F	0x4832U
#define VP_TIMER_IN_ON_EN	0x4825U
#define VP_TIMER_IN_OFF_EN	0x4827U
#define VP_TIMER_IN_OFF_EN_UI	0x4840U
#define VP_TIMER_IN_MODE	0x4837U
/* 各内机定时 Modbus 镜像(4870 起)，勿占用屏绑定 0x4840 */
#define INDOOR_UNIT_TIMER_VP_BASE	0x4870U


#define DHW_0F00                0x0F00        //??????
#define	SCREENSAVETIME	20*1000	//20s
typedef struct
{
    /* ???? */
    u8 On_Enable;        // ??????????
	
    /* ?? */
    u8 OnHour;
    u8 OnMinute;
	u8 Off_Enable;        // ??????????
    u8 OffHour;
    u8 OffMinute;

    /* ?? & ?? */
    u8 Mode;           // ?? / ?? / ????
    u8 Temp;           // ?????C ?? F????????

    /* ?????? */
    u8 Week;           // bit0~bit6 = ??~??
} EXTERN_TIMER_T;
typedef struct
{
    /* ???? */
    u8 On_Enable;        // ??????????
	
    /* ?? */
    u8 OnHour;
    u8 OnMinute;
	u8 Off_Enable;        // ??????????
    u8 OffHour;
    u8 OffMinute;

    /* ?? & ?? */
	u8 Region;
    u8 Mode;           // ?? / ?? / ????
//	u8 Fan;
    u8 Temp;           // ?????C ?? F????????

    /* ?????? */
    u8 Week;           // bit0~bit6 = ??~??
} EXTERN_TIMER_T1;

extern u16 key_dowm;
extern u8 set_send_type;


//extern u8 Check_Data[100];
extern u16 getDar1;
extern u16 APP_down_delay;


extern	bit	LockKeyCountEnalbe;
extern	u16	LockKeyCount;
extern	bit	TimerEnable;
extern	u8	TimerSet[4];
extern	u16	Language_Num;
extern	u8	SetTempOld_HeatC,SetTempOld_HeatF,SetTempOld_CoolC,SetTempOld_CoolF,SetTempOld_AutoC,SetTempOld_AutoF;
extern	u8	SetTemp_CoolLower_C,SetTemp_CoolUpper_C,SetTemp_HeatLower_C,SetTemp_HeatUpper_C,SetTemp_CoolLower_F,SetTemp_CoolUpper_F,SetTemp_HeatLower_F,SetTemp_HeatUpper_F;
extern	u8	SetPara_Addr;
extern	u16	Defrosting;
extern	u16 Electric_Heating;
extern	bit	WiFi_Tuya_Reseting;
extern	bit	OTAReload_Delay;
extern	bit	Screensaver_Enable;
extern	u16	Screensaver_Count;

char GetValue0F00(void);
void Control_TouchPollStep(void);
void Control_IntentStep(void);
void Control_IndoorSwitchPollStep(void);
void Control_IndoorSwitchSyncFromVp(void);
void Control_Function1(void);
u16 GetPageID(void);
void	PowerOnOffSwitch(void);
bit	IsItHomePage(unsigned char NowPage);
void	HomePage(bit TurnPage);
void	HomePage_SyncIndoorDisplay(void);
void	Basefunction_Init(void);
void	Temp_Limit(unsigned short Addr,unsigned char TempType);
void	SetModeProcess(unsigned short	NewMode);
void	RTCdisplay4change(void);
void	Ready4WriteRTC(void);
signed short	TempUnitTrans(signed short temp,unsigned char type);
void	UnitChangePro(void);
void	SaveSetTemperature(void);
void	Timer_EnableOrDisable(void);
void	TimerDisplay(void);
void	TimerRunning(void);
void	HomePage_Icon(void);
void	Control_LockKeyService(void);
void	Control_SanitizeSetTempIntVp(void);
u16 Control_ClampSetTempCByMode(u8 mode, u16 temp_c);
void SyncSetTempCachesFromC(u16 temp_c);
void	Control_ClearDefrostSession(void);
void	Wifi_Resting_Pro(void);
void	OTAReload_DelayPage(void);
void	ScreenSaver_Pro(void);
void	Control_ScreensaverInit(void);
void	Control_MirrorExtSetTempDisplay(u8 mode);
void	Control_MirrorAllExtSetTempDisplay(void);
u32 SetTempIntVpByMode(u8 mode, u8 unit_f);
u32 SetTempFloatVpCByMode(u8 mode);
u32 SetTempFloatVpFByMode(u8 mode);
u16 SetTempIntRead(u8 mode, u8 unit_f);
void SetTempIntWrite(u8 mode, u8 unit_f, u16 temp);
void SetTempFloatWriteC(u8 mode, float temp_c);
void TempUnit_RefreshAll(void);
#endif

