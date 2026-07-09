#include "app_core.h"
#include "wifi.h"
#include "upload.h"
#include "modbus.h"
#include "control.h"
#include "app_share.h"
#include "ErrorHistory.h"
#include "stdio.h"

extern u8 g_mb_switch_wr_src;

static u8 s_wifi_service_depth = 0;
static u8 s_wifi_svc_defer = 0;
static u8 s_sw_up_val = 0;
static bit s_sw_up_req = 0;
static u16 s_sw_up_expire = 0;
static bit s_indoor_up_req = 0;
static u16 s_indoor_up_expire = 0;

#define SWITCH_UPLOAD_DEFER_MS     300U
#define INDOOR_UPLOAD_DEFER_MS     300U
#define WIFI_SERVICE_MAX_DEPTH     3
#define WIFI_SERVICE_MAX_ROUNDS    2

void App_RunWifiService(void)
{
	u8 i;

	if(s_wifi_service_depth >= WIFI_SERVICE_MAX_DEPTH)
		return;
	for(i = 0; i < WIFI_SERVICE_MAX_ROUNDS; i++)
	{
		s_wifi_service_depth++;
		wifi_uart_service();
		s_wifi_service_depth--;
	}
}

void App_WifiUartRxPullOnly(void)
{
	wifi_uart_rx_pull();
}

void App_WifiUartServiceYield(void)
{
	WDT_RST();
	wifi_uart_rx_pull();
	wifi_uart_service();
}

void App_WifiUartDeferService(void)
{
	if(s_wifi_svc_defer < 4)
		s_wifi_svc_defer++;
}

void App_WifiUartDrainDeferred(void)
{
	u8 n;

	n = s_wifi_svc_defer;
	s_wifi_svc_defer = 0;
	while(n--)
		App_WifiUartServiceYield();
}

void App_WifiUartYield(u8 process_download)
{
	WDT_RST();
	wifi_uart_rx_pull();
	if(process_download)
		wifi_uart_process_download();
}

static void App_SwitchUploadFlushNow(void)
{
	s_sw_up_req = 0;
	upload_dp_switch_report_syn(s_sw_up_val);
}

void App_SwitchUploadRequest(u8 val)
{
	val = val ? 1 : 0;
	if(s_sw_up_req && s_sw_up_val != val)
		App_SwitchUploadFlushNow();
	s_sw_up_val = val;
	s_sw_up_req = 1;
	s_sw_up_expire = g_ms_tick + SWITCH_UPLOAD_DEFER_MS;
}

void App_SwitchUploadService(void)
{
	if(!s_sw_up_req)
		return;
	if((u16)(g_ms_tick - s_sw_up_expire) > 0x8000U)
		return;
	App_SwitchUploadFlushNow();
}

void App_SwitchUploadCancel(void)
{
	s_sw_up_req = 0;
}

void App_UploadIndoorUnitRequest(void)
{
	s_indoor_up_req = 1;
	s_indoor_up_expire = g_ms_tick + INDOOR_UPLOAD_DEFER_MS;
}

void App_UploadIndoorUnitService(void)
{
	if(!s_indoor_up_req)
		return;
	if((u16)(g_ms_tick - s_indoor_up_expire) > 0x8000U)
		return;
	s_indoor_up_req = 0;
	upload_indoor_unit();
}

void SyncLink_ApplySwitch(unsigned char pwr, unsigned char src)
{
	u16 mode;
	u16 old_vp;
	u16 new_vp;
	u16 pwr16;

	if(pwr > 1)
		pwr = 1;
	pwr16 = pwr;

	old_vp = Control_GetExtPowerLastCmd();
	if(old_vp == 0xFFFF)
	{
		read_dgus_vp((u32)0x4021, (u8 *)&old_vp, 1);
		if(old_vp > 1)
			old_vp = 1;
	}
	else if(old_vp > 1)
		old_vp = 1;

	if(old_vp == pwr16)
		return;

	read_dgus_vp((u32)0x2002, (u8 *)&mode, 1);
	if(pwr16 == 0)
		mode = 0;
	else if(mode == 0)
		mode = 1;
	write_dgus_vp((u32)0x2002, (u8 *)&mode, 1);
	if(src == SYNC_SRC_APP)
		write_dgus_vp((u32)0x4021, (u8 *)&pwr16, 1);

	read_dgus_vp((u32)0x4021, (u8 *)&new_vp, 1);
	if(new_vp > 1)
		new_vp = 1;

	printf("[SYNC] %c pwr=%u vp4021=%u mode2002=%u\r\n",
		(char)src, (u16)pwr16, (u16)new_vp, (u16)mode);

	Control_AckExtPowerVp(pwr16);
	App_ControlBlockExtInput(src == SYNC_SRC_SCR ? 15 : 8);

	g_mb_switch_wr_src = (u8)src;
	Modbus_TriggerUserWriteWith(pwr16, mode);
}

void App_OnWr100Done(u8 pwr, u8 src)
{
	(void)src;
	if(!upload_is_boot_ready())
		return;
	pwr = pwr ? 1 : 0;
	App_SwitchUploadCancel();
	App_UploadSwitchReport(pwr);
}

void App_SyncLinkApplySwitch(u8 pwr, u8 src) { SyncLink_ApplySwitch(pwr, src); }

