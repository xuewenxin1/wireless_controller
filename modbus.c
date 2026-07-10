/******************************************************************************

                  版权所有 (C), 2019, 北京迪文科技有限公司

 ******************************************************************************
  文 件 名   : modbus.c
  版 本 号   : V1.0
  作    者   : chengjing
  生成日期   : 2019年7月31日
  功能描述   : modbus salve程序，有0x03,0x06和0x10码功能，协议和T5的modubs salve一
			  样,读写dgus vp地址的值。
  修改历史   :
  1.日    期   : 
    作    者   : 
    修改内容   : 
******************************************************************************/

#include "modbus.h"
#include "uart.h"
#include "control.h"
#include "rtc.h"
#include "wifi.h"
#include "sys.h"
#include "ErrorHistory.h"
#include "upload.h"
#include "app_core.h"
#include "app_share.h"
#include <stdio.h>
extern u16 Defrosting;
extern void SyncSetTempCachesFromC(u16 temp_c);
extern u16 Control_ClampSetTempCByMode(u8 mode, u16 temp_c);
extern void Control_ClearDefrostSession(void);

#ifndef MODBUS_RSP_TIMEOUT_MS
#define MODBUS_RSP_TIMEOUT_MS  800
#endif
#ifndef MODBUS_SKIP_RD12_POLL
#define MODBUS_SKIP_RD12_POLL  0
#endif
#ifndef MODBUS_SKIP_RD1000_POLL
#define MODBUS_SKIP_RD1000_POLL  0
#endif

/********************************
*  0：没有接收数据
*  0xA5:开始接收数据
*  0x5A00:正常接收数据
*  0x5AA5:modbus数据接收完成
********************************/
u8	Modbus_Buffer[MODBUS_BUFFER_LEN] = 0;

bit	Modbus_Read_Initial = TRUE;
bit	Modbus_Read_UpperLimit,Modbus_Read_LowerLimit,Modbus_Read_Check,Modbus_Read_Check_2,Modbus_Read_Check_3,Modbus_Read_Check_4,Modbus_Read_Check_5,Modbus_Read_User,Modbus_Read_Set,Modbus_Write_Ueser,Modbus_Read_Check_4_old,Modbus_Write_5,Modbus_Write_6,Modbus_Write_6_All;
bit	Modbus_Read_Error,Modbus_Write_Defrost;
bit	Modbus_Write_DefrostParam = 0;
bit	Modbus_Write_Timer = 0;

u8 Modbus_DefrostRetryLeft = 0;
u8 Indoor_Unit_Index = 0;
static u8 s_defrost_want = 0;
static u8 s_defrost_verify = 0;
static u16 s_defrost_param_vp = 0;
static u8 Modbus_Timer_Unit_Index = 0;
static u8 s_timer_q[INDOOR_UNIT_NUM];
static u8 s_timer_q_len = 0;
static u8 s_timer_q_pos = 0;
static bit Modbus_Write_Timer_Pending = 0;
static bit Modbus_Write_Ueser_Pending = 0;
static bit Modbus_Write_6_Pending = 0;
#define MODBUS_USER_HOLD_INIT 2000
#define MODBUS_SETTEMP_HOLD_INIT 50000
static u16 s_user_power_hold = 0;
static u16 s_user_hold_pwr = 0;
static u16 s_user_hold_mode = 0;
static u16 s_user_hold_stemp[4];
static bit s_user_settemp_dirty = 0;
static u32 s_user_stemp_hold[4];
static u16 s_indoor_pwr_hold[INDOOR_UNIT_NUM];
static u8 s_indoor_hold_pwr[INDOOR_UNIT_NUM];
static u8 s_mb_unit_on[INDOOR_UNIT_NUM];
static u8 s_indoor_seen[INDOOR_UNIT_NUM];
static u8 s_indoor_installed = INDOOR_UNIT_NUM;
static u16 s_reg214_cfg = INDOOR_UNIT_NUM;
static u8 s_indoor_wr_pending_mask = 0;
static u8 s_indoor_wr_unit = 0;
static bit Modbus_Write_DefrostParam_Pending = 0;
static bit Modbus_Write_5_Pending = 0;

#define INDOOR_UNIT_DATA_REG_NUM	8
static u16 s_indoor_regs[INDOOR_UNIT_REG_NUM];

static void Modbus_FillIndoorUnitRegs(u8 index, u16 *regs);
static u8 Modbus_IndoorRegsChanged(u8 idx, u16 *regs);
static void Modbus_TryStartIndoorWrite(void);
static u8 Modbus_AllIndoorOffVp(void);
static void Modbus_TriggerGroupControlWrite(void);
static void Modbus_TriggerGroupControlWriteOff(void);
static void Modbus_TryGroupOrIndoorWrite(void);
static u8 Modbus_IndoorWriteActive(void);
static void Modbus_TriggerIndoorUnitWrite(u8 index, u8 write_all);
static void Modbus_SnapshotHoldSetTemps(void);
static u8 Modbus_ShouldApplyRd100SetTemp(u8 mode, u16 modbus_temp);
static void Modbus_SetTempHoldTick(void);
static void Modbus_RefreshSetTempHoldAfterWr100(void);
static u16 Modbus_RxRelReg(u8 *rx, u8 reg_index);

static void Modbus_IndoorHoldSet(u8 index, u16 power)
{
	if(index >= INDOOR_UNIT_NUM)
		return;
	s_indoor_hold_pwr[index] = power ? 1 : 0;
	s_indoor_pwr_hold[index] = MODBUS_USER_HOLD_INIT;
}

static void Modbus_IndoorHoldSetAll(void)
{
	u8 i;
	u16 pwr;

	for(i = 0; i < INDOOR_UNIT_NUM; i++)
	{
		read_dgus_vp((u32)(0x4010 + i), (u8 *)&pwr, 1);
		Modbus_IndoorHoldSet(i, pwr);
	}
}

static void Modbus_IndoorHoldTick(void)
{
	u8 i;

	for(i = 0; i < INDOOR_UNIT_NUM; i++)
	{
		if(s_indoor_pwr_hold[i] > 1)
			s_indoor_pwr_hold[i]--;
	}
	if(Modbus_Write_6_Pending && s_indoor_wr_unit < INDOOR_UNIT_NUM
		&& s_indoor_pwr_hold[s_indoor_wr_unit] > 0
		&& s_indoor_pwr_hold[s_indoor_wr_unit] < MODBUS_USER_HOLD_INIT)
		s_indoor_pwr_hold[s_indoor_wr_unit] = MODBUS_USER_HOLD_INIT;
}

static void Modbus_SnapshotHoldSetTemps(void)
{
	u8 m;

	for(m = 1; m <= 3; m++)
	{
		read_dgus_vp((u32)SetTempIntVpByMode(m, 0), (u8 *)&s_user_hold_stemp[m], 1);
		s_user_stemp_hold[m] = MODBUS_SETTEMP_HOLD_INIT;
	}
}

static void Modbus_SetTempHoldTick(void)
{
	u8 m;

	for(m = 1; m <= 3; m++)
	{
		if(s_user_stemp_hold[m] > 0)
			s_user_stemp_hold[m]--;
	}
}

static u8 Modbus_ShouldApplyRd100SetTemp(u8 mode, u16 modbus_temp)
{
	if(mode < 1 || mode > 3)
		return 0;
	if(s_user_stemp_hold[mode] == 0)
		return 1;
	if(modbus_temp == s_user_hold_stemp[mode])
	{
		s_user_stemp_hold[mode] = 0;
		return 1;
	}
	return 0;
}

static void Modbus_RefreshSetTempHoldAfterWr100(void)
{
	Modbus_SnapshotHoldSetTemps();
}

static u8 Modbus_Indoor11000BlockEmpty(u8 *rx)
{
	if(Modbus_RxRelReg(rx, 0) != 0)
		return 0;
	if(Modbus_RxRelReg(rx, 1) != 0)
		return 0;
	if(Modbus_RxRelReg(rx, 2) != 0)
		return 0;
	if(Modbus_RxRelReg(rx, 4) != 0)
		return 0;
	if(Modbus_RxRelReg(rx, 5) != 0)
		return 0;
	return 1;
}

static u8 Modbus_IndoorPollLimit(void)
{
	if(s_indoor_installed < 1)
		return 1;
	if(s_indoor_installed > INDOOR_UNIT_NUM)
		return INDOOR_UNIT_NUM;
	return s_indoor_installed;
}

static void Modbus_ApplyIndoorSlotVisibility(void)
{
	u8 i;
	u16 zero = 0;
	u8 limit = Modbus_IndoorPollLimit();

	for(i = limit; i < INDOOR_UNIT_NUM; i++)
		write_dgus_vp((u32)(0x4860 + i), (u8 *)&zero, 1);
}

static void Modbus_SyncIndoorInstalledCount(u16 reg214)
{
	u8 i;
	u8 detected = 0;
	u16 disp214 = reg214;

	/* 0x1004 直接显示 reg214 原值（与 wireless 一致） */
	write_dgus_vp((u32)VP_INDOOR_INSTALLED_COUNT, (u8 *)&disp214, 1);

	for(i = 0; i < INDOOR_UNIT_NUM; i++)
	{
		if(s_indoor_seen[i])
			detected = (u8)(i + 1);
	}
	/* 轮询/隐藏内机槽位仍按物理上限 6 台封顶 */
	if(reg214 >= 1 && reg214 <= INDOOR_UNIT_NUM)
		s_indoor_installed = (u8)reg214;
	else if(reg214 > INDOOR_UNIT_NUM)
		s_indoor_installed = INDOOR_UNIT_NUM;
	else if(detected > 0)
		s_indoor_installed = detected;
	else if(s_indoor_installed < 1 || s_indoor_installed > INDOOR_UNIT_NUM)
		s_indoor_installed = INDOOR_UNIT_NUM;

	Modbus_ApplyIndoorSlotVisibility();
}

u8 Modbus_GetIndoorInstalledCount(void)
{
	return Modbus_IndoorPollLimit();
}

static void Modbus_SyncIndoorOnCountVp(void)
{
	u8 i;
	u16 pwr;

	for(i = 0; i < INDOOR_UNIT_NUM; i++)
	{
		if(s_indoor_pwr_hold[i] > 0)
			continue;
		if(Modbus_IndoorWriteActive() || s_indoor_wr_pending_mask)
			continue;
		read_dgus_vp((u32)(0x4010 + i), (u8 *)&pwr, 1);
		if(pwr > 1)
			pwr = 1;
		if((u8)pwr != s_mb_unit_on[i])
			Modbus_TriggerIndoorUnitWrite(i, 0);
	}
}

static void Modbus_QueueIndoorWriteUnit(u8 index)
{
	u16 pwr;

	if(index >= INDOOR_UNIT_NUM)
		return;
	s_indoor_wr_pending_mask |= (1u << index);
	read_dgus_vp((u32)(0x4010 + index), (u8 *)&pwr, 1);
	Modbus_IndoorHoldSet(index, pwr);
}

