/******************************************************************************

                  版权所有 (C), 2019, 北京迪文科技有限公司

 ******************************************************************************
  文 件 名   : uart.c
  版 本 号   : V2.0
  作    者   : chengjing
  生成日期   : 2019年4月1日
  功能描述   : 串口函数
  修改历史   :
  1.日    期   : 
    作    者   : 
    修改内容   : 
******************************************************************************/

#include "uart.h"
#include "modbus.h"
#include "wifi.h"
#include "sys.h"
#include <stdio.h>

/********************************
*   串口接受缓存区
*  缓存区大小可以根据实际进行修改
*  接收到缓存数据请尽快处理，否者
*  下一条数据会往尾端增加
********************************/
u8 uart2_busy=0;
u16 uart2_rx_count=0;

u8 xdata Uart2_Rx[UART2_MAX_LEN];

u16 uart5_rx_count=0;
u8 xdata Uart5_Rx[UART5_MAX_LEN];

volatile unsigned char *pUart0SendAddr = NULL;		// uart0 transmit buffer address
volatile unsigned short nUart0SendIndex = 0;		// uart0 transmit data number
unsigned char nUart0Sending;

volatile unsigned char *pUart5SendAddr = NULL;
volatile unsigned short nUart5SendIndex = 0;
unsigned char nUart5Sending = 0;

u8 modbus_res_finish = 0;
u8 modbus_send_finish = 1;
u16 modbus_error = 0;


u8 wifi_send_en = 0;
u16 wifi_rx_drop_count = 0;
u32 wifi_rx_byte_total = 0;

u16 modbus_rx_status = 0;

typedef union{
  u16 all; 
	u8 temp[2];
	
}MOUBUS_DATA;


void UART2_Init(void)
{
	MUX_SEL |= 0x40;
	P0MDOUT &= 0XCF;
	P0MDOUT |= 0X10;
	SCON0   = 0x50;
	ADCON   = 0x80;
	PCON   &= 0x7F;
	
	SREL0H  = WIFI_BAUDRATE_9600 >> 8;
	SREL0L  = WIFI_BAUDRATE_9600 & 0xFF;
	
	ES0     = 1;
    EA      = 1;
}

//发送一个字节（保持 ES0 开启，避免上报应答期间丢失 WiFi 下发帧）
void Uart2SendData(u8 byte)
{
	uart2_busy = 1;
	TI0 = 0;
	SBUF0 = byte;
	while(!TI0) {
		WDT_RST();
		wifi_uart_rx_pull();
	}
	TI0 = 0;
	uart2_busy = 0;
}


void UART2_ISR_PC(void)    interrupt 4
{
    u8 res;
	EA = 0;

    if(RI0)
    {
		RI0=0;
		res = SBUF0;
		wifi_rx_byte_total++;
		uart_receive_input(res);
    }
    
	if(TI0==1)
    {
		if(!uart2_busy)
		{
			TI0=0;
			if (nUart0SendIndex > 0)
			{
				SBUF0 = *pUart0SendAddr;
				pUart0SendAddr++;
				nUart0SendIndex--;
				nUart0Sending = 1;
			}
			else
			{
				nUart0Sending	= 0;
			}
		}
    }
    EA = 1;
}

/*****************************************************************************
 函 数 名  : void UART5_Init(void)
 功能描述  : 串口5初始化（RS485）
 修改历史  :
  1.日    期   : 2019年4月30日
    作    者   : chengjing
    修改内容   : 创建
*****************************************************************************/
void UART5_Init(void)
{
    SCON3T = 0x80;
    SCON3R = 0x80;

#ifdef UART5BUAD2400
    BODE3_DIV_H = 0x2A;
    BODE3_DIV_L = 0x00;
#else
    BODE3_DIV_H = 0x0A;   // 9600bps
    BODE3_DIV_L = 0x80;
#endif

    ES3T = 1;     // 发送中断
    ES3R = 1;     // 接收中断

    RS485_TX_EN = 0;   // 默认接收
    EA = 1;
}

