#ifndef __UART_H__
#define __UART_H__

#include "sys.h"
#include <stdio.h>

#define WIFI_BAUDRATE_9600			688			/* UART2: 1024-FOSC/(64*9600) */
#define UART45_BAUD_DIV_H			0x0A		/* UART4/UART5: FCLK/(8*9600)=2688 */
#define UART45_BAUD_DIV_L			0x80
#define UART2_MAX_LEN  128
#define UART5_MAX_LEN 256

sbit RS485_TX_EN = P0^1;
sbit RS485_UART4_EN = P0^0;

#define UART2_MAX_LEN  128
extern u16 uart2_rx_count;
extern u8 xdata  Uart2_Rx[UART2_MAX_LEN];

#define UART5_MAX_LEN 256
extern u16 uart5_rx_count;
extern u8 xdata Uart5_Rx[UART5_MAX_LEN]; 

//#define	UART5BUAD2400

extern u8 modbus_res_finish;
extern u8 modbus_send_finish;
extern u16 modbus_error;

extern u16 modbus_rx_status;
extern u8 wifi_send_en;

void UART2_Init(void);
void UART2_RxEnable(void);
void Uart2SendData(u8 byte);
extern unsigned char nUart0Sending;
void UART5_Init(void);
void UART5_Sendbyte(u8 dat);
void UART5_SendStr(u8 *pstr,u16 strlen);
extern unsigned char nUart5Sending;
void UART4_Init(void);

// BOOT_DBG_ON=1: 启动诊断日志（低频，勿在主循环每圈打印）
#define BOOT_DBG_ON 0
// BOOT_SKIP_TEMP=1: 跳过 TempUnit_RefreshAll，用于判断 home 后是否死在该函数
#define BOOT_SKIP_TEMP 1
#if BOOT_DBG_ON
    #define BOOT_DBG(x) printf x
#else
    #define BOOT_DBG(x)
#endif

// 调试：DEBUG_ON=1 时输出 WiFi 详细日志
#define DEBUG_ON 0
#if DEBUG_ON
    #define DEBUG_PRINT(x) printf x
    #define WIFI_DBG(x)   printf x
#else
    #define DEBUG_PRINT(x)
    #define WIFI_DBG(x)
#endif
// MODBUS_DBG_ON=1: Modbus 通讯调试（每次读写打印地址+典型数据）
#define MODBUS_DBG_ON 0
#if MODBUS_DBG_ON
    #define MODBUS_DBG(x) printf x
#else
    #define MODBUS_DBG(x)
#endif

#endif
