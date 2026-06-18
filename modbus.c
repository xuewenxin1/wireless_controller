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
extern u16 Defrosting;


/********************************
*  0：没有接收数据
*  0xA5:开始接收数据
*  0x5A00:正常接收数据
*  0x5AA5:modbus数据接收完成
********************************/
u8	Modbus_Buffer[MODBUS_BUFFER_LEN] = 0;

bit	Modbus_Read_Initial = TRUE;
bit	Modbus_Read_UpperLimit,Modbus_Read_LowerLimit,Modbus_Read_Check,Modbus_Read_Check_2,Modbus_Read_Check_3,Modbus_Read_Check_4,Modbus_Read_Check_5,Modbus_Read_User,Modbus_Read_Set,Modbus_Write_Ueser,Modbus_Write_Set,Modbus_Read_Check_4_old,Modbus_Write_5,Modbus_Write_6,Modbus_Write_6_All;
bit	Modbus_Read_Error,Modbus_Write_ClearPower,Modbus_Write_Defrost;
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

bit	First_Write600 = TRUE;

signed	short	SetParameterUpperLimit[SET_PARAMETER_NUM];
signed	short	SetParameterLowerLimit[SET_PARAMETER_NUM];
unsigned	char	IsCheckParameterTemperature[CHECK_PARAMETER_NUM] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1};
unsigned	char	IsCheckParameterTemperature_2[CHECK_PARAMETER_NUM_2] = {0,0,0,1,1,1,1,1,1,1,1,0,1,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
//unsigned char ver[6] = {'0','.','0','.','1','\0'};

u8	send_type = 0;
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
		read_dgus_vp((u32)(0x4808U + (u32)(mode - 1) * 2U), (u8 *)&temp, 1);
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
		case 0: return 0;
		case 1: return 2; /* 送风 */
		case 2: return 4;
		case 3: return 6;
		default: return 8;
	}
}

#define INDOOR_UNIT_DATA_REG_NUM	8
#define INDOOR_INFO_REG_NUM			8

static u8 s_check_status[MODBUS_CHECK_STATUS_LEN];
static u16 s_indoor_regs[INDOOR_UNIT_REG_NUM];
static u16 s_indoor_sent_regs[INDOOR_UNIT_NUM][INDOOR_UNIT_DATA_REG_NUM];
static u8 s_indoor_sent_valid[INDOOR_UNIT_NUM];
static bit Modbus_Write_6_Pending = 0;

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
	regs[2] = temp;

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

#define GROUP_CONTROL_REG_NUM	24

static u16 s_group_regs[GROUP_CONTROL_REG_NUM];
static u16 s_group_sent_regs[GROUP_CONTROL_REG_NUM];
static u8 s_group_sent_valid = 0;
static bit Modbus_Write_5_Pending = 0;

static void Modbus_FillGroupControlRegs(u16 *regs)
{
	u8 i;
	u16 val;

	for(i = 0; i < 7; i++)
		regs[i] = 0xFF;

	read_dgus_vp((u32)(0x3020), (u8 *)&val, 1);
	regs[7] = val;

	read_dgus_vp((u32)(0x1090), (u8 *)&val, 1);
	regs[8] = val;

	read_dgus_vp((u32)(0x3025), (u8 *)&val, 1);
	regs[9] = val;

	for(i = 10; i < GROUP_CONTROL_REG_NUM; i++)
		regs[i] = 0;
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
		Send_Count = 200;
	}
}

