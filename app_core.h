#ifndef APP_CORE_H
#define APP_CORE_H

#include "sys.h"
#include "sync_link.h"

/*
 * Code Banking 桥接层（COMMON）
 *   application 可直接调用各 bank
 *   各 bank 禁止互调，跨 bank 时只调用本文件 App_* / SyncLink_*
 */

/* WiFi 调度 */
void App_RunWifiService(void);
void App_WifiUartRxPullOnly(void);       /* 仅搬 UART2 字节，可在 Modbus bank 内调用 */
void App_WifiUartServiceYield(void);     /* rx_pull + 完整帧解析（COMMON 主循环调用） */
void App_WifiUartDeferService(void);     /* Modbus 阻塞期间置位，退出 handler 后解析 */
void App_WifiUartDrainDeferred(void);
void App_WifiUartYield(u8 process_download); /* upload 上报期间：可选 cmd6 下发解析 */

/* 屏端开关同步后上报 WiFi */
void App_SwitchUploadRequest(u8 val);
void App_SwitchUploadService(void);
void App_SwitchUploadCancel(void);
void App_OnWr100Done(u8 pwr, u8 src);
void App_SyncLinkApplySwitch(u8 pwr, u8 src);

/* Modbus */
void App_ModbusTriggerUserWrite(void);
void App_ModbusClearManualDefrost(void);
void App_ModbusTriggerIndoorUnitWrite(u8 index, u8 write_all);
void App_ModbusTriggerGroupControlWrite(void);
void App_ModbusTriggerGroupControlWriteOff(void);
void App_ModbusTriggerDefrostParamWrite(u16 vp);
void App_ModbusStartManualDefrost(void);
void App_ModbusApplyIndoorTimerVpToUnits(u8 update_on, u8 update_off);
void App_ModbusRefreshCheckParamDisplay(void);
u8 App_ModbusUserWriteBusy(void);
u8 App_ModbusUserHoldActive(void);
u8 App_ModbusIndoorWriteBusy(void);
void App_ModbusGetCheckStatus(u8 *buf, u8 len);
void App_ControlIndoorSwitchPollStep(void);
void App_ControlIndoorSwitchSyncFromVp(void);

/* Control / 主页 */
void App_HomePage(bit turn_page);
void App_HomePageIcon(void);
void App_HomePageSyncIndoorDisplay(void);
void App_UnitChangePro(void);
void App_SetTempIntWrite(u8 mode, u8 unit_f, u16 temp);
void App_SyncSetTempCachesFromC(u16 temp_c);
void App_ControlAckExtPowerVp(u16 pwr);
void App_ControlBlockExtInput(u8 ticks);
signed short App_TempUnitTrans(signed short temp, unsigned char type);

/* Upload */
void App_UploadSwitch(void);
void App_UploadSwitchReport(u8 val);
void App_UploadMode(void);
void App_UploadTempSet(void);
void App_UploadTempUnit(void);
void App_UploadTempCurrent(void);
void App_UploadWorkState(void);
void App_UploadChildLock(void);
void App_UploadDefrost(void);
void App_UploadDefrostFreq(void);
void App_UploadDefrostTime(void);
void App_UploadDefrostOutTemp(void);
void App_UploadRelayStatus(void);
void App_UploadFault(void);
void App_UploadIndoorUnit(void);
void App_UploadIndoorUnitRequest(void);
void App_UploadIndoorUnitService(void);
void App_UploadInsideTimeOpen(void);
void App_UploadInsideTimeClose(void);
void App_UploadExternalTimeOpen(void);
void App_UploadExternalTimeClose(void);
void App_UploadAroundTemp(void);
void App_UploadTempTop(void);
void App_UploadEffluentTemp(void);
void App_UploadEvaporatorTemp(void);
void App_UploadShadowBool(u8 dpid, u8 val);
void App_UploadShadowEnum(u8 dpid, u8 val);
void App_UploadShadowValue(u8 dpid, u32 val);
void App_UploadStateQueryReply(void);
void App_UploadPollStep(void);
void App_ProtocolSyncSwitchShadow(u8 val);

/* ErrorHistory */
void App_ErrorHistoryPageChange(u16 page);
void App_ErrorHistoryTryMigrateFlash(void);
void App_ErrorHistoryResetPage(void);
void App_ClearErrorHistory(void);
void App_ErrorHistoryDisplay(void);

/* WiFi DP 上报（upload/modbus 经 COMMON 调 mcu_api） */
unsigned char App_McuDpBoolUpdate(u8 dpid, u8 val);
unsigned char App_McuDpEnumUpdate(u8 dpid, u8 val);
unsigned char App_McuDpValueUpdate(u8 dpid, u32 val);
unsigned char App_McuDpStringUpdate(u8 dpid, const u8 value[], u16 len);
unsigned char App_McuDpRawUpdate(u8 dpid, const u8 value[], u16 len);
unsigned long App_MyStrlen(u8 *str);

void App_ApplyLanguageFromTouchVp(void);
void App_HandleSettingsBackOrWifiReset(void);

#endif