static u8 Modbus_IndoorWriteActive(void)
{
	return (Modbus_Write_6 || Modbus_Write_6_Pending) ? 1 : 0;
}

static u8 Modbus_PopPendingIndoorUnit(void)
{
	u8 i;

	for(i = 0; i < INDOOR_UNIT_NUM; i++)
	{
		if(s_indoor_wr_pending_mask & (1u << i))
		{
			s_indoor_wr_pending_mask &= (u8)~(1u << i);
			return i;
		}
	}
	return 0xFF;
}

static void Modbus_TryStartIndoorWrite(void)
{
	u8 idx;

	if(Modbus_IndoorWriteActive())
		return;
	while(1)
	{
		idx = Modbus_PopPendingIndoorUnit();
		if(idx >= INDOOR_UNIT_NUM)
			return;
		Modbus_FillIndoorUnitRegs(idx, s_indoor_regs);
		if(Modbus_IndoorRegsChanged(idx, s_indoor_regs))
		{
			s_indoor_wr_unit = idx;
			Modbus_Write_6_All = 0;
			Modbus_Write_6 = TRUE;
			Modbus_Write_6_Pending = 0;
			Send_Count = MODBUS_POLL_GAP_MS;
#if MODBUS_DBG_ON
			printf("[MODBUS] indoor_wr unit=%u all=0\r\n", (u16)idx);
#endif
			return;
		}
	}
}

static void Modbus_FlushIndoorWritePending(void)
{
	Modbus_TryGroupOrIndoorWrite();
}

static void Modbus_YieldWiFi(void)
{
	WDT_RST();
	wifi_uart_rx_pull();
	wifi_uart_service();
}

bit	First_Write600 = TRUE;

