#ifndef __UPLOAD_H__
#define __UPLOAD_H__
#include "sys.h"
#include "t5los8051.h"
void upload_indoor_unit(void);
void upload_check_status(void);
void upload_history_faults(void);
void upload_all_timers(void);
void upload_inside_time_open(void);
void upload_inside_time_close(void);
void upload_external_time_open(void);
void upload_external_time_close(void);
void upload_electricity_statistics(void);
void upload_timestamp_from_rtc(void);
void upload_timestamp_boot_once(void);
void upload_modbus_poll_report(void);

void upload_set_boot_ready(u8 ready);
u8 upload_is_boot_ready(void);

/* 应答模块 cmd=8 状态查询（非全量轮询，仅模块请求时触发） */
void upload_state_query_reply(void);

void upload_dp_switch(void);
void upload_dp_mode(void);
void upload_dp_temp_set(void);
void upload_dp_temp_unit(void);
void upload_dp_work_state(void);
void upload_dp_child_lock(void);
void upload_dp_defrost(void);
void upload_dp_relay_status(void);
void upload_dp_fault(void);
void upload_dp_temp_current(void);
void upload_dp_temp_top(void);
void upload_dp_effluent_temp(void);
void upload_dp_around_temp(void);
void upload_dp_evaporator_temp(void);
void upload_dp_defrost_freq(void);
void upload_dp_defrost_out_temp(void);
void upload_dp_defrost_time(void);

void upload_shadow_bool(u8 dpid, u8 val);
void upload_shadow_enum(u8 dpid, u8 val);
void upload_shadow_value(u8 dpid, u32 val);

#endif
