#ifndef APP_SHARE_H
#define APP_SHARE_H

#include "sys.h"

u16 GetPageID(void);
signed short TempUnitTrans(signed short temp, unsigned char type);

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

u8 DgusVpIsEnabled(u32 vp);
void DgusCopyTimerOffUiToProto(void);
void DgusCopyTimerOffProtoToUi(void);
u8 DgusReadTimerOffEnable(void);

#endif