signed	short	SetParameterUpperLimit[SET_PARAMETER_NUM];
signed	short	SetParameterLowerLimit[SET_PARAMETER_NUM];
unsigned	char	IsCheckParameterTemperature[CHECK_PARAMETER_NUM] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1};
unsigned	char	IsCheckParameterTemperature_2[CHECK_PARAMETER_NUM_2] = {0,0,0,1,1,1,1,1,1,1,1,0,1,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
//unsigned char ver[6] = {'0','.','0','.','1','\0'};

u8	send_type = 0;
u8	Modbus_Dbg_SendType = 0;
u8	Modbus_Dbg_FuncCode = 0;
u16 Modbus_Dbg_LastTxAddr = 0;
static u8 Modbus_Dbg_LastTxRw = 0;
static u8 Modbus_Dbg_LastTxUnit = 0;

#if MODBUS_DBG_ON
static u16 Modbus_TxU16(u8 *buf, u8 byte_off);
static u16 Modbus_RxU16(u8 *rx, u8 byte_off);
static void Modbus_DbgLogTx(u16 addr, u8 is_rd, u16 len);
static void Modbus_DbgLogRx(u16 addr, u8 is_rd, u8 *rx);
#endif
u16 En_change_Controller = 1;//1，可以改写线控器开关机，模式，设定温度F
u8 Read_Or_Write = 0;//区分读或者写指令的回复
u8 Analysis_OK = 1;//解析完成标志


unsigned short	xuantest1=11,xuantest2=22;


STR_FIRST_QUERY FirstQueryMember = {65,35,35,5,60,40};

typedef union{
	
	u8 temp[4];
	u16 all;
	u16 u16_temp[2];
}ERC_DATA;
typedef union{
	
	u8 temp[2];
	s16 all;
}ERC_DATA1;


u16 CalculateModbusCrc(u8 *pBuf, u16 nLen)
{
	u16 i;
	u8	j;
	u16 nData;
	u16 nFlag;
	
	nData = 0xFFFF;
	for (i=0; i<nLen; i++)
	{
		nData = nData ^ pBuf[i];
		for (j=0; j<8; j++)
		{
			nFlag = nData & 0x01;
			nData = nData >> 1;
			nData = nData & 0x7fff;
			
			if (nFlag != 0)
			{
				nData = nData ^ 0xa001;
			}
		}
	}
	
	return nData;
}

/*****************************************************************************
 函 数 名  : void Modbus_Init(void)
 功能描述  : modbus初始化
 输入参数  :	
 输出参数  : 
 修改历史  :
  1.日    期   : 2019年7月31日
    作    者   : chengjing
    修改内容   : 创建
*****************************************************************************/
void Modbus_Init(void)
{
	UART5_Init();
				
}
static void Modbus_AppendU16(u16 *pLen, u16 val)
{
	Modbus_Buffer[(*pLen)++] = (u8)(val >> 8);
	Modbus_Buffer[(*pLen)++] = (u8)(val & 0xFF);
}

static void Modbus_AppendModeSetTempReg(u16 *pLen, u8 mode)
{
	u16 temp = 0;

	if(mode >= 1 && mode <= 3)
		read_dgus_vp((u32)SetTempIntVpByMode(mode, 0), (u8 *)&temp, 1);
	Modbus_AppendU16(pLen, (u16)(temp * 10));
}

static u16 Modbus_EncodeIndoorRunMode(u16 power, u16 dgus_mode)
{
	u16 modbus_mode;
	if(power == 0)
		return 0;
	switch(dgus_mode)
	{
		case 0: modbus_mode = 2; break; /* 制冷 */
		case 1: modbus_mode = 6; break; /* 除湿 */
		case 2: modbus_mode = 1; break; /* 送风 */
		case 3: modbus_mode = 3; break; /* 制热 */
		case 4: modbus_mode = 7; break; /* 自动 */
		default: modbus_mode = 7; break;
	}
	return (u16)(0x0080 | modbus_mode);
}

static u16 Modbus_EncodeIndoorFan(u16 dgus_fan)
{
	switch(dgus_fan)
	{
		case 0: return 8; /* 自动风 */
		case 1: return 2; /* 低风 */
		case 2: return 4; /* 中风 */
		case 3: return 6; /* 高风 */
		case 4: return 7; /* 超高风 */
		default: return 8;
	}
}

static u16 Modbus_DecodeIndoorModeBits(u16 reg)
{
	switch(reg & 0x0007)
	{
		case 2: return 0; /* 制冷 */
		case 6: return 1; /* 除湿 */
		case 1: return 2; /* 送风 */
		case 3: return 3; /* 制热 */
		case 7: return 4; /* 自动 */
		default: return 0;
	}
}

static u16 Modbus_DecodeIndoorFan(u16 reg)
{
	if(reg == 8)
		return 0;
	if(reg == 0)
		return 0;
	if(reg <= 2)
		return 1;
	if(reg <= 4)
		return 2;
	if(reg <= 6)
		return 3;
	return 4;
}

static u16 Modbus_ScaleIndoorSetTemp(u8 index, u16 temp_c)
{
	if(index == 0)
		return (u16)(temp_c * 10);
	return (u16)(temp_c * 2);
}

static u16 Modbus_UnscaleIndoorSetTemp(u8 index, u16 raw)
{
	if(index == 0)
		return (u16)(raw / 10);
	return (u16)(raw / 2);
}

static void Modbus_SyncHomeIndoorModeFromUnit(u8 uidx, u16 dgus_mode)
{
	if(dgus_mode > 4)
		dgus_mode = 0;
	write_dgus_vp((u32)(0x1101 + (u16)uidx * 0x20), (u8 *)&dgus_mode, 1);
	HomePage_SyncIndoorDisplay();
}

#define INDOOR_INFO_REG_NUM			8

static u8 s_check_status[MODBUS_CHECK_STATUS_LEN];
static u16 s_indoor_sent_regs[INDOOR_UNIT_NUM][INDOOR_UNIT_DATA_REG_NUM];
static u8 s_indoor_sent_valid[INDOOR_UNIT_NUM];

static void Modbus_FillIndoorUnitRegs(u8 index, u16 *regs)
{
	u8 i;
	u16 power, mode, temp, fan;
	u32 vp_base;

	read_dgus_vp((u32)(0x4010 + index), (u8 *)&power, 1);
	read_dgus_vp((u32)(0x4700 + index), (u8 *)&mode, 1);
	read_dgus_vp((u32)(0x1020 + index), (u8 *)&temp, 1);
	read_dgus_vp((u32)(0x4710 + index), (u8 *)&fan, 1);

	regs[0] = Modbus_EncodeIndoorRunMode(power, mode);
	regs[1] = Modbus_EncodeIndoorFan(fan);
	regs[2] = Modbus_ScaleIndoorSetTemp(index, temp);

	vp_base = INDOOR_UNIT_TIMER_VP_BASE + (u32)index * 0x10UL;
	regs[3] = 0;
	read_dgus_vp(vp_base + 0, (u8 *)&regs[4], 1);
	read_dgus_vp(vp_base + 1, (u8 *)&regs[5], 1);
	read_dgus_vp(vp_base + 2, (u8 *)&regs[6], 1);
	read_dgus_vp(vp_base + 3, (u8 *)&regs[7], 1);

	for(i = 8; i < INDOOR_UNIT_REG_NUM; i++)
		regs[i] = 0;
}

static u8 Modbus_IndoorRegsChanged(u8 idx, u16 *regs)
{
	u8 i;

	if(!s_indoor_sent_valid[idx])
		return 1;
	for(i = 0; i < INDOOR_UNIT_DATA_REG_NUM; i++)
	{
		if(regs[i] != s_indoor_sent_regs[idx][i])
			return 1;
	}
	return 0;
}

static void Modbus_SaveIndoorSentRegs(u8 idx, u16 *regs)
{
	u8 i;

	for(i = 0; i < INDOOR_UNIT_DATA_REG_NUM; i++)
		s_indoor_sent_regs[idx][i] = regs[i];
	s_indoor_sent_valid[idx] = 1;
}

static u8 Modbus_AllIndoorOffVp(void)
{
	u8 i;
	u16 pwr;

	for(i = 0; i < INDOOR_UNIT_NUM; i++)
	{
		read_dgus_vp((u32)(0x4010 + i), (u8 *)&pwr, 1);
		if(pwr != 0)
			return 0;
	}
	return 1;
}

static void Modbus_IndoorHoldSetAllOff(void)
{
	u8 i;

	for(i = 0; i < INDOOR_UNIT_NUM; i++)
		Modbus_IndoorHoldSet(i, 0);
}

static void Modbus_TryGroupOrIndoorWrite(void)
{
	if(Modbus_IndoorWriteActive())
		return;
	if(Modbus_Write_5 || Modbus_Write_5_Pending)
		return;
	if(Modbus_AllIndoorOffVp())
	{
		if(s_indoor_wr_pending_mask)
		{
			Modbus_TryStartIndoorWrite();
			return;
		}
		s_indoor_wr_pending_mask = 0;
		Modbus_TriggerGroupControlWriteOff();
		return;
	}
	Modbus_TryStartIndoorWrite();
}

#define GROUP_CONTROL_REG_NUM	24

static u16 s_group_regs[GROUP_CONTROL_REG_NUM];
static u16 s_group_sent_regs[GROUP_CONTROL_REG_NUM];
static u8 s_group_sent_valid = 0;

static void Modbus_FillGroupControlRegsForce(u16 *regs, s8 force_pwr)
{
	u8 i;
	u16 val;
	u16 group_pwr;

	if(force_pwr >= 0)
		group_pwr = (u16)force_pwr;
	else
		group_pwr = Modbus_AllIndoorOffVp() ? 0 : 1;

	for(i = 0; i < 7; i++)
		regs[i] = 0xFF;

	read_dgus_vp((u32)(0x3020), (u8 *)&val, 1);
	regs[7] = Modbus_EncodeIndoorRunMode(group_pwr, val);

	read_dgus_vp((u32)(0x1090), (u8 *)&val, 1);
	regs[8] = val;

	read_dgus_vp((u32)(0x3025), (u8 *)&val, 1);
	regs[9] = Modbus_EncodeIndoorFan(val);

	for(i = 10; i < GROUP_CONTROL_REG_NUM; i++)
		regs[i] = 0;
}

static void Modbus_FillGroupControlRegs(u16 *regs)
{
	Modbus_FillGroupControlRegsForce(regs, -1);
}

static u8 Modbus_GroupRegsChanged(u16 *regs)
{
	u8 i;

	if(!s_group_sent_valid)
		return 1;
	for(i = 0; i < GROUP_CONTROL_REG_NUM; i++)
	{
		if(regs[i] != s_group_sent_regs[i])
			return 1;
	}
	return 0;
}

static void Modbus_SaveGroupSentRegs(u16 *regs)
{
	u8 i;

	for(i = 0; i < GROUP_CONTROL_REG_NUM; i++)
		s_group_sent_regs[i] = regs[i];
	s_group_sent_valid = 1;
}

void Modbus_TriggerGroupControlWrite(void)
{
	Modbus_FillGroupControlRegs(s_group_regs);
	if(Modbus_GroupRegsChanged(s_group_regs))
	{
		Modbus_Write_5 = TRUE;
		Modbus_Write_5_Pending = 0;
		Send_Count = MODBUS_POLL_GAP_MS;
#if MODBUS_DBG_ON
		printf("[MODBUS] group_wr addr=15000 pwr=%u\r\n",
			(u16)(Modbus_AllIndoorOffVp() ? 0 : 1));
#endif
	}
}

void Modbus_TriggerGroupControlWriteOff(void)
{
	u8 i;

	for(i = 0; i < INDOOR_UNIT_NUM; i++)
		Modbus_IndoorHoldSet(i, 0);
	s_indoor_wr_pending_mask = 0;
	Modbus_FillGroupControlRegsForce(s_group_regs, 0);
	if(Modbus_GroupRegsChanged(s_group_regs))
	{
		Modbus_Write_5 = TRUE;
		Modbus_Write_5_Pending = 0;
		Send_Count = MODBUS_POLL_GAP_MS;
#if MODBUS_DBG_ON
		printf("[MODBUS] group_wr addr=15000 pwr=0\r\n");
#endif
	}
}

static void Modbus_ReportFaultCode(u16 fault_code)
{
	write_dgus_vp((u32)(0x4607),(u8 *)&fault_code,1);
	upload_dp_fault();
}

static u8 Modbus_IsFaultActive(u16 err_code, u16 *err_words)
{
	u8 wi, bi;

	if(err_code == 0)
		return 0;
	wi = (u8)((err_code - 1) / 16);
	bi = (u8)((err_code - 1) % 16);
	if(wi >= 4)
		return 0;
	return (err_words[wi] >> bi) & 0x01;
}

static u8 Modbus_PackCheckStatusByte(u16 val, u8 is_temp)
{
	s16 temp;

	if(is_temp)
	{
		temp = (s16)(val / 10);
		return (u8)temp;
	}
	return (u8)(val & 0xFF);
}

static void Modbus_PackCheckStatusBlock(u8 *rx, u8 count, u8 *is_temp_arr, u8 dst_off)
{
	u8 i;
	ERC_DATA u8tou16;

	for(i = 0; i < count; i++)
	{
		u8tou16.temp[0] = rx[3 + i * 2];
		u8tou16.temp[1] = rx[4 + i * 2];
		s_check_status[dst_off + i] = Modbus_PackCheckStatusByte(u8tou16.all, is_temp_arr[i]);
	}
}

static u8 Modbus_WriteFloatTempPair(u32 vp_c, u32 vp_f, float temp_c)
{
	float old_c;
	float temp_f;

	read_dgus_vp(vp_c, (u8 *)&old_c, 2);
	if((u16)old_c == (u16)temp_c)
		return 0;
	temp_f = (float)TempUnitTrans((signed short)temp_c, 'F');
	write_dgus_vp(vp_c, (u8 *)&temp_c, 2);
	write_dgus_vp(vp_f, (u8 *)&temp_f, 2);
	return 1;
}

void Modbus_GetCheckStatus(u8 *buf, u8 len)
{
	u8 i, n;

	if(buf == 0)
		return;
	n = (len < MODBUS_CHECK_STATUS_LEN) ? len : MODBUS_CHECK_STATUS_LEN;
	for(i = 0; i < n; i++)
		buf[i] = s_check_status[i];
}

void Modbus_SetDefrostDisplay(u16 on)
{
	u16 ui_val;

	write_dgus_vp((u32)(0x201b), (u8 *)&on, 1);
	Defrosting = on ? TRUE : FALSE;
	ui_val = on;
	write_dgus_vp((u32)(0x3041), (u8 *)&ui_val, 1);
	if(!on)
		s_defrost_want = 0;
}

static void Modbus_SyncDefrostFromReg205(u16 reg205_raw)
{
	u8 def_on;
	u16 vp_old;

	def_on = (u8)(reg205_raw & 0x0001);
	read_dgus_vp((u32)(0x201b), (u8 *)&vp_old, 1);

	if(def_on)
	{
		if(s_defrost_want)
			s_defrost_want = 0;
		if(vp_old != 1 || !Defrosting)
		{
			Modbus_SetDefrostDisplay(1);
			Ready_To_Save();
			upload_dp_defrost();
		}
	}
	else if(!s_defrost_want)
	{
		if(vp_old != 0 || Defrosting)
		{
			Modbus_SetDefrostDisplay(0);
			Control_ClearDefrostSession();
			Ready_To_Save();
			upload_dp_defrost();
		}
	}
}

void Modbus_StartManualDefrost(void)
{
	u16 one = 1;

	s_defrost_want = 1;
	s_defrost_verify = 0;
	Modbus_DefrostRetryLeft = 3;
	Modbus_SetDefrostDisplay(one);
	Modbus_Write_Defrost = TRUE;
	Modbus_Write_Ueser = TRUE;
	Modbus_Write_Ueser_Pending = 0;
	Send_Count = MODBUS_POLL_GAP_MS;
}

void Modbus_ClearManualDefrost(void)
{
	s_defrost_want = 0;
	s_defrost_verify = 0;
}

static void Modbus_StartNextTimerWrite(void)
{
	if(s_timer_q_pos >= s_timer_q_len)
	{
		s_timer_q_len = 0;
		s_timer_q_pos = 0;
		return;
	}
	Modbus_Timer_Unit_Index = s_timer_q[s_timer_q_pos++];
	Modbus_Write_Timer = TRUE;
	Modbus_Write_Timer_Pending = 0;
	Send_Count = MODBUS_POLL_GAP_MS;
}

static void Modbus_QueueIndoorTimerWrite(u8 index)
{
	if(s_timer_q_len < INDOOR_UNIT_NUM)
		s_timer_q[s_timer_q_len++] = index;
	if(!Modbus_Write_Timer && !Modbus_Write_Timer_Pending)
		Modbus_StartNextTimerWrite();
}

void Modbus_ApplyIndoorTimerVpToUnits(u8 update_on, u8 update_off)
{
	u8 i, sel_count = 0;
	u16 on_en, off_en, hour, minute, on_time, off_time, sel;
	u32 vp_base;

	s_timer_q_len = 0;
	s_timer_q_pos = 0;

	if(update_on)
	{
		read_dgus_vp((u32)VP_TIMER_IN_ON_EN, (u8 *)&on_en, 1);
		read_dgus_vp((u32)(0x1084), (u8 *)&hour, 1);
		read_dgus_vp((u32)(0x1085), (u8 *)&minute, 1);
		on_time = (u16)(((u16)hour << 8) | minute);
	}
	if(update_off)
	{
		read_dgus_vp((u32)VP_TIMER_IN_OFF_EN, (u8 *)&off_en, 1);
		read_dgus_vp((u32)(0x1086), (u8 *)&hour, 1);
		read_dgus_vp((u32)(0x1087), (u8 *)&minute, 1);
		off_time = (u16)(((u16)hour << 8) | minute);
	}

	for(i = 0; i < Modbus_IndoorPollLimit(); i++)
	{
		read_dgus_vp((u32)(0x4860 + i), (u8 *)&sel, 1);
		if(!sel)
			continue;
		sel_count++;
		vp_base = INDOOR_UNIT_TIMER_VP_BASE + (u32)i * 0x10UL;
		if(update_on)
		{
			write_dgus_vp(vp_base + 0, (u8 *)&on_en, 1);
			write_dgus_vp(vp_base + 1, (u8 *)&on_time, 1);
		}
		if(update_off)
		{
			write_dgus_vp(vp_base + 2, (u8 *)&off_en, 1);
			write_dgus_vp(vp_base + 3, (u8 *)&off_time, 1);
		}
		Modbus_QueueIndoorTimerWrite(i);
	}
	/* 4870 镜像区供 Modbus 定时寄存器，勿写屏绑定 0x4840 */
	vp_base = INDOOR_UNIT_TIMER_VP_BASE;
	if(update_on)
	{
		write_dgus_vp(vp_base + 0, (u8 *)&on_en, 1);
		write_dgus_vp(vp_base + 1, (u8 *)&on_time, 1);
	}
	if(update_off)
	{
		write_dgus_vp(vp_base + 2, (u8 *)&off_en, 1);
		write_dgus_vp(vp_base + 3, (u8 *)&off_time, 1);
		write_dgus_vp((u32)VP_TIMER_IN_OFF_EN_UI, (u8 *)&off_en, 1);
	}
}

void Modbus_TriggerIndoorUnitWrite(u8 index, u8 write_all)
{
	u8 i;

	if(Modbus_AllIndoorOffVp())
	{
		Modbus_IndoorHoldSetAllOff();
		if(write_all)
		{
			s_indoor_wr_pending_mask = 0;
			if(Modbus_IndoorWriteActive())
				return;
			Modbus_TriggerGroupControlWriteOff();
			return;
		}
		if(index >= INDOOR_UNIT_NUM)
			return;
		Modbus_QueueIndoorWriteUnit(index);
		Modbus_TryGroupOrIndoorWrite();
		return;
	}

	if(write_all)
	{
		Modbus_IndoorHoldSetAll();
		s_indoor_wr_pending_mask = 0;
		if(Modbus_IndoorWriteActive())
			return;
		Modbus_TriggerGroupControlWrite();
		return;
	}
	else if(index < INDOOR_UNIT_NUM)
		Modbus_QueueIndoorWriteUnit(index);
	else
		return;
	Modbus_TryGroupOrIndoorWrite();
}

void Modbus_TriggerDefrostParamWrite(u16 vp)
{
	s_defrost_param_vp = vp;
	Modbus_Write_DefrostParam = TRUE;
	Modbus_Write_DefrostParam_Pending = 0;
	Send_Count = MODBUS_POLL_GAP_MS;
}

void Modbus_TriggerUserWrite(void)
{
	u16 pwr, mode;

	read_dgus_vp((u32)(0x4021),(u8 *)&pwr,1);
	read_dgus_vp((u32)(0x2002),(u8 *)&mode,1);
	if(pwr > 1)
		return;
	if((Modbus_Write_Ueser || Modbus_Write_Ueser_Pending) && pwr == s_user_hold_pwr && mode == s_user_hold_mode)
		return;
	if(s_user_power_hold > 0 && pwr == s_user_hold_pwr)
		return;
	Modbus_SnapshotHoldSetTemps();
	s_user_hold_pwr = pwr;
	s_user_hold_mode = mode;
	s_user_power_hold = MODBUS_USER_HOLD_INIT;
	Modbus_Write_Ueser = TRUE;
	Modbus_Write_Ueser_Pending = 0;
	Send_Count = MODBUS_POLL_GAP_MS;
#if MODBUS_DBG_ON
	printf("[MODBUS] user_wr pwr=%u\r\n", (u16)s_user_hold_pwr);
#endif
}

void Modbus_MarkSetTempDirty(void)
{
	s_user_settemp_dirty = 1;
}

void Modbus_QueueSetTempUserWrite(void)
{
	u16 pwr, mode;

	read_dgus_vp((u32)(0x4021),(u8 *)&pwr,1);
	read_dgus_vp((u32)(0x2002),(u8 *)&mode,1);
	if(pwr > 1)
		return;
	Modbus_SnapshotHoldSetTemps();
	if(Modbus_Write_Ueser || Modbus_Write_Ueser_Pending)
	{
		s_user_settemp_dirty = 1;
		return;
	}
	s_user_hold_pwr = pwr;
	s_user_hold_mode = mode;
	s_user_power_hold = MODBUS_USER_HOLD_INIT;
	Modbus_Write_Ueser = TRUE;
	Modbus_Write_Ueser_Pending = 0;
	Send_Count = MODBUS_POLL_GAP_MS;
#if MODBUS_DBG_ON
	printf("[MODBUS] user_wr pwr=%u stemp\r\n", (u16)s_user_hold_pwr);
#endif
}

void Modbus_FlushSetTempUserWrite(void)
{
	if(!s_user_settemp_dirty)
		return;
	if(Modbus_Write_Ueser || Modbus_Write_Ueser_Pending)
		return;
	s_user_settemp_dirty = 0;
	Modbus_QueueSetTempUserWrite();
}

void Modbus_SetTempWriteService(void)
{
	if(!s_user_settemp_dirty || Modbus_UserWriteBusy())
		return;
	Modbus_FlushSetTempUserWrite();
}

void Modbus_TriggerUserWriteWith(u16 pwr, u16 mode)
{
	if(pwr > 1)
		pwr = 1;
	Modbus_SnapshotHoldSetTemps();
	s_user_hold_pwr = pwr;
	s_user_hold_mode = mode;
	s_user_power_hold = MODBUS_USER_HOLD_INIT;
	Modbus_Write_Ueser = TRUE;
	Modbus_Write_Ueser_Pending = 0;
	Send_Count = MODBUS_POLL_GAP_MS;
#if MODBUS_DBG_ON
	printf("[MODBUS] user_wr pwr=%u mode=%u\r\n", (u16)s_user_hold_pwr, (u16)s_user_hold_mode);
#endif
}

u8 Modbus_UserWriteBusy(void)
{
	return (Modbus_Write_Ueser || Modbus_Write_Ueser_Pending) ? 1 : 0;
}

u8 Modbus_IndoorWriteBusy(void)
{
	return (Modbus_IndoorWriteActive() || s_indoor_wr_pending_mask
		|| Modbus_Write_5 || Modbus_Write_5_Pending) ? 1 : 0;
}

u8 Modbus_UserHoldActive(void)
{
	return s_user_power_hold > 0;
}

void Modbus_GetUserPowerMode(u16 *pwr, u16 *mode)
{
	*pwr = s_user_hold_pwr;
	*mode = s_user_hold_mode;
}

static void Modbus_ClearPollReadFlags(void)
{
	Modbus_Read_User = FALSE;
	Modbus_Read_Check = FALSE;
	Modbus_Read_Check_2 = FALSE;
	Modbus_Read_Check_3 = FALSE;
	Modbus_Read_Check_4 = FALSE;
	Modbus_Read_Check_5 = FALSE;
}

static void Modbus_StartIndoorPollAfterCheck2(void)
{
	Modbus_ClearPollReadFlags();
#if MODBUS_SKIP_RD1000_POLL
	Indoor_Unit_Index = 0;
#if MODBUS_SKIP_RD12_POLL
	Modbus_Read_Check_4 = TRUE;
#else
	Modbus_Read_Check_5 = TRUE;
#endif
#else
	Modbus_Read_Check_3 = TRUE;
	Indoor_Unit_Index = 0;
#endif
}

static void Modbus_AdvancePollOnTimeout(void)
{
	switch(Modbus_Dbg_SendType)
	{
	case 5:
		Modbus_Read_User = FALSE;
		Modbus_Read_Check = TRUE;
		break;
	case 4:
		Modbus_Read_Check = FALSE;
		Modbus_Read_Check_2 = TRUE;
		break;
	case 6:
		Modbus_StartIndoorPollAfterCheck2();
		break;
#if !MODBUS_SKIP_RD1000_POLL
	case 8:
		if(Indoor_Unit_Index + 1 < Modbus_IndoorPollLimit())
		{
			Indoor_Unit_Index++;
			Modbus_Read_Check_3 = TRUE;
		}
		else
		{
			Indoor_Unit_Index = 0;
			Modbus_Read_Check_3 = FALSE;
#if MODBUS_SKIP_RD12_POLL
			Modbus_Read_Check_4 = TRUE;
#else
			Modbus_Read_Check_5 = TRUE;
#endif
		}
		break;
#endif
	case 12:
		if(Indoor_Unit_Index + 1 < Modbus_IndoorPollLimit())
		{
			Indoor_Unit_Index++;
			Modbus_Read_Check_5 = TRUE;
		}
		else
		{
			Indoor_Unit_Index = 0;
			Modbus_Read_Check_5 = FALSE;
			Modbus_Read_Check_4 = TRUE;
		}
		break;
	case 9:
		Modbus_Read_Check_4 = FALSE;
		Indoor_Unit_Index = 0;
		Modbus_Read_User = TRUE;
		break;
	default:
		break;
	}
}

void Modbus_Salve_Handler1(void)
{
	unsigned short	Modbus_Crc;
	unsigned short	Modbus_Len = 0;
	unsigned short	Modbus_u16temp, Modbus_Power, Modbus_Mode;
	u16 error;
	u16 dbg_addr;
	static unsigned char s_modbus_depth = 0;

	if(s_modbus_depth >= 2)
		return;
	s_modbus_depth++;

	if(s_user_power_hold > 1)
		s_user_power_hold--;
	Modbus_SetTempHoldTick();
	Modbus_IndoorHoldTick();
	if(Modbus_Write_Ueser_Pending && s_user_power_hold < MODBUS_USER_HOLD_INIT)
		s_user_power_hold = MODBUS_USER_HOLD_INIT;

	Modbus_Analysis_Data1();

	if(Modbus_Write_Ueser && !Modbus_Write_Ueser_Pending && modbus_error && Modbus_Error_Count >= 200)
		modbus_error = 0;

	if(modbus_error && !modbus_res_finish)
	{
		if(Modbus_Error_Count >= MODBUS_RSP_TIMEOUT_MS)
		{
			modbus_error = 0;
			if(Modbus_Write_6)
			{
				Modbus_QueueIndoorWriteUnit(s_indoor_wr_unit);
				Modbus_Write_6 = FALSE;
				Modbus_Write_6_Pending = 0;
				Modbus_Write_6_All = 0;
				Indoor_Unit_Index = 0;
				Modbus_Read_User = TRUE;
				Modbus_FlushIndoorWritePending();
			}
			else if(Modbus_Write_5)
			{
				Modbus_Write_5 = FALSE;
				Modbus_Write_5_Pending = 0;
				Modbus_Read_User = TRUE;
				Modbus_FlushIndoorWritePending();
			}
			else if(Modbus_Write_Ueser_Pending)
			{
				Modbus_Write_Ueser = FALSE;
				Modbus_Write_Ueser_Pending = 0;
			}
			else if(Modbus_Dbg_LastTxRw)
			{
				Modbus_AdvancePollOnTimeout();
			}
		}
		Modbus_YieldWiFi();
		goto modbus_done;
	}

	Modbus_Read_Initial = FALSE;
	if(Modbus_Write_Ueser) {
		if(s_user_power_hold == 0)
			s_user_power_hold = MODBUS_USER_HOLD_INIT;
		send_type = 7;
	}
	else if(Modbus_Write_Timer)
		send_type = 15;
	else if(Modbus_Write_6)
		send_type = 11;
	else if(Modbus_Write_5)
		send_type = 10;
	else if(Modbus_Write_DefrostParam)
		send_type = 14;
	else if(Modbus_Read_User)
		send_type = 5;
	else	if(Modbus_Read_Check)
		send_type = 4;
	else	if(Modbus_Read_Check_2)
		send_type = 6;
	else	if(Modbus_Read_Check_3)
		send_type = 8;
	else    if(Modbus_Read_Check_4)
		send_type = 9;
	else	if(Modbus_Read_Check_5)
		send_type = 12;
	else
		send_type = 5;

	Modbus_Buffer[Modbus_Len++] = 0x01;

	switch(send_type)
	{
		case	4://200地址
			Modbus_Buffer[Modbus_Len++] = 0x03;
			Modbus_Buffer[Modbus_Len++] = (200 / 256);
			Modbus_Buffer[Modbus_Len++] = (200 % 256);
			Modbus_Buffer[Modbus_Len++] = 0;//长度占4位
			Modbus_Buffer[Modbus_Len++] = CHECK_PARAMETER_NUM;

		break;
		case	5://read 100D
			Modbus_Buffer[Modbus_Len++] = 0x03;
			Modbus_Buffer[Modbus_Len++] = (100 / 256);//修改地址为100开始
			Modbus_Buffer[Modbus_Len++] = (100 % 256);
			Modbus_Buffer[Modbus_Len++] = 0x00;//长度占4位
			Modbus_Buffer[Modbus_Len++] = USER_PARAMETER_NUM;

		break;

		case	6://300地址
			Modbus_Buffer[Modbus_Len++] = 0x03;
			Modbus_Buffer[Modbus_Len++] = (300 / 256);
			Modbus_Buffer[Modbus_Len++] = (300 % 256);
													Modbus_Buffer[Modbus_Len++] = 0;//长度占4位
			Modbus_Buffer[Modbus_Len++] = CHECK_PARAMETER_NUM_2;
		break;

		case	9://read 30000D
			Modbus_Buffer[Modbus_Len++] = 0x03;
			Modbus_Buffer[Modbus_Len++] = (30366 / 256);
			Modbus_Buffer[Modbus_Len++] = (30366 % 256);
			Modbus_Buffer[Modbus_Len++] = 0;
			Modbus_Buffer[Modbus_Len++] = CHECK_PARAMETER_NUM_4;
		break;
		case 8: // read n*100+1000：仅内机室内温度
		{
			u16 addr = 1000 + (u16)Indoor_Unit_Index * 100;
			Modbus_Buffer[Modbus_Len++] = 0x03;
			Modbus_Buffer[Modbus_Len++] = (u8)(addr >> 8);
			Modbus_Buffer[Modbus_Len++] = (u8)(addr & 0xFF);
			Modbus_Buffer[Modbus_Len++] = 0;
			Modbus_Buffer[Modbus_Len++] = INDOOR_INFO_REG_NUM;
		}
		break;
		case 12: // Read 11000 series
		{
			u16 addr = 11000 + (u16)Indoor_Unit_Index * 30;
			Modbus_Buffer[Modbus_Len++] = 0x03;
			Modbus_Buffer[Modbus_Len++] = (u8)(addr >> 8);
			Modbus_Buffer[Modbus_Len++] = (u8)(addr & 0xFF);
			Modbus_Buffer[Modbus_Len++] = 0;
			Modbus_Buffer[Modbus_Len++] = 30; // Read 30 registers
		}
		break;
		case    10:{
			u8 i;

			Modbus_FillGroupControlRegs(s_group_regs);
			Modbus_Buffer[Modbus_Len++] = 0x10;
			Modbus_Buffer[Modbus_Len++] = (15000 / 256);
			Modbus_Buffer[Modbus_Len++] = (15000 % 256);
			Modbus_Buffer[Modbus_Len++] = 0;
			Modbus_Buffer[Modbus_Len++] = 24;
			Modbus_Buffer[Modbus_Len++] = 48;
			for(i = 0; i < GROUP_CONTROL_REG_NUM; i++)
				Modbus_AppendU16(&Modbus_Len, s_group_regs[i]);
		}
		break;
	case    11:
	{
		u8 ri;
		u16 addr = 11000 + (u16)s_indoor_wr_unit * 30;

		Modbus_FillIndoorUnitRegs(s_indoor_wr_unit, s_indoor_regs);
		Modbus_Buffer[Modbus_Len++] = 0x10;
		Modbus_Buffer[Modbus_Len++] = (u8)(addr >> 8);
		Modbus_Buffer[Modbus_Len++] = (u8)(addr & 0xFF);
		Modbus_Buffer[Modbus_Len++] = 0;
		Modbus_Buffer[Modbus_Len++] = 30;
		Modbus_Buffer[Modbus_Len++] = 60;

		for(ri = 0; ri < 8; ri++)
			Modbus_AppendU16(&Modbus_Len, s_indoor_regs[ri]);
		for(ri = 0; ri < 22; ri++)
		{
			Modbus_Buffer[Modbus_Len++] = 0;
			Modbus_Buffer[Modbus_Len++] = 0;
		}
	}
	break;
	case	7:
		Modbus_Buffer[Modbus_Len++] = 0x10;
	Modbus_Buffer[Modbus_Len++] = (100 / 256);
	Modbus_Buffer[Modbus_Len++] = (100 % 256);
	Modbus_Buffer[Modbus_Len++] = 0;
	Modbus_Buffer[Modbus_Len++] = 13;
	Modbus_Buffer[Modbus_Len++] = 26;

	read_dgus_vp((u32)(0x4021),(u8 *)&Modbus_Power,1);
	Modbus_u16temp = Modbus_Power ? 1 : 0;
	Modbus_Buffer[Modbus_Len++] = (Modbus_u16temp / 256);
	Modbus_Buffer[Modbus_Len++] = (Modbus_u16temp % 256);

	read_dgus_vp((u32)(0x2002),(u8 *)&Modbus_Mode,1);
	Modbus_u16temp = 0;
	Modbus_u16temp = Modbus_Mode;
	//			if(Modbus_Mode == 0)
	//				Modbus_u16temp = 8;
	//			else
	//				Modbus_u16temp = Modbus_Mode * 2 - 1;
	Modbus_Buffer[Modbus_Len++] = (Modbus_u16temp / 256);
	Modbus_Buffer[Modbus_Len++] = (Modbus_u16temp % 256);

	Modbus_AppendModeSetTempReg(&Modbus_Len, 1);
	Modbus_AppendModeSetTempReg(&Modbus_Len, 2);

	Modbus_u16temp = 0;
	Modbus_Buffer[Modbus_Len++] = (Modbus_u16temp / 256);
	Modbus_Buffer[Modbus_Len++] = (Modbus_u16temp % 256);

	Modbus_u16temp = 0;
	Modbus_Buffer[Modbus_Len++] = (Modbus_u16temp / 256);
	Modbus_Buffer[Modbus_Len++] = (Modbus_u16temp % 256);

	read_dgus_vp((u32)(0x201b),(u8 *)&Modbus_Mode,1);
	Modbus_u16temp = Modbus_Mode;
	Modbus_Buffer[Modbus_Len++] = (Modbus_u16temp / 256);
	Modbus_Buffer[Modbus_Len++] = (Modbus_u16temp % 256);

	Modbus_u16temp = 0;
	Modbus_Buffer[Modbus_Len++] = (Modbus_u16temp / 256);
	Modbus_Buffer[Modbus_Len++] = (Modbus_u16temp % 256);

	Modbus_u16temp = 0;
	Modbus_Buffer[Modbus_Len++] = (Modbus_u16temp / 256);
	Modbus_Buffer[Modbus_Len++] = (Modbus_u16temp % 256);

	Modbus_AppendModeSetTempReg(&Modbus_Len, 3);

	Modbus_u16temp = 0;
	Modbus_Buffer[Modbus_Len++] = (Modbus_u16temp / 256);
	Modbus_Buffer[Modbus_Len++] = (Modbus_u16temp % 256);

	Modbus_u16temp = 100;
	Modbus_Buffer[Modbus_Len++] = (Modbus_u16temp / 256);
	Modbus_Buffer[Modbus_Len++] = (Modbus_u16temp % 256);

	Modbus_u16temp = 100;
	Modbus_Buffer[Modbus_Len++] = (Modbus_u16temp / 256);
	Modbus_Buffer[Modbus_Len++] = (Modbus_u16temp % 256);
	break;
	case 14:
	{
		u16 freq, dtime, t3c;
		u16 reg = 30366;

		read_dgus_vp((u32)(0x3660), (u8 *)&freq, 1);
		read_dgus_vp((u32)(0x3670), (u8 *)&dtime, 1);
		read_dgus_vp((u32)(0x3680), (u8 *)&t3c, 1);
		t3c = (u16)(t3c * 10);

		Modbus_Buffer[Modbus_Len++] = 0x10;
		Modbus_Buffer[Modbus_Len++] = (u8)(reg >> 8);
		Modbus_Buffer[Modbus_Len++] = (u8)(reg & 0xFF);
		Modbus_Buffer[Modbus_Len++] = 0;
		Modbus_Buffer[Modbus_Len++] = 3;
		Modbus_Buffer[Modbus_Len++] = 6;
		Modbus_AppendU16(&Modbus_Len, freq);
		Modbus_AppendU16(&Modbus_Len, dtime);
		Modbus_AppendU16(&Modbus_Len, t3c);
	}
	break;
	case 15:
	{
		u8 ti;
		u16 addr = (u16)(11004 + (u16)Modbus_Timer_Unit_Index * 30);
		u32 vp_base = INDOOR_UNIT_TIMER_VP_BASE + (u32)Modbus_Timer_Unit_Index * 0x10UL;
		u16 treg[4];

		read_dgus_vp(vp_base + 0, (u8 *)&treg[0], 1);
		read_dgus_vp(vp_base + 1, (u8 *)&treg[1], 1);
		read_dgus_vp(vp_base + 2, (u8 *)&treg[2], 1);
		read_dgus_vp(vp_base + 3, (u8 *)&treg[3], 1);

		Modbus_Buffer[Modbus_Len++] = 0x10;
		Modbus_Buffer[Modbus_Len++] = (u8)(addr >> 8);
		Modbus_Buffer[Modbus_Len++] = (u8)(addr & 0xFF);
		Modbus_Buffer[Modbus_Len++] = 0;
		Modbus_Buffer[Modbus_Len++] = 4;
		Modbus_Buffer[Modbus_Len++] = 8;
		for(ti = 0; ti < 4; ti++)
			Modbus_AppendU16(&Modbus_Len, treg[ti]);
	}
	break;

	}
	Modbus_Crc = CalculateModbusCrc(Modbus_Buffer,Modbus_Len);
	Modbus_Buffer[Modbus_Len++]=(u8)Modbus_Crc;
	Modbus_Buffer[Modbus_Len++]=(u8)(Modbus_Crc>>8);

	if(!modbus_error && ((Modbus_Write_6 && !Modbus_Write_6_Pending) || (Modbus_Write_5 && !Modbus_Write_5_Pending) || (Modbus_Write_Timer && !Modbus_Write_Timer_Pending) || ((Send_Count >= MODBUS_POLL_GAP_MS) && !Modbus_Write_6 && !Modbus_Write_5 && !Modbus_Write_Timer) || (Modbus_Write_Ueser && !Modbus_Write_Ueser_Pending) || (Modbus_Write_DefrostParam && !Modbus_Write_DefrostParam_Pending)))
		{
	if(!nUart5Sending)
		{
		if(Modbus_Buffer[1] == 0x03)
			Read_Or_Write = 1;
		else
			Read_Or_Write = 0;
		dbg_addr = ((u16)Modbus_Buffer[2] << 8) | (u16)Modbus_Buffer[3];
		Modbus_Dbg_SendType = send_type;
		Modbus_Dbg_FuncCode = Modbus_Buffer[1];
		Modbus_Dbg_LastTxAddr = dbg_addr;
		Modbus_Dbg_LastTxRw = (Modbus_Buffer[1] == 0x03) ? 1 : 0;
		if(send_type == 11)
			Modbus_Dbg_LastTxUnit = s_indoor_wr_unit;
		else if(send_type == 8)
			Modbus_Dbg_LastTxUnit = Indoor_Unit_Index;
#if MODBUS_DBG_ON
		Modbus_DbgLogTx(dbg_addr, Modbus_Dbg_LastTxRw, Modbus_Len);
#endif
																	UART5_SendStr(Modbus_Buffer,Modbus_Len);//发送数据F
		if(Modbus_Write_Ueser)
			Modbus_Write_Ueser_Pending = 1;
		if(Modbus_Write_DefrostParam)
			Modbus_Write_DefrostParam_Pending = 1;
		if(Modbus_Write_5)
			Modbus_Write_5_Pending = 1;
		if(Modbus_Write_6)
			Modbus_Write_6_Pending = 1;
		if(Modbus_Write_Timer)
			Modbus_Write_Timer_Pending = 1;
		Send_Count = 0;
		modbus_error = 1;
	}

	}

	if(!modbus_error)
	{
		Modbus_Error_Count = 0;
	}
	else if(Modbus_Error_Count >= MODBUS_ERROR)
	{
		error = 23;
		Modbus_ReportFaultCode(error);
																	AllErrorBit[1] |= 0x0040;//通讯故障： 在Error2的第6位，从位数从1开始
	}

	Modbus_YieldWiFi();

modbus_done:
	s_modbus_depth--;
}

u16 GetModbusReg(u8 *rx, u16 reg)
{
	u16 index = (reg - 200) * 2;

	return ((u16)rx[3 + index] << 8) |
	((u16)rx[4 + index]);
}

static u16 Modbus_RxU16(u8 *rx, u8 byte_off)
{
	return ((u16)rx[3 + (u16)byte_off] << 8) | (u16)rx[4 + byte_off];
}

/* 读回复数据区第 reg_index 个寄存器（相对本次读起始地址，0 起） */
static u16 Modbus_RxRelReg(u8 *rx, u8 reg_index)
{
	return Modbus_RxU16(rx, (u8)(reg_index * 2));
}

/* 内机开关/模式 VP 401x/470x：仅 11000 系列回写；hold 期间跳过 */
static void Modbus_ApplyIndoorRunVp(u8 uidx, u16 dgus_power, u16 dgus_mode)
{
	if(uidx >= INDOOR_UNIT_NUM)
		return;
	if(dgus_power > 1)
		dgus_power = 1;
	if(s_indoor_pwr_hold[uidx] > 0)
	{
		if(dgus_power == s_indoor_hold_pwr[uidx])
			s_indoor_pwr_hold[uidx] = 0;
		else
			return;
	}
	write_dgus_vp((u32)(0x4010 + uidx), (u8 *)&dgus_power, 1);
	write_dgus_vp((u32)(0x4700 + uidx), (u8 *)&dgus_mode, 1);
}

static u16 Modbus_TxU16(u8 *buf, u8 byte_off)
{
	return ((u16)buf[byte_off] << 8) | (u16)buf[byte_off + 1];
}

#if MODBUS_DBG_ON
static u8 Modbus_DbgIsIndoorRoomPoll(u16 addr)
{
	return (addr >= 1000 && addr <= 1500 && ((addr - 1000) % 100) == 0) ? 1 : 0;
}

static void Modbus_DbgLogTx(u16 addr, u8 is_rd, u16 len)
{
	u16 d0, d1;
	if(is_rd)
	{
		if(Modbus_DbgIsIndoorRoomPoll(addr))
			return;
		printf("[MODBUS] rd%u\r\n", (unsigned)addr);
	}
	else
	{
		d0 = (len >= 9) ? Modbus_TxU16(Modbus_Buffer, 7) : 0;
		d1 = (len >= 11) ? Modbus_TxU16(Modbus_Buffer, 9) : 0;
		printf("[MODBUS] wr%u v0=%u v1=%u\r\n", (unsigned)addr, (unsigned)d0, (unsigned)d1);
	}
}

static void Modbus_DbgLogRx(u16 addr, u8 is_rd, u8 *rx)
{
	u16 d0, d1;
	if(is_rd)
	{
		if(Modbus_DbgIsIndoorRoomPoll(addr))
		{
			u16 t7 = Modbus_RxRelReg(rx, 7);
			printf("[MODBUS] rd%u room=%.1fC\r\n",
				(unsigned)addr, (float)t7 / 10.0f);
			return;
		}
		d0 = Modbus_RxU16(rx, 0);
		d1 = Modbus_RxU16(rx, 2);
		printf("[MODBUS] rd%u v0=%u v1=%u\r\n", (unsigned)addr, (unsigned)d0, (unsigned)d1);
	}
	else
		printf("[MODBUS] wr%u ok\r\n", (unsigned)addr);
}
#endif /* MODBUS_DBG_ON */

void Modbus_Analysis_Data1(void)
{
	u16 modbus_addr = 0;
	ERC_DATA  u8tou16;
	ERC_DATA1	u8tos16;
	u16	Modbus_Analysis_TempType;
	u16	Modbus_Analysis_Onoff;
	u16	Modbus_Analysis_mode;
	u16	Modbus_Analysis_ModeTemp;
	u16	Modbus_Analysis_SetTemp;
	u16	Modbus_Analysis_FanMode;
	u16	Modbus_Temp1;//获取到的温度为实际温度*10，需转换
	static	u16	SetTempOld;
	static	u16	modeOld;
	u16 i = 0, j=0;

	u16	modbus_rec_len = 0;
	u16	modbus_rec_CRC = 0;
	u16	modbus_calculate_CRC = 0;
	float	ftemp;
	u16	Error_Code;
	u16	Error_Cnt = 0;
	unsigned short	NewError[4]= {0};
	unsigned short Error_Temp=0;
	if(modbus_res_finish == 1)//接收完毕
	{
		modbus_res_finish = 0;
		Modbus_YieldWiFi();
		//                      Analysis_OK = 1;         //解析完成标志
		//                      modbus_send_finish = 0;//发送完毕标志清零

		//Uart5_Rx[0] = 0x7e;//解决接收buf第一位被清零问题20230113F
		//读和写命令的长度读取不一样，需要区分
		if(Modbus_Dbg_LastTxRw == 1)
		{
			modbus_rec_len = (Uart5_Rx[2]) + 5;//接收数据长度F
		}
		else
		{
			modbus_rec_len = 8;//接收数据长度F
		}

		//接收的校验和
		u8tou16.temp[0] = Uart5_Rx[(modbus_rec_len - 1)];
		u8tou16.temp[1] = Uart5_Rx[(modbus_rec_len - 2)];
		modbus_rec_CRC = u8tou16.all;
		//计算的检验和
		modbus_calculate_CRC = CalculateModbusCrc(Uart5_Rx,(modbus_rec_len - 2));

		//若计算的校验和等于接收的校验和，则解析
		if(modbus_rec_CRC == modbus_calculate_CRC)
		{
			modbus_addr = Modbus_Dbg_SendType;
			send_type = 0;
			if(Modbus_Dbg_LastTxRw)
			{
#if MODBUS_DBG_ON
				Modbus_DbgLogRx(Modbus_Dbg_LastTxAddr, 1, Uart5_Rx);
#endif
			}
			else
			{
#if MODBUS_DBG_ON
				Modbus_DbgLogRx(Modbus_Dbg_LastTxAddr, 0, Uart5_Rx);
#endif
			}
		}
		else
		{
			modbus_addr = 0xFFFF;
#if MODBUS_DBG_ON
			printf("[MODBUS] RX crc err addr=%u\r\n", Modbus_Dbg_LastTxAddr);
#endif
			if(Modbus_Dbg_LastTxRw && Modbus_Dbg_SendType == 5)
			{
				Modbus_Read_User = FALSE;
				Modbus_Read_Check = TRUE;
			}
		}

	if(Modbus_Dbg_LastTxRw == 1)//读命令的回复才解析
	{
		switch(modbus_addr)
		{
			case	5:
				if(!Modbus_Write_Ueser)//下发100后暂停解码读100的返回数据 等待写100成功后恢复F
				{
					u16 power_vp;
					u16 active_mode;

					u16 old4021;

					u8tou16.temp[0] = Uart5_Rx[3];
					u8tou16.temp[1] = Uart5_Rx[4];
					Modbus_Analysis_Onoff = u8tou16.all;
					power_vp = Modbus_Analysis_Onoff ? 1 : 0;
					if(s_user_power_hold > 0)
					{
						if(power_vp == s_user_hold_pwr)
							s_user_power_hold = 0;
					}
					else
					{
						read_dgus_vp((u32)VP_EXT_PWR, (u8 *)&old4021, 1);
						if(old4021 > 1)
							old4021 = 1;
						if(old4021 != power_vp)
						{
							write_dgus_vp((u32)VP_EXT_PWR, (u8 *)&power_vp, 1);
							App_ControlAckExtPowerVp(power_vp);
							App_ControlBlockExtInput(8);
						}
					}

					//设定控制模式
					read_dgus_vp((u32)(0x2002),(u8 *)&modeOld,1);
					u8tou16.temp[0] = Uart5_Rx[5];
					u8tou16.temp[1] = Uart5_Rx[6];
					Modbus_Analysis_ModeTemp = u8tou16.all;
					if(s_user_power_hold == 0 && modeOld != Modbus_Analysis_ModeTemp)
					{
						write_dgus_vp((u32)(0x2002),(u8*)&Modbus_Analysis_ModeTemp,1);
						Ready_To_Save();
						upload_dp_mode();
					}

					read_dgus_vp((u32)SetTempIntVpByMode(1, 0),(u8 *)&SetTempOld,1);
					u8tou16.temp[0] = Uart5_Rx[7];
					u8tou16.temp[1] = Uart5_Rx[8];
					Modbus_Analysis_SetTemp = u8tou16.all/10;
					if(Modbus_Analysis_SetTemp)
					{
						Modbus_Analysis_SetTemp = Control_ClampSetTempCByMode(1, Modbus_Analysis_SetTemp);
					if(Modbus_ShouldApplyRd100SetTemp(1, Modbus_Analysis_SetTemp)
						&& SetTempOld != Modbus_Analysis_SetTemp){
						SetTempIntWrite(1, 0, Modbus_Analysis_SetTemp);
						SetTempIntWrite(1, 1, (u16)TempUnitTrans((signed short)Modbus_Analysis_SetTemp, 'F'));
						read_dgus_vp((u32)(0x2002), (u8 *)&active_mode, 1);
						if(active_mode == 1)
							SyncSetTempCachesFromC(Modbus_Analysis_SetTemp);
						Ready_To_Save();
						if(active_mode == 1)
							upload_dp_temp_set();
					}
					}

					read_dgus_vp((u32)SetTempIntVpByMode(2, 0),(u8 *)&SetTempOld,1);
					u8tou16.temp[0] = Uart5_Rx[9];
					u8tou16.temp[1] = Uart5_Rx[10];
					Modbus_Analysis_SetTemp = u8tou16.all/10;
					if(Modbus_Analysis_SetTemp)
					{
						Modbus_Analysis_SetTemp = Control_ClampSetTempCByMode(2, Modbus_Analysis_SetTemp);
					if(Modbus_ShouldApplyRd100SetTemp(2, Modbus_Analysis_SetTemp)
						&& SetTempOld != Modbus_Analysis_SetTemp){
						SetTempIntWrite(2, 0, Modbus_Analysis_SetTemp);
						SetTempIntWrite(2, 1, (u16)TempUnitTrans((signed short)Modbus_Analysis_SetTemp, 'F'));
						read_dgus_vp((u32)(0x2002), (u8 *)&active_mode, 1);
						if(active_mode == 2)
							SyncSetTempCachesFromC(Modbus_Analysis_SetTemp);
						Ready_To_Save();
						if(active_mode == 2)
							upload_dp_temp_set();
					}
					}

					read_dgus_vp((u32)SetTempIntVpByMode(3, 0),(u8 *)&SetTempOld,1);
					u8tou16.temp[0] = Uart5_Rx[21];
					u8tou16.temp[1] = Uart5_Rx[22];
					Modbus_Analysis_SetTemp = u8tou16.all/10;
					if(Modbus_Analysis_SetTemp)
					{
						Modbus_Analysis_SetTemp = Control_ClampSetTempCByMode(3, Modbus_Analysis_SetTemp);
					if(Modbus_ShouldApplyRd100SetTemp(3, Modbus_Analysis_SetTemp)
						&& SetTempOld != Modbus_Analysis_SetTemp){
						SetTempIntWrite(3, 0, Modbus_Analysis_SetTemp);
						SetTempIntWrite(3, 1, (u16)TempUnitTrans((signed short)Modbus_Analysis_SetTemp, 'F'));
						read_dgus_vp((u32)(0x2002), (u8 *)&active_mode, 1);
						if(active_mode == 3)
							SyncSetTempCachesFromC(Modbus_Analysis_SetTemp);
						Ready_To_Save();
						if(active_mode == 3)
							upload_dp_temp_set();
					}
					}

				}

				Control_MirrorAllExtSetTempDisplay();

				Modbus_Read_User = FALSE;
				Modbus_Read_Check = TRUE;
				Modbus_YieldWiFi();
			break;
			case	4://200地址
			{
				read_dgus_vp((u32)(0x2003),(u8 *)&Modbus_Analysis_TempType,1);
				if(Modbus_Analysis_TempType)
				{
					//							xuantest2=5;
					//							write_dgus_vp((u32)(0x5054),(u8*)&xuantest2,2);

					for(i=0;i<CHECK_PARAMETER_NUM;i++)
					{
						u8tou16.temp[0] = Uart5_Rx[3+i*2];
						u8tou16.temp[1] = Uart5_Rx[4+i*2];
						if(IsCheckParameterTemperature[i])//获取到的温度为实际温度*10，需转换
						{
							Modbus_Temp1=0;
							Modbus_Temp1 =u8tou16.all/10;//获取到的温度为实际温度*10，需转换
							u8tou16.all = TempUnitTrans(Modbus_Temp1,'F');
							write_dgus_vp((u32)(0x3400+2*i),(u8*)&u8tou16.all,2);
						}
						else
						{
							write_dgus_vp((u32)(0x4400+2*i),(u8*)&u8tou16.all,2);
						}
						if((i & 3) == 3)
							Modbus_YieldWiFi();
					}
				}
				else
				{
					//							xuantest1=4;
					//							write_dgus_vp((u32)(0x5050),(u8*)&xuantest1,2);
					for(i=0;i<CHECK_PARAMETER_NUM;i++)
					{
						u8tou16.temp[0] = Uart5_Rx[3+i*2];
						u8tou16.temp[1] = Uart5_Rx[4+i*2];

						if(IsCheckParameterTemperature[i])//获取到的温度为实际温度*10，需转换
						{
							Modbus_Temp1=0;
							Modbus_Temp1 =u8tou16.all/10;
							write_dgus_vp((u32)(0x4400+2*i),(u8*)&Modbus_Temp1,2);
						}
						else
						{
							write_dgus_vp((u32)(0x4400+2*i),(u8*)&u8tou16.all,2);
						}
						if((i & 3) == 3)
							Modbus_YieldWiFi();

					}
				}

				Modbus_PackCheckStatusBlock(Uart5_Rx, CHECK_PARAMETER_NUM, IsCheckParameterTemperature, 0);

				Modbus_Analysis_SetTemp = GetModbusReg(Uart5_Rx, 218) / 10;
				if(Modbus_WriteFloatTempPair(0x4800, 0x4810, (float)Modbus_Analysis_SetTemp)
					&& upload_is_boot_ready())
					upload_dp_around_temp();

				Modbus_Analysis_SetTemp = GetModbusReg(Uart5_Rx, 220) / 10;
				if(Modbus_WriteFloatTempPair(0x4802, 0x4812, (float)Modbus_Analysis_SetTemp)
					&& upload_is_boot_ready())
					upload_dp_temp_current();

				Modbus_Analysis_SetTemp = GetModbusReg(Uart5_Rx, 221) / 10;
				if(Modbus_WriteFloatTempPair(0x4804, 0x4814, (float)Modbus_Analysis_SetTemp)
					&& upload_is_boot_ready())
					upload_dp_temp_top();

				Modbus_Analysis_mode = GetModbusReg(Uart5_Rx, 214);
				s_reg214_cfg = Modbus_Analysis_mode;
				Modbus_SyncIndoorInstalledCount(s_reg214_cfg);

				{
					u16 cnt215;

					cnt215 = GetModbusReg(Uart5_Rx, 215);
					if(cnt215 > INDOOR_UNIT_NUM)
						cnt215 = INDOOR_UNIT_NUM;
					write_dgus_vp((u32)VP_INDOOR_ON_COUNT, (u8 *)&cnt215, 1);
#if MODBUS_DBG_ON
					printf("[MODBUS] rd200 reg214=%u reg215=%u ->4016\r\n",
						(unsigned)GetModbusReg(Uart5_Rx, 214),
						(unsigned)cnt215);
#endif
				}

				/* reg205=外机除霜状态：0非除霜 1除霜中（非温度） */
				Modbus_SyncDefrostFromReg205(GetModbusReg(Uart5_Rx, 205));

				Modbus_Read_Check = FALSE;
				Modbus_Read_Check_2 = TRUE;
				Modbus_YieldWiFi();
			}
			break;

			case	6://300地址

				//点检数据1的数量为25，如果有修改，需要更新页面的显示
				read_dgus_vp((u32)(0x2003),(u8 *)&Modbus_Analysis_TempType,1);
				if(Modbus_Analysis_TempType)
				{
					for(i=0;i<CHECK_PARAMETER_NUM_2;i++)
					{
						u8tou16.temp[0] = Uart5_Rx[3+i*2];
						u8tou16.temp[1] = Uart5_Rx[4+i*2];
						if(IsCheckParameterTemperature_2[i])//获取到的温度为实际温度*10，需转换
						{
							Modbus_Temp1=0;
							Modbus_Temp1 =u8tou16.all/10;//获取到的温度为实际温度*10，需转换
							u8tou16.all = TempUnitTrans(Modbus_Temp1,'F');
							write_dgus_vp((u32)(0x3400+CHECK_PARAMETER_NUM*2+2*i),(u8*)&u8tou16.all,2);
						}
						else
						{
							write_dgus_vp((u32)(0x4400+CHECK_PARAMETER_NUM*2+2*i),(u8*)&u8tou16.all,2);
						}
						if((i & 3) == 3)
							Modbus_YieldWiFi();
					}
				}
				else
				{
					for(i=0;i<CHECK_PARAMETER_NUM_2;i++)
					{
						u8tou16.temp[0] = Uart5_Rx[3+i*2];
						u8tou16.temp[1] = Uart5_Rx[4+i*2];

						if(IsCheckParameterTemperature_2[i])//获取到的温度为实际温度*10，需转换
						{
							Modbus_Temp1=0;
							Modbus_Temp1 =u8tou16.all/10;
							write_dgus_vp((u32)(0x4400+CHECK_PARAMETER_NUM*2+2*i),(u8*)&Modbus_Temp1,2);
						}
						else
						{
							write_dgus_vp((u32)(0x4400+CHECK_PARAMETER_NUM*2+2*i),(u8*)&u8tou16.all,2);
						}
						if((i & 3) == 3)
							Modbus_YieldWiFi();
					}
				}

				//??????
				Error_Cnt=0;
				for(i=0;i<4;i++)
				{
					u8tou16.temp[0] = Uart5_Rx[3+(40+i)*2];
					u8tou16.temp[1] = Uart5_Rx[4+(40+i)*2];
					AllErrorBit[i] = u8tou16.all;
					NewError[i]=u8tou16.all;

					//????????
					if( u8tou16.all )
					{
						Error_Cnt ++;
					}
				}

				if( Error_Cnt >1 )//存在多个错误时，报其他故障
				{
					Error_Temp=0;
					Error_Temp = 47;
					read_dgus_vp((u32)(0x4607),(u8 *)&modeOld,1);
					if(modeOld != Error_Temp)
					{
						write_dgus_vp((u32)(0x4607),(u8*)&Error_Temp,1);
						if(upload_is_boot_ready())
							upload_dp_fault();
					}
				}
				else
				{
					for(i=0;i<4;i++)
					{
						for(j=0;j<16;j++)
						{
							if( ((NewError[i] >> j) & 0x01) )
							{
								Error_Temp=0;
								Error_Temp = i*16+j+1;
								read_dgus_vp((u32)(0x4607),(u8 *)&modeOld,1);
								if(modeOld != Error_Temp)
								{
									write_dgus_vp((u32)(0x4607),(u8*)&Error_Temp,1);
									if(upload_is_boot_ready())
										upload_dp_fault();
								}
							}
						}
					}
				}	

				Modbus_PackCheckStatusBlock(Uart5_Rx, CHECK_PARAMETER_NUM_2, IsCheckParameterTemperature_2, CHECK_PARAMETER_NUM);
				if(upload_is_boot_ready())
					upload_check_status();

				u8tos16.temp[0] = Uart5_Rx[9];
				u8tos16.temp[1] = Uart5_Rx[10];
				ftemp = (float)u8tos16.all/10;
				{
					float old_e;

					read_dgus_vp((u32)(0x1006),(u8 *)&old_e,2);
					if((u16)old_e != (u16)ftemp)
					{
						write_dgus_vp((u32)(0x1006),(u8*)&ftemp,2);
						ftemp = TempUnitTrans((unsigned short)ftemp,'F');
						write_dgus_vp((u32)(0x1008),(u8*)&ftemp,2);
						if(upload_is_boot_ready())
							upload_dp_evaporator_temp();
					}
				}

				u8tos16.temp[0] = Uart5_Rx[21];
				u8tos16.temp[1] = Uart5_Rx[22];
				if(Modbus_WriteFloatTempPair(0x4806, 0x4816, (float)(u8tos16.all / 10))
					&& upload_is_boot_ready())
					upload_dp_effluent_temp();


				Modbus_Read_Check_2 = FALSE;
				Modbus_StartIndoorPollAfterCheck2();
				Modbus_YieldWiFi();
			break;
			case 9:
			{
				u16 temp_c;
				u16 freq_raw, time_raw, t3_raw;
				u16 vp_u16;

				u8tos16.temp[0] = Uart5_Rx[3];
				u8tos16.temp[1] = Uart5_Rx[4];
				freq_raw = u8tos16.all;
				read_dgus_vp((u32)(0x3660), (u8 *)&vp_u16, 1);
				if(vp_u16 != freq_raw)
				{
					Modbus_Analysis_mode = freq_raw;
					write_dgus_vp((u32)(0x1010),(u8*)&Modbus_Analysis_mode,2);
					write_dgus_vp((u32)(0x3660),(u8*)&Modbus_Analysis_mode,1);
					if(upload_is_boot_ready())
						upload_dp_defrost_freq();
				}

				u8tos16.temp[0] = Uart5_Rx[5];
				u8tos16.temp[1] = Uart5_Rx[6];
				time_raw = u8tos16.all;
				read_dgus_vp((u32)(0x3670), (u8 *)&vp_u16, 1);
				if(vp_u16 != time_raw)
				{
					Modbus_Analysis_mode = time_raw;
					write_dgus_vp((u32)(0x1012),(u8*)&Modbus_Analysis_mode,2);
					write_dgus_vp((u32)(0x3670),(u8*)&Modbus_Analysis_mode,1);
					if(upload_is_boot_ready())
						upload_dp_defrost_time();
				}

				u8tos16.temp[0] = Uart5_Rx[7];
				u8tos16.temp[1] = Uart5_Rx[8];
				t3_raw = u8tos16.all;
				temp_c = (u16)(t3_raw / 10);
				read_dgus_vp((u32)(0x3680), (u8 *)&vp_u16, 1);
				if(vp_u16 != temp_c)
				{
					ftemp = (float)temp_c;
					write_dgus_vp((u32)(0x1014),(u8*)&ftemp,2);
					ftemp = (float)TempUnitTrans((signed short)temp_c, 'F');
					write_dgus_vp((u32)(0x101a),(u8*)&ftemp,2);
					write_dgus_vp((u32)(0x3680),(u8*)&temp_c,1);
					if(upload_is_boot_ready())
						upload_dp_defrost_out_temp();
				}
				s_check_status[70] = Modbus_PackCheckStatusByte(freq_raw, 0);
				s_check_status[71] = Modbus_PackCheckStatusByte(time_raw, 0);
				s_check_status[72] = Modbus_PackCheckStatusByte(t3_raw, 1);
				if(upload_is_boot_ready())
					upload_check_status();
			}
			Modbus_Read_Check_4 = FALSE;
			Indoor_Unit_Index = 0;
			break;
#if !MODBUS_SKIP_RD1000_POLL
			case 8:
			{
				u16 reg_val;
				u8 uidx;

				if(Modbus_Dbg_LastTxAddr >= 1000)
					uidx = (u8)((Modbus_Dbg_LastTxAddr - 1000) / 100);
				else
					uidx = Modbus_Dbg_LastTxUnit;
				if(uidx >= 6)
					uidx = 0;

				/* 1000 系列：仅读 n*100+1007 内机室内温度，不写 401x/470x/471x/102x */
				reg_val = Modbus_RxRelReg(Uart5_Rx, 7);
				if(reg_val > 0)
				{
					ftemp = (float)reg_val / 10.0f;
					Modbus_WriteFloatTempPair(
						(u32)(VP_INDOOR_ROOM_C_BASE + (u16)uidx * 2),
						(u32)(VP_INDOOR_ROOM_F_BASE + (u16)uidx * 2),
						ftemp);
					write_dgus_vp((u32)(0x1100 + (u16)uidx * 0x20), (u8 *)&ftemp, 2);
				}

				if(uidx + 1 < Modbus_IndoorPollLimit())
				{
					Indoor_Unit_Index = (u8)(uidx + 1);
					Modbus_Read_Check_3 = TRUE;
				}
				else
				{
					Indoor_Unit_Index = 0;
					Modbus_Read_Check_3 = FALSE;
#if MODBUS_SKIP_RD12_POLL
					Modbus_Read_Check_4 = TRUE;
#else
					Modbus_Read_Check_5 = TRUE;
#endif
				}
			}
		break;
#endif
		case 12:{
			u16 reg_val;
			u16 dgus_power;
			u16 dgus_mode;
			u16 dgus_fan;
			u16 temp_c;
			u8 uidx;

			if(Modbus_Dbg_LastTxAddr >= 11000)
				uidx = (u8)((Modbus_Dbg_LastTxAddr - 11000) / 30);
			else
				uidx = Indoor_Unit_Index;
			if(uidx >= INDOOR_UNIT_NUM)
				uidx = 0;

			if(!Modbus_Indoor11000BlockEmpty(Uart5_Rx))
				s_indoor_seen[uidx] = 1;

			reg_val = Modbus_RxRelReg(Uart5_Rx, 0);
			dgus_power = (reg_val & 0x0080) ? 1 : 0;
			s_mb_unit_on[uidx] = (u8)dgus_power;

			if(!Modbus_Indoor11000BlockEmpty(Uart5_Rx))
			{
				dgus_mode = Modbus_DecodeIndoorModeBits(reg_val);
				Modbus_ApplyIndoorRunVp(uidx, dgus_power, dgus_mode);
				Modbus_SyncHomeIndoorModeFromUnit(uidx, dgus_mode);

				reg_val = Modbus_RxRelReg(Uart5_Rx, 1);
				dgus_fan = Modbus_DecodeIndoorFan(reg_val);
				write_dgus_vp((u32)(0x4710 + uidx), (u8 *)&dgus_fan, 1);

				reg_val = Modbus_RxRelReg(Uart5_Rx, 2);
				temp_c = Modbus_UnscaleIndoorSetTemp(uidx, reg_val);
				if(temp_c >= 16 && temp_c <= 32)
					write_dgus_vp((u32)(0x1020 + uidx), (u8 *)&temp_c, 1);
			}

			if(uidx + 1 < Modbus_IndoorPollLimit())
			{
				Indoor_Unit_Index = (u8)(uidx + 1);
				Modbus_Read_Check_5 = TRUE;
			}
			else
			{
				Indoor_Unit_Index = 0;
				Modbus_Read_Check_5 = FALSE;
				Modbus_Read_Check_4 = TRUE;
				Modbus_SyncIndoorInstalledCount(s_reg214_cfg);
				Modbus_SyncIndoorOnCountVp();
			}
		}
		break;
		}

	}
	else
	{
		send_type = 0;
		if( modbus_addr != 0xFFFF)
		{
			modbus_addr=0;
			modbus_addr =(Uart5_Rx[2]<<8)|Uart5_Rx[3];
		}

		switch(modbus_addr)
		{
			case	100:
				Modbus_Write_Ueser = FALSE;
				Modbus_Write_Ueser_Pending = 0;
				Modbus_Write_Defrost = FALSE;
				Modbus_RefreshSetTempHoldAfterWr100();
				if(g_mb_switch_wr_src != 0)
				{
					App_OnWr100Done((u8)s_user_hold_pwr, g_mb_switch_wr_src);
					g_mb_switch_wr_src = 0;
				}
				if(s_user_settemp_dirty)
					Modbus_FlushSetTempUserWrite();
				if(!Modbus_Read_Check && !Modbus_Read_Check_2 && !Modbus_Read_Check_3
					&& !Modbus_Read_Check_4 && !Modbus_Read_Check_5)
					Modbus_Read_Check = TRUE;
			break;

			case	15000:
				Modbus_Write_5 = FALSE;
				Modbus_Write_5_Pending = 0;
				Modbus_SaveGroupSentRegs(s_group_regs);
				if(Modbus_AllIndoorOffVp())
				{
					for(i = 0; i < INDOOR_UNIT_NUM; i++)
						s_indoor_sent_valid[i] = 0;
				}
				Modbus_FlushIndoorWritePending();
			break;

			case	11000:
			case    11030:
			case    11060:
			case    11090:
			case    11120:
			case    11150:
			Modbus_Write_6_Pending = 0;
			Modbus_SaveIndoorSentRegs(s_indoor_wr_unit, s_indoor_regs);
			Modbus_Write_6 = FALSE;
			Modbus_Write_6_All = 0;
			Modbus_FlushIndoorWritePending();
			if(!Modbus_Write_6 && !s_indoor_wr_pending_mask)
				Modbus_Read_User = TRUE;
		break;

		case	11004:
		case    11034:
		case    11064:
		case    11094:
		case    11124:
		case    11154:
			Modbus_Write_Timer = FALSE;
			Modbus_Write_Timer_Pending = 0;
			if(s_timer_q_pos < s_timer_q_len)
				Modbus_StartNextTimerWrite();
			else
		break;

		case	30366:
			Modbus_Write_DefrostParam = FALSE;
			Modbus_Write_DefrostParam_Pending = 0;
		break;
		}
	}
	}
}

void Modbus_RefreshCheckParamDisplay(void)
{
	u8 i;
	u16 raw;
	signed short temp_c;
	u16 temp_f;
	u16 unit;
	read_dgus_vp((u32)(0x2003), (u8 *)&unit, 1);
	for(i = 0; i < CHECK_PARAMETER_NUM; i++)
	{
			read_dgus_vp((u32)(0x4400 + 2 * i), (u8 *)&raw, 1);
			if(IsCheckParameterTemperature[i])
			{
				temp_c = (signed short)(raw / 10);
				temp_f = (u16)TempUnitTrans(temp_c, 'F');
				if(unit)
					write_dgus_vp((u32)(0x3400 + 2 * i), (u8 *)&temp_f, 1);
			}
			if((i & 3) == 3)
				WDT_RST();
	}
	for(i = 0; i < CHECK_PARAMETER_NUM_2; i++)
	{
		read_dgus_vp((u32)(0x4400 + CHECK_PARAMETER_NUM * 2 + 2 * i), (u8 *)&raw, 1);
		if(IsCheckParameterTemperature_2[i])
		{
			temp_c = (signed short)(raw / 10);
			temp_f = (u16)TempUnitTrans(temp_c, 'F');
			if(unit)
				write_dgus_vp((u32)(0x3400 + CHECK_PARAMETER_NUM * 2 + 2 * i), (u8 *)&temp_f, 1);
		}
		if((i & 3) == 3)
			WDT_RST();
	}
}