void App_ModbusTriggerUserWrite(void) { Modbus_TriggerUserWrite(); }
void App_ModbusClearManualDefrost(void) { Modbus_ClearManualDefrost(); }
void App_ModbusTriggerIndoorUnitWrite(u8 index, u8 write_all) { Modbus_TriggerIndoorUnitWrite(index, write_all); }
void App_ModbusTriggerGroupControlWrite(void) { Modbus_TriggerGroupControlWrite(); }
void App_ModbusTriggerGroupControlWriteOff(void) { Modbus_TriggerGroupControlWriteOff(); }
void App_ModbusTriggerDefrostParamWrite(u16 vp) { Modbus_TriggerDefrostParamWrite(vp); }
void App_ModbusStartManualDefrost(void) { Modbus_StartManualDefrost(); }
void App_ModbusApplyIndoorTimerVpToUnits(u8 update_on, u8 update_off) { Modbus_ApplyIndoorTimerVpToUnits(update_on, update_off); }
void App_ModbusRefreshCheckParamDisplay(void) { Modbus_RefreshCheckParamDisplay(); }
u8 App_ModbusUserWriteBusy(void) { return Modbus_UserWriteBusy(); }
u8 App_ModbusUserHoldActive(void) { return Modbus_UserHoldActive(); }
u8 App_ModbusIndoorWriteBusy(void) { return Modbus_IndoorWriteBusy(); }
void App_ModbusGetCheckStatus(u8 *buf, u8 len) { Modbus_GetCheckStatus(buf, len); }
void App_ControlIndoorSwitchPollStep(void) { Control_IndoorSwitchPollStep(); }
void App_ControlIndoorSwitchSyncFromVp(void) { Control_IndoorSwitchSyncFromVp(); }

void App_HomePage(bit turn_page) { HomePage(turn_page); }
void App_HomePageIcon(void) { HomePage_Icon(); }
void App_HomePageSyncIndoorDisplay(void) { HomePage_SyncIndoorDisplay(); }
void App_UnitChangePro(void) { UnitChangePro(); }
void App_SetTempIntWrite(u8 mode, u8 unit_f, u16 temp) { SetTempIntWrite(mode, unit_f, temp); }
void App_SyncSetTempCachesFromC(u16 temp_c) { SyncSetTempCachesFromC(temp_c); }
void App_ControlAckExtPowerVp(u16 pwr) { Control_AckExtPowerVp(pwr); }
void App_ControlBlockExtInput(u8 ticks) { Control_BlockExtInput(ticks); }
signed short App_TempUnitTrans(signed short temp, unsigned char type) { return TempUnitTrans(temp, type); }

void App_UploadSwitch(void) { upload_dp_switch(); }
void App_UploadSwitchReport(u8 val) { upload_dp_switch_report_syn(val); }
void App_UploadMode(void) { upload_dp_mode(); }
void App_UploadTempSet(void) { upload_dp_temp_set(); }
void App_UploadTempUnit(void) { upload_dp_temp_unit(); }
void App_UploadTempCurrent(void) { upload_dp_temp_current(); }
void App_UploadWorkState(void) { upload_dp_work_state(); }
void App_UploadChildLock(void) { upload_dp_child_lock(); }
void App_UploadDefrost(void) { upload_dp_defrost(); }
void App_UploadDefrostFreq(void) { upload_dp_defrost_freq(); }
void App_UploadDefrostTime(void) { upload_dp_defrost_time(); }
void App_UploadDefrostOutTemp(void) { upload_dp_defrost_out_temp(); }
void App_UploadRelayStatus(void) { upload_dp_relay_status(); }
void App_UploadFault(void) { upload_dp_fault(); }
void App_UploadIndoorUnit(void) { upload_indoor_unit(); }
void App_UploadInsideTimeOpen(void) { upload_inside_time_open(); }
void App_UploadInsideTimeClose(void) { upload_inside_time_close(); }
void App_UploadExternalTimeOpen(void) { upload_external_time_open(); }
void App_UploadExternalTimeClose(void) { upload_external_time_close(); }
void App_UploadAroundTemp(void) { upload_dp_around_temp(); }
void App_UploadTempTop(void) { upload_dp_temp_top(); }
void App_UploadEffluentTemp(void) { upload_dp_effluent_temp(); }
void App_UploadEvaporatorTemp(void) { upload_dp_evaporator_temp(); }
void App_UploadShadowBool(u8 dpid, u8 val) { upload_shadow_bool(dpid, val); }
void App_UploadShadowEnum(u8 dpid, u8 val) { upload_shadow_enum(dpid, val); }
void App_UploadShadowValue(u8 dpid, u32 val) { upload_shadow_value(dpid, val); }
void App_UploadStateQueryReply(void) { upload_state_query_reply(); }
void App_UploadPollStep(void) { upload_poll_step(); }

extern void protocol_sync_switch_shadow(unsigned char val);
void App_ProtocolSyncSwitchShadow(u8 val) { protocol_sync_switch_shadow(val); }

void App_ErrorHistoryPageChange(u16 page) { ErrorHistory_PageChange(page); }
void App_ErrorHistoryTryMigrateFlash(void) { ErrorHistory_TryMigrateFlash(); }
void App_ErrorHistoryResetPage(void) { ErrorHistory_ResetPage(); }
void App_ClearErrorHistory(void) { ClearErrorHistory(); }
void App_ErrorHistoryDisplay(void) { ErrorHistoryDisplay(); }

unsigned char App_McuDpRawUpdate(u8 dpid, const u8 value[], u16 len) { return mcu_dp_raw_update(dpid, value, len); }
unsigned char App_McuDpBoolUpdate(u8 dpid, u8 val) { return mcu_dp_bool_update(dpid, val); }
unsigned char App_McuDpEnumUpdate(u8 dpid, u8 val) { return mcu_dp_enum_update(dpid, val); }
unsigned char App_McuDpValueUpdate(u8 dpid, u32 val) { return mcu_dp_value_update(dpid, val); }
unsigned char App_McuDpStringUpdate(u8 dpid, const u8 value[], u16 len) { return mcu_dp_string_update(dpid, value, len); }
unsigned long App_MyStrlen(u8 *str) { return my_strlen(str); }
