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
u16 modbus_error = 1;


u8 wifi_send_en = 0;

u16 modbus_rx_status = 0;

typedef union{
  u16 all; 
	u8 temp[2];
	
}MOUBUS_DATA;


void UART2_Init(void)
{
//	MUX_SEL |= 0x40;
//	P0MDOUT &= 0XCF;
//	P0MDOUT |= 0X10;
//	ADCON   = 0x80;
//	SCON0   = 0x50;
//	PCON   &= 0x7F;
//	SREL0H  = WIFI_BAUDRATE_9600 >> 8;
//	SREL0L  = WIFI_BAUDRATE_9600 & 0xFF;
//	REN0    = 0;
//	ES0     = 0;
	ADCON   = 0x80;					/*0x80=??SREL0H:L*/
    SCON0   = 0x50;					/*??1:10?UART*/
		PCON   &= 0x7F;        /*.7=SMOD,???????,0=???*/
    SREL0H  = WIFI_BAUDRATE_9600 >> 8;			/*1024-FOSC/(64*???)*/
		SREL0L  = WIFI_BAUDRATE_9600 & 0xFF;			/*1024-206438400/(64*9600)*/
		REN0    = 1;
    ES0     = 1;
    EA      = 1;
}

void UART2_RxEnable(void)
{
	REN0    = 1;
	ES0     = 1;
	EA      = 1;
}

static u8 s_uart2_tx_byte;

//void Uart2SendData(u8 byte)
//{
//	s_uart2_tx_byte = byte;
//	while(nUart0Sending)
//		WDT_RST();
//	pUart0SendAddr = &s_uart2_tx_byte;
//	nUart0SendIndex = 1;
//	SBUF0 = s_uart2_tx_byte;
//	pUart0SendAddr++;
//	nUart0SendIndex--;
//	nUart0Sending = 1;
//	while(nUart0Sending)
//		WDT_RST();
//}

s8 Uart2SendData(u8 *pBuf, u8 nLen)
{
	if (pBuf==NULL || nLen<1)
	{
		return -1;
	}

	pUart0SendAddr = pBuf;
	nUart0SendIndex = nLen;
	
//	IO_UART0_DE = 1;
	SBUF0 = *pUart0SendAddr;
	
	pUart0SendAddr++;
	nUart0SendIndex--;

	nUart0Sending = 1;
}

void UART2_ISR_PC(void)    interrupt 4
{
    u8 res=0;
    EA=0;
    if(RI0==1)
    {
		res = SBUF0;
		uart_receive_input(res);
		RI0=0;
    }
    
	if(TI0==1)
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
		
		uart2_busy=0;
    }
    EA=1;
}



///*****************************************************************************
// 函 数 名  : void UART5_Init(void)
// 功能描述  : 串口5初始化
// 输入参数  :	
// 输出参数  : 
// 修改历史  :
//  1.日    期   : 2019年4月30日
//    作    者   : chengjing
//    修改内容   : 创建
//*****************************************************************************/
//void UART5_Init(void)
//{
//    SCON3T=0x80;
//    SCON3R=0x80;
//#ifdef	UART5BUAD2400
//		BODE3_DIV_H=0x2A;
//    BODE3_DIV_L=0x00;
//#else
//    BODE3_DIV_H=0x0A;     //FCLK/(8*DIV）	//9600
//    BODE3_DIV_L=0x80;
//#endif
//    ES3T=1;    //UART5 发送中断使能
//    ES3R=1;
//	RS485_TX_EN=0;
//    EA=1;
//}





///*****************************************************************************
// 函 数 名  : void UART5_SendStr(u8 *pstr,u8 strlen)
// 功能描述  : 串口发送一个字节
// 输入参数  :	pstr:发送字符串首地址
//			strlen：发送字符串长度
// 输出参数  : 
// 修改历史  :
//  1.日    期   : 2019年4月30日
//    作    者   : chengjing
//    修改内容   : 创建
//*****************************************************************************/
//void UART5_SendStr(u8 *pstr,u16 strlen)
//{
//	if((NULL == pstr)||(strlen < 8))
//	{
//		return;
//	}
//	pUart5SendAddr = pstr;
//	nUart5SendIndex = strlen;
//	RS485_TX_EN = 1;
//	SBUF3_TX = *pUart5SendAddr;
//	pUart5SendAddr++;
//	nUart5SendIndex--;
//	nUart5Sending = 1;
//	ES3R = 0;
//}

