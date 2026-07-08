#ifndef APP_SHARE_H
#define APP_SHARE_H

#include "sys.h"

u16 GetPageID(void);
signed short TempUnitTrans(signed short temp, unsigned char type);
u32 SetTempIntVpByMode(u8 mode, u8 unit_f);
u16 SetTempIntRead(u8 mode, u8 unit_f);
void SetTempIntWrite(u8 mode, u8 unit_f, u16 temp);
void SetTempFloatWriteC(u8 mode, float temp_c);

extern bit g_upload_status_boot_done;
extern bit g_upload_fault_boot_done;

void Control_SyncExtPowerVp(u16 pwr);
void Control_ApplyExtPowerVp(u16 pwr);
void Control_AckExtPowerVp(u16 pwr);
void Control_BlockExtInput(u8 ticks);
void Control_HoldExtPowerPoll(u8 ticks);
bit Control_IsExtInputBlocked(void);
void Control_TickExtInputBlock(void);
void Control_NotifyExtModeVp(u16 mode);
void Control_HandleExtPowerChange(u16 pwr);
void Control_HandleExtModeChange(u16 mode);
void Control_PollExtPowerVp(void);
u16 Control_GetExtPowerLastCmd(void);
void Control_RevertExtPowerVp(void);

#endif
