#include "app_share.h"
#include "control.h"
#include "sys.h"

bit g_upload_status_boot_done = 0;
bit g_upload_fault_boot_done = 0;

static u8 s_ext_input_block = 0;
static u16 s_4021_last_cmd = 0xFFFF;

u16 GetPageID(void)
{
	u16 usPID;

	read_dgus_vp(PIC_NOW, (u8 *)&usPID, 1);
	return usPID;
}

signed short TempUnitTrans(signed short temp, unsigned char type)
{
	signed short Cache = 0;

	if(type == 'C')
	{
		if(temp < -22)
			temp = -22;
		else if(temp > 266)
			temp = 266;
		Cache = (temp - 32) * 10 * 5 / 9;
	}
	else if(type == 'F')
	{
		if(temp < -30)
			temp = -30;
		else if(temp > 130)
			temp = 130;
		Cache = temp * 10 * 9 / 5 + 320;
	}

	Cache = (Cache > 0) ? ((Cache + 5) / 10) : ((Cache - 5) / 10);
	return Cache;
}

void Control_BlockExtInput(u8 ticks)
{
	if(ticks > s_ext_input_block)
		s_ext_input_block = ticks;
}

bit Control_IsExtInputBlocked(void)
{
	return s_ext_input_block > 0;
}

void Control_TickExtInputBlock(void)
{
	if(s_ext_input_block > 0)
		s_ext_input_block--;
}

u16 Control_GetExtPowerLastCmd(void)
{
	return s_4021_last_cmd;
}

void Control_AckExtPowerVp(u16 pwr)
{
	s_4021_last_cmd = pwr ? 1 : 0;
}

void Control_SyncExtPowerVp(u16 pwr)
{
	write_dgus_vp((u32)0x4021, (u8 *)&pwr, 1);
}

void Control_ApplyExtPowerVp(u16 pwr)
{
	write_dgus_vp((u32)0x4021, (u8 *)&pwr, 1);
}

u8 DgusVpIsEnabled(u32 vp)
{
	u16 w = 0;

	read_dgus_vp(vp, (u8 *)&w, 1);
	return w ? 1 : 0;
}

void DgusCopyTimerOffUiToProto(void)
{
	u16 w = 0;

	read_dgus_vp((u32)VP_TIMER_IN_OFF_EN_UI, (u8 *)&w, 1);
	write_dgus_vp((u32)VP_TIMER_IN_OFF_EN, (u8 *)&w, 1);
}

void DgusCopyTimerOffProtoToUi(void)
{
	u16 w = 0;

	read_dgus_vp((u32)VP_TIMER_IN_OFF_EN, (u8 *)&w, 1);
	write_dgus_vp((u32)VP_TIMER_IN_OFF_EN_UI, (u8 *)&w, 1);
}

u8 DgusReadTimerOffEnable(void)
{
	if(DgusVpIsEnabled(VP_TIMER_IN_OFF_EN_UI))
		return 1;
	return DgusVpIsEnabled(VP_TIMER_IN_OFF_EN);
}