//void UART5_TX_ISR_PC(void)    interrupt 12
//{
//	EA = 0;
//	SCON3T &= 0xFE;
//	if(nUart5SendIndex)
//	{
//		SBUF3_TX = *pUart5SendAddr;
//		pUart5SendAddr++;
//		nUart5SendIndex--;
//	}
//	else
//	{
//		ES3R = 1;
//		nUart5Sending = 0;
//		RS485_TX_EN = 0;
//	}
//	EA = 1;
//}

///*****************************************************************************
// 函 数 名  : void UART5_RX_ISR_PC(void)    interrupt 13
// 功能描述  : 串口中断接收函数
// 输入参数  :	
// 输出参数  : 
// 修改历史  :
//  1.日    期   : 2019年4月30日
//    作    者   : chengjing
//    修改内容   : 创建
//*****************************************************************************/


//void UART5_RX_ISR_PC(void)    interrupt 13
//{
//    u8 res = 0;
//		u16 modbus_len = 0;
//	
//    EA = 0;
//    if((SCON3R&0x01)==0x01)
//    {
//      res = SBUF3_RX;
//			
//			if(uart5_rx_count >= 6)
//			{
//				
//				Uart5_Rx[uart5_rx_count] = res;
//				uart5_rx_count++;
//				
//				/*Modi by xuan 20250728: 天氟地水的通讯为正规通讯 */
//				if(Uart5_Rx[1]==0x10)
//				{
//					modbus_len=8;
//				}
//				else
//				{
//					modbus_len = Uart5_Rx[2]+5;
//				}
//				
//				if(uart5_rx_count >= modbus_len)
//				{
//					modbus_res_finish = 1;		//接收完成标志
//					modbus_error = 0;					//接收成功，通讯故障清零F
////					Analysis_OK = 0;  				//解析完成标志清零
//					
//					uart5_rx_count = 0;
//				}
//				Send_Count = 0;						//与上次发送或接收间隔清零
//			}
//			else
//			{
//					Uart5_Rx[uart5_rx_count] = res;
//					uart5_rx_count++;
//			}

//			
//      SCON3R&=0xFE;       
//    }
//    EA = 1;
//}
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

    pUart5SendAddr  = pstr;
    nUart5SendIndex = strlen;

    RS485_TX_EN = 1;          // 切到发送
//    ES3R = 0;                 // 发送期间禁止接收中断

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
//        SBUF3_TX = *pUart5SendAddr++;
		SBUF3_TX = *pUart5SendAddr;
		pUart5SendAddr++;
        nUart5SendIndex--;
    }
    else
    {
		ES3R = 1;             // 允许接收中断
        nUart5Sending = 0;
        RS485_TX_EN = 0;      // 切回接收
        
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

//        /* 防止数组越界 */
//        if (uart5_rx_count >= UART5_MAX_LEN)
//        {
//            uart5_rx_count = 0;
//        }

//        Uart5_Rx[uart5_rx_count++] = res;

//        /* 至少收到从机地址 + 功能码 + 长度 */
//        if (uart5_rx_count >= 3)
//        {
//            if (Uart5_Rx[1] == 0x10)
//            {
//                modbus_len = 8;
//            }
//            else
//            {
//                modbus_len = Uart5_Rx[2] + 5;

//                /* 长度非法保护 */
//                if ((modbus_len < 8) || (modbus_len > UART5_MAX_LEN))
//                {
//                    uart5_rx_count = 0;
//                    EA = 1;
//                    return;
//                }
//            }

//            if (uart5_rx_count >= modbus_len)
//            {
//                modbus_res_finish = 1;
//                modbus_error = 0;
//                Send_Count = 0;
//                uart5_rx_count = 0;
//            }
//        }

//        SCON3R &= 0xFE;   // 清接收中断标志
//    }

//    EA = 1;
if(uart5_rx_count >= 6)
			{
				
				Uart5_Rx[uart5_rx_count] = res;
				uart5_rx_count++;
				
				/*Modi by xuan 20250728: 天氟地水的通讯为正规通讯 */
				if(Uart5_Rx[1]==0x10)
				{
					modbus_len=8;
				}
				else
				{
					modbus_len = Uart5_Rx[2]+5;
				}
				
				if(uart5_rx_count >= modbus_len)
				{
					modbus_res_finish = 1;		//接收完成标志
					modbus_error = 0;					//接收成功，通讯故障清零F
//					Analysis_OK = 0;  				//解析完成标志清零
					
					uart5_rx_count = 0;
				}
				Send_Count = 0;						//与上次发送或接收间隔清零
			}
			else
			{
					Uart5_Rx[uart5_rx_count] = res;
					uart5_rx_count++;
			}

			
      SCON3R&=0xFE;       
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