static void Modbus_ReportFaultCode(u16 fault_code)
{
	write_dgus_vp((u32)(0x4607),(u8 *)&fault_code,1);
	mcu_dp_value_update(DPID_FAULT_VALUE,fault_code);
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

static void Modbus_WriteFloatTempPair(u32 vp_c, u32 vp_f, float temp_c)
{
	float temp_f;

	temp_f = (float)TempUnitTrans((signed short)temp_c, 'F');
	write_dgus_vp(vp_c, (u8 *)&temp_c, 2);
	write_dgus_vp(vp_f, (u8 *)&temp_f, 2);
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

void Modbus_StartManualDefrost(void)
{
	u16 one = 1;

	s_defrost_want = 1;
	s_defrost_verify = 0;
	Modbus_DefrostRetryLeft = 3;
	Defrosting = TRUE;
	write_dgus_vp((u32)(0x201b), (u8 *)&one, 1);
	Modbus_Write_Defrost = TRUE;
	Modbus_Write_Ueser = TRUE;
	Send_Count = 200;
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
	Send_Count = 200;
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
		read_dgus_vp((u32)(0x4825), (u8 *)&on_en, 1);
		read_dgus_vp((u32)(0x1084), (u8 *)&hour, 1);
		read_dgus_vp((u32)(0x1085), (u8 *)&minute, 1);
		on_time = (u16)(((u16)hour << 8) | minute);
	}
	if(update_off)
	{
		read_dgus_vp((u32)(0x4827), (u8 *)&off_en, 1);
		read_dgus_vp((u32)(0x1086), (u8 *)&hour, 1);
		read_dgus_vp((u32)(0x1087), (u8 *)&minute, 1);
		off_time = (u16)(((u16)hour << 8) | minute);
	}

	for(i = 0; i < INDOOR_UNIT_NUM; i++)
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
}

void Modbus_TriggerIndoorUnitWrite(u8 index, u8 write_all)
{
	if(index < INDOOR_UNIT_NUM)
	{
		Modbus_FillIndoorUnitRegs(index, s_indoor_regs);
		if(write_all || Modbus_IndoorRegsChanged(index, s_indoor_regs))
		{
			Indoor_Unit_Index = index;
			Modbus_Write_6_All = write_all ? 1 : 0;
			Modbus_Write_6 = TRUE;
			Modbus_Write_6_Pending = 0;
			Send_Count = 200;
		}
	}
}

void Modbus_TriggerDefrostParamWrite(u16 vp)
{
	s_defrost_param_vp = vp;
	Modbus_Write_DefrostParam = TRUE;
	Send_Count = 200;
}
void Modbus_Salve_Handler1(void)
{
	unsigned short	Modbus_Crc;
	unsigned short	Modbus_Len = 0;
	unsigned short	Modbus_u16temp, Modbus_Power, Modbus_Mode;
	u16 error;

	Modbus_Read_Initial = FALSE;
	if(Modbus_Write_Ueser)
		send_type = 7;
	else if(Modbus_Write_Timer)
		send_type = 15;
	else if(Modbus_Write_6)
		send_type = 11;
	else if(Modbus_Write_5)
		send_type = 10;
	else if(Modbus_Write_Set)
		send_type = 13;
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
		case 8: //read 1100
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
		u16 addr = 11000 + (u16)Indoor_Unit_Index * 30;

		Modbus_FillIndoorUnitRegs(Indoor_Unit_Index, s_indoor_regs);
		Modbus_Buffer[Modbus_Len++] = 0x10;
		Modbus_Buffer[Modbus_Len++] = (u8)(addr >> 8);
		Modbus_Buffer[Modbus_Len++] = (u8)(addr & 0xFF);
		Modbus_Buffer[Modbus_Len++] = 0;
		Modbus_Buffer[Modbus_Len++] = 30;
		Modbus_Buffer[Modbus_Len++] = 60;

		for(ri = 0; ri < 8; ri++)
			Modbus_AppendU16(&Modbus_Len, s_indoor_regs[ri]);
		for(ri = 0; ri < 20; ri++)
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
	Modbus_u16temp = 0;
	Modbus_u16temp = Modbus_Power+1;
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
	case 13:
	{
		u16 reg = (u16)(800 + SetPara_Addr);
		u16 val;

		read_dgus_vp((u32)(0x4800 + SetPara_Addr), (u8 *)&val, 1);
		Modbus_Buffer[Modbus_Len++] = 0x06;
		Modbus_Buffer[Modbus_Len++] = (u8)(reg >> 8);
		Modbus_Buffer[Modbus_Len++] = (u8)(reg & 0xFF);
		Modbus_Buffer[Modbus_Len++] = (u8)(val >> 8);
		Modbus_Buffer[Modbus_Len++] = (u8)(val & 0xFF);
	}
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


	if((Modbus_Write_6 && !Modbus_Write_6_Pending) || (Modbus_Write_5 && !Modbus_Write_5_Pending) || (Modbus_Write_Timer && !Modbus_Write_Timer_Pending) || ((Send_Count >= 200) && !Modbus_Write_6 && !Modbus_Write_5 && !Modbus_Write_Timer) || Modbus_Write_Ueser || Modbus_Write_Set || Modbus_Write_DefrostParam)
		{
		if(Modbus_Buffer[1] == 0x03)
			{
													Read_Or_Write = 1;//读命令
		}
	else
		{
																	Read_Or_Write = 0;//写命令
	}
	if(!nUart5Sending)
		{
																	UART5_SendStr(Modbus_Buffer,Modbus_Len);//发送数据F
	}

	Send_Count = 0;//与上次发送或接收间隔清零

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

	modbus_error = 1;//通讯故障置1，接收数据成功会清零。
	}
	//********************************************************************//
	//********************************????***********************************//

	Modbus_Analysis_Data1();

}

u16 GetModbusReg(u8 *rx, u16 reg)
{
	u16 index = (reg - 200) * 2;

	return ((u16)rx[3 + index] << 8) |
	((u16)rx[4 + index]);
}

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
	static	u16	OnoffOld;
	static	u16	modeOld;
	u16	Modbus_Power[2];

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
		//                      Analysis_OK = 1;         //解析完成标志
		//                      modbus_send_finish = 0;//发送完毕标志清零

		//Uart5_Rx[0] = 0x7e;//解决接收buf第一位被清零问题20230113F
		//读和写命令的长度读取不一样，需要区分
		if(Read_Or_Write == 1)
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
			modbus_addr =send_type;//根据send_type来
			send_type = 0;
		}
		else
		{
			modbus_addr = 0xFFFF;
			//                              Analysis_OK = 0;         //校验和错误，解析完成标志清零，重新发
		}

	if(Read_Or_Write == 1)//读命令的回复才解析
	{
		switch(modbus_addr)
		{
			case	5:
				if(!Modbus_Write_Ueser)//下发100后暂停解码读100的返回数据 等待写100成功后恢复F
				{
					u16 power_vp;

					read_dgus_vp((u32)(0x4021),(u8 *)&OnoffOld,1);
					u8tou16.temp[0] = Uart5_Rx[3];
					u8tou16.temp[1] = Uart5_Rx[4];
					Modbus_Analysis_Onoff = u8tou16.all;
					power_vp = (Modbus_Analysis_Onoff >= 2) ? 1 : 0;
					if(OnoffOld != power_vp)
					{
						write_dgus_vp((u32)(0x4012),(u8*)&power_vp,1);
						write_dgus_vp((u32)(0x4021),(u8*)&power_vp,1);
						if(!power_vp)
						{
							Modbus_Analysis_ModeTemp = 0;
							write_dgus_vp((u32)(0x2002),(u8*)&Modbus_Analysis_ModeTemp,1);
						}
						Ready_To_Save();
					}

					//设定控制模式
					read_dgus_vp((u32)(0x2002),(u8 *)&modeOld,1);
					u8tou16.temp[0] = Uart5_Rx[5];
					u8tou16.temp[1] = Uart5_Rx[6];
					Modbus_Analysis_ModeTemp = u8tou16.all;
					if(modeOld != Modbus_Analysis_ModeTemp)
					{
						write_dgus_vp((u32)(0x2002),(u8*)&Modbus_Analysis_ModeTemp,1);
						Ready_To_Save();
					}

					read_dgus_vp((u32)(0x4808),(u8 *)&SetTempOld,1);
					u8tou16.temp[0] = Uart5_Rx[7];
					u8tou16.temp[1] = Uart5_Rx[8];
					Modbus_Analysis_SetTemp = u8tou16.all/10;
					if(SetTempOld != Modbus_Analysis_SetTemp){
						SetTempIntWrite(1, 0, Modbus_Analysis_SetTemp);
						SetTempIntWrite(1, 1, (u16)TempUnitTrans((signed short)Modbus_Analysis_SetTemp, 'F'));
						Ready_To_Save();
					}

					read_dgus_vp((u32)(0x480a),(u8 *)&SetTempOld,1);
					u8tou16.temp[0] = Uart5_Rx[9];
					u8tou16.temp[1] = Uart5_Rx[10];
					Modbus_Analysis_SetTemp = u8tou16.all/10;
					if(SetTempOld != Modbus_Analysis_SetTemp){
						SetTempIntWrite(2, 0, Modbus_Analysis_SetTemp);
						SetTempIntWrite(2, 1, (u16)TempUnitTrans((signed short)Modbus_Analysis_SetTemp, 'F'));
						Ready_To_Save();
					}

					read_dgus_vp((u32)(0x480c),(u8 *)&SetTempOld,1);
					u8tou16.temp[0] = Uart5_Rx[21];
					u8tou16.temp[1] = Uart5_Rx[22];
					Modbus_Analysis_SetTemp = u8tou16.all/10;
					if(SetTempOld != Modbus_Analysis_SetTemp){
						SetTempIntWrite(3, 0, Modbus_Analysis_SetTemp);
						SetTempIntWrite(3, 1, (u16)TempUnitTrans((signed short)Modbus_Analysis_SetTemp, 'F'));
						Ready_To_Save();
					}

					read_dgus_vp((u32)(0x201b),(u8 *)&modeOld,1);
					u8tou16.temp[0] = Uart5_Rx[15];
					u8tou16.temp[1] = Uart5_Rx[16];
					Modbus_Analysis_ModeTemp = u8tou16.all;
					if(modeOld != Modbus_Analysis_ModeTemp)
					{
						write_dgus_vp((u32)(0x201b),(u8*)&Modbus_Analysis_ModeTemp,1);
						Defrosting = Modbus_Analysis_ModeTemp ? TRUE : FALSE;
						Ready_To_Save();
					}

				}

				Modbus_Read_User = FALSE;
				Modbus_Read_Check = TRUE;
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

					}
				}

				Modbus_PackCheckStatusBlock(Uart5_Rx, CHECK_PARAMETER_NUM, IsCheckParameterTemperature, 0);

				Modbus_Analysis_SetTemp = GetModbusReg(Uart5_Rx, 218) / 10;
				Modbus_WriteFloatTempPair(0x4800, 0x4810, (float)Modbus_Analysis_SetTemp);

				Modbus_Analysis_SetTemp = GetModbusReg(Uart5_Rx, 220) / 10;
				Modbus_WriteFloatTempPair(0x4802, 0x4812, (float)Modbus_Analysis_SetTemp);

				Modbus_Analysis_SetTemp = GetModbusReg(Uart5_Rx, 221) / 10;
				Modbus_WriteFloatTempPair(0x4804, 0x4814, (float)Modbus_Analysis_SetTemp);

				Modbus_Analysis_mode = GetModbusReg(Uart5_Rx, 214);
				write_dgus_vp((u32)(0x1004),(u8*)&Modbus_Analysis_mode,1);

				Modbus_Analysis_mode = GetModbusReg(Uart5_Rx, 221)/10;
				write_dgus_vp((u32)(0x4016),(u8*)&Modbus_Analysis_mode,1);

				Modbus_Read_Check = FALSE;
				Modbus_Read_Check_2 = TRUE;
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
					write_dgus_vp((u32)(0x4607),(u8*)&Error_Temp,1);
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
								write_dgus_vp((u32)(0x4607),(u8*)&Error_Temp,1);
							}
						}
					}
				}	

				Modbus_PackCheckStatusBlock(Uart5_Rx, CHECK_PARAMETER_NUM_2, IsCheckParameterTemperature_2, CHECK_PARAMETER_NUM);

				u8tos16.temp[0] = Uart5_Rx[9];
				u8tos16.temp[1] = Uart5_Rx[10];
				ftemp = (float)u8tos16.all/10;
				write_dgus_vp((u32)(0x1006),(u8*)&ftemp,2);
				ftemp = TempUnitTrans((unsigned short)ftemp,'F');
				write_dgus_vp((u32)(0x1008),(u8*)&ftemp,2);

				u8tos16.temp[0] = Uart5_Rx[21];
				u8tos16.temp[1] = Uart5_Rx[22];
				ftemp = (float)u8tos16.all/10;
				Modbus_WriteFloatTempPair(0x4806, 0x4816, (float)(u8tos16.all / 10));


				Modbus_Read_Check_2 = FALSE;
				Modbus_Read_Check_3 = TRUE;
			break;
			case 9:
			{
				u16 temp_c;
				u16 freq_raw, time_raw, t3_raw;

				u8tos16.temp[0] = Uart5_Rx[3];
				u8tos16.temp[1] = Uart5_Rx[4];
				freq_raw = u8tos16.all;
				Modbus_Analysis_mode = freq_raw;
				write_dgus_vp((u32)(0x1010),(u8*)&Modbus_Analysis_mode,2);
				write_dgus_vp((u32)(0x3660),(u8*)&Modbus_Analysis_mode,1);

				u8tos16.temp[0] = Uart5_Rx[5];
				u8tos16.temp[1] = Uart5_Rx[6];
				time_raw = u8tos16.all;
				Modbus_Analysis_mode = time_raw;
				write_dgus_vp((u32)(0x1012),(u8*)&Modbus_Analysis_mode,2);
				write_dgus_vp((u32)(0x3670),(u8*)&Modbus_Analysis_mode,1);

				u8tos16.temp[0] = Uart5_Rx[7];
				u8tos16.temp[1] = Uart5_Rx[8];
				t3_raw = u8tos16.all;
				temp_c = (u16)(t3_raw / 10);
				ftemp = (float)temp_c;
				write_dgus_vp((u32)(0x1014),(u8*)&ftemp,2);
				ftemp = (float)TempUnitTrans((signed short)temp_c, 'F');
				write_dgus_vp((u32)(0x101a),(u8*)&ftemp,2);
				write_dgus_vp((u32)(0x3680),(u8*)&temp_c,1);
				s_check_status[70] = Modbus_PackCheckStatusByte(freq_raw, 0);
				s_check_status[71] = Modbus_PackCheckStatusByte(time_raw, 0);
				s_check_status[72] = Modbus_PackCheckStatusByte(t3_raw, 1);
			}
			Modbus_Read_Check_4 = FALSE;
			Indoor_Unit_Index = 0;
			Modbus_Write_6 = TRUE;
			upload_request_report();
			upload_check_status();
			break;
			case 8:
			{
				u16 temp_c;
				u16 temp_f;

				// Read 1000 series (Operation Info) for current unit
				u8tos16.temp[0] = Uart5_Rx[7*2+3]; // T1 Temp
				u8tos16.temp[1] = Uart5_Rx[7*2+4];
				Modbus_Analysis_mode = u8tos16.all;
				write_dgus_vp((u32)(0x1100 + (u16)Indoor_Unit_Index * 0x20),(u8*)&Modbus_Analysis_mode,2);
				temp_c = (u16)(Modbus_Analysis_mode / 10);
				write_dgus_vp((u32)(0x1030 + (u16)Indoor_Unit_Index * 2),(u8*)&temp_c,1);
				temp_f = (u16)TempUnitTrans((signed short)temp_c, 'F');
				write_dgus_vp((u32)(0x1040 + (u16)Indoor_Unit_Index * 2),(u8*)&temp_f,1);

				u8tos16.temp[0] = Uart5_Rx[8*2+3]; // T2 Temp
				u8tos16.temp[1] = Uart5_Rx[8*2+4];
				Modbus_Analysis_mode = u8tos16.all;
				write_dgus_vp((u32)(0x1102 + (u16)Indoor_Unit_Index * 0x20),(u8*)&Modbus_Analysis_mode,2);

				if(Indoor_Unit_Index < 5)
				{
					Indoor_Unit_Index++;
					Modbus_Read_Check_3 = TRUE;
				}
				else
				{
					Indoor_Unit_Index = 0;
					Modbus_Read_Check_3 = FALSE;
					Modbus_Read_Check_5 = TRUE; // Go to Read 11000
				}
			}
		break;
		case 12:{
		// Read 11000 series (Control Info) for current unit
		// Parsing if needed...

			if(Indoor_Unit_Index < 5)
			{
				Indoor_Unit_Index++;
				Modbus_Read_Check_5 = TRUE;
			}
			else
			{
				Indoor_Unit_Index = 0;
				Modbus_Read_Check_5 = FALSE;
				Modbus_Read_Check_4 = TRUE; // Go to Read 30366
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
				Modbus_Write_Defrost = FALSE;
			break;

			case	15000:
				Modbus_Write_5 = FALSE;
				Modbus_Write_5_Pending = 0;
				Modbus_SaveGroupSentRegs(s_group_regs);
			break;

			case	11000:
			case    11030:
			case    11060:
			case    11090:
			case    11120:
			case    11150:
			Modbus_SaveIndoorSentRegs(Indoor_Unit_Index, s_indoor_regs);
			if(Modbus_Write_6_All && Indoor_Unit_Index < 5)
			{
				Indoor_Unit_Index++;
				Modbus_Write_6 = TRUE;
			}
			else
			{
				Indoor_Unit_Index = 0;
				Modbus_Write_6 = FALSE;
				Modbus_Write_6_All = 0;
				Modbus_Read_User = TRUE;
			}
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

		case	611:
			Modbus_Write_ClearPower = FALSE;
		break;

		case	877:

			Modbus_Write_Set = FALSE;//通过修改参数手动除霜F
		break;

		case	30366:
			Modbus_Write_DefrostParam = FALSE;
		break;

		default:
			if(modbus_addr == (800+SetPara_Addr))
				Modbus_Write_Set = FALSE;
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
