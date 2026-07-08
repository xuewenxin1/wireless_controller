#include "app_share.h"

#define VP_FLOAT_C_BASE		0x4800U
#define VP_FLOAT_F_BASE		0x4810U
#define VP_INT_C_BASE		0x4808U
#define VP_INT_F_BASE		0x4818U

bit g_upload_status_boot_done = 0;
bit g_upload_fault_boot_done = 0;

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

u32 SetTempIntVpByMode(u8 mode, u8 unit_f)
{
	if(mode < 1 || mode > 3)
		return 0;
	if(unit_f)
		return VP_INT_F_BASE + (u32)(mode - 1) * 2;
	return VP_INT_C_BASE + (u32)(mode - 1) * 2;
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
