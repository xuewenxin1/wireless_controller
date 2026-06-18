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
void upload_report_changed(void);
void upload_report_force_sync(void);
void upload_request_report(void);
void upload_report_poll(void);

#endif
