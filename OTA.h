#ifndef OTA_H
#define OTA_H

/*
 * 主板 OTA 转发开关（编译期）
 * 0：不启用，涂鸦 OTA 不进入主板升级流程，行为与改 OTA 前一致
 * 1：启用，UPDATE_START 后置 TuyaOTAState，经 UART5 向主控转发固件
 */
#ifndef BOARD_OTA_ENABLE
#define BOARD_OTA_ENABLE  0
#endif

extern void OTAinit(void);
extern void OTA_ModbusPro(void);

#endif /* OTA_H */