/*****************************************************************************
 函 数 名  : UART5_SendStr
 功能描述  : 串口5发送字符串（RS485）
 输入参数  : pstr   - 数据首地址
             strlen - 数据长度
 修改历史  :
  1.日    期   : 2019年4月30日
    作    者   : chengjing
    修改内容   : 创建
*****************************************************************************/
void UART5_SendStr(u8 *pstr, u16 strlen)
{
    if ((pstr == NULL) || (strlen < 8) )//|| (strlen > UART5_MAX_LEN)
        return;

    uart5_rx_count = 0;
    pUart5SendAddr  = pstr;
    nUart5SendIndex = strlen;

    RS485_TX_EN = 1;          // 切到发送

    SBUF3_TX = *pUart5SendAddr;
	pUart5SendAddr++;
    nUart5SendIndex--;
    nUart5Sending = 1;
	ES3R = 0;
}

/*****************************************************************************
 函 数 名  : UART5_TX_ISR_PC
 功能描述  : 串口5发送中断
*****************************************************************************/
void UART5_TX_ISR_PC(void) interrupt 12
{
    EA = 0;
    SCON3T &= 0xFE;   // 清发送中断标志

    if (nUart5SendIndex > 0)
    {
		SBUF3_TX = *pUart5SendAddr;
		pUart5SendAddr++;
        nUart5SendIndex--;
    }
    else
    {
        u16 i;

        nUart5Sending = 0;
        RS485_TX_EN = 0;      // 切回接收
        for(i = 0; i < 120; i++);  // ~1 字符时间 @9600，避免过早开 RX 丢首字节
        ES3R = 1;
    }
    EA = 1;
}

/*****************************************************************************
 函 数 名  : UART5_RX_ISR_PC
 功能描述  : 串口5接收中断（Modbus）
*****************************************************************************/
void UART5_RX_ISR_PC(void) interrupt 13
{
    u8 res;
    u16 modbus_len = 0;

    EA = 0;

    if ((SCON3R & 0x01) == 0x01)
    {
        res = SBUF3_RX;
        SCON3R &= 0xFE;

        if (uart5_rx_count >= UART5_MAX_LEN)
            uart5_rx_count = 0;

        if (uart5_rx_count == 0 && res != 0x01)
            ;
        else
        {
            Uart5_Rx[uart5_rx_count++] = res;

            if (uart5_rx_count >= 3)
            {
                if (Uart5_Rx[0] != 0x01)
                    uart5_rx_count = 0;
                else if (Uart5_Rx[1] == 0x03)
                    modbus_len = (u16)Uart5_Rx[2] + 5;
                else if (Uart5_Rx[1] == 0x06 || Uart5_Rx[1] == 0x10)
                    modbus_len = 8;
                else
                    modbus_len = 8;

                if (modbus_len < 8)
                    modbus_len = 8;
                if (modbus_len > UART5_MAX_LEN)
                    uart5_rx_count = 0;
                else if (uart5_rx_count >= modbus_len)
                {
                    modbus_res_finish = 1;
                    modbus_error = 0;
                    uart5_rx_count = 0;
                    Send_Count = 0;
                }
            }
        }
    }
    EA = 1;
}
/*****************************************************************************
 函 数 名  : void UART4_Init(void)
 功能描述  : 串口4初始化 (用于调试 printf)
 波 特 率  : 9600 @ 206.4384MHz
*****************************************************************************/
void UART4_Init(void)
{
    SCON2T = 0x80;
    SCON2R = 0x80;
    
    // 设置波特率为 9600
    // DIV = 206438400 / (8 * 9600) = 2688 (0x0A80)
    BODE2_DIV_H = 0x0A;     
    BODE2_DIV_L = 0x80;
    
    // 初始化 P0.0 为推挽输出 (TR4 方向控制)
    P0MDOUT |= 0x01;        
    RS485_UART4_EN = 0;
}

/*****************************************************************************
 函 数 名  : char putchar(char c)
 功能描述  : 重定向 putchar 到 UART4，支持 printf 输出
 注意事项  : RS485 半双工模式，发送前后需切换方向引脚
*****************************************************************************/
char putchar(char c)
{
    u16 i;
    RS485_UART4_EN = 1;      // 切换到发送模式
    
    SBUF2_TX = c;
    while(!(SCON2T & 0x01)); // 等待发送完成标志 (TI2)
    SCON2T &= 0xFE;          // 清除 TI2 标志
    
    // 9600波特率下，TI置位可能略早于停止位发送完毕
    // 增加约 100us 延时（对应 1 个位的时间）以确保 485 切换不会截断停止位
    for(i=0; i<100; i++);
    
    RS485_UART4_EN = 0;      // 切换回接收模式
    
    WDT_RST();               // 每次打印字符后喂狗，防止长字符串导致复位
    return c;
}
