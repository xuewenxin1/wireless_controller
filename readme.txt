
??MCU_SDK??????????????????????????????????MCU???????????????????????????????MCU????

MCU_SDK??????????????
https://docs.tuya.com/zh/iot/device-development/access-mode-mcu/wifi-general-solution/software-reference-wifi/overview-of-migrating-tuyas-mcu-sdk?id=K9hhi0xr5vll9

================================================================================
Code Banking 布局（2026-07 稳定方案）
================================================================================

Bank 分配
  COMMON   : main, uart, sys, rtc, Wifipro, app_core, modbus  （与 origin/main 一致）
  Bank2    : control
  Bank5    : protocol + mcu_api + system + OTA
  Bank6    : upload + ErrorHistory

规则
  1. modbus 必须在 COMMON，才能像 origin/main 一样在 Modbus_YieldWiFi 内直接调 wifi_uart_service()
  2. 不要把 modbus 拆到 Bank1 再用 DgusWorkerStep 分步写屏 —— 会导致 3 秒进主页后栈破坏/死机
  3. 各 bank 禁止互调，跨 bank 走 app_core 的 App_* / SyncLink_*

WiFi 桥接（app_core）
  App_WifiUartYield(1)        upload(Bank6) 上报期间：rx_pull + cmd6 下发
  App_WifiUartServiceYield()  可选，COMMON 内其它模块使用

已验证问题根因
  - modbus 在 Bank1 + DgusWorkerStep + 嵌套 WiFi/DGUS = 3 秒后 wr100 乱值/死机
  - 乱地址 59392、pwr=110 mode=80 均为内存/栈破坏，不是真实 Modbus 数据

串口波特率：全部 9600（sys.c INIT_CPU 与 uart.c 一致）

Fix 2026-07-07: screen->App ext switch + indoor unit 6
  1. Touch 4021 -> App_UploadSwitchReport (force upload)
  2. Modbus wr100 OK -> App_OnWr100Done -> App_UploadSwitchReport
  3. Poll 11000 off-by-one fixed: unit6 (11150/4015) was skipped

VP: 4021=ext cmd, 4010-4015=indoor1-6 switch, 4016=indoor on count

Modbus 内机写入策略（2026-07-07）
  - 单台开关/参数：写 11000 系列（11000 + 台号×30）
  - 屏按钮 0x4006 全开：只写 15000 群控（不走 11000）
  - 屏/App 4010~4015 全为 0：15000 群控关机
  - App dpid=104 一次关 >=2 台：15000 群控关机
  - App dpid=104 只关 1 台：11000 单台写

主页右上角小图标 VP（0=隐藏 1=显示，屏工程图标可见性需绑这些 VP）
  - 0x3043 WiFi：连上路由器后为 1
  - 0x2004 定时：4821/4820/4825/4827 任一定时启用为 1
  - 0x3041 除霜：除霜运行中为 1
