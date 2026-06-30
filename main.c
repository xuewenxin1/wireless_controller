/******************************************************************************

                  版权所有 (C), 2019, 北京迪文科技有限公司

 ******************************************************************************
  文 件 名   : main.c
  版 本 号   : V2.0
  作    者   : chengjing
  生成日期   : 2019年4月30日
  功能描述   : 主函数，外设和参数初始化，主循环中主要功能函数入口。
  修改历史   :
  1.日    期   : 
    作    者   : 
    修改内容   : 
******************************************************************************/

#include "t5los8051.h"
#include "sys.h"
#include "uart.h"
#include "control.h"
#include "modbus.h"
#include "stdio.h"
#include "rtc.h"
#include "wifi.h"			//涂鸦协议头文件F
#include "Wifipro.h"	//wifi控制头文件F
#include "upload.h"
#include "ErrorHistory.h"
#include "OTA.h"

extern u32 wifi_rx_byte_total;
extern u16 wifi_rx_drop_count;
extern unsigned char wifi_rx_cmd6_count;
extern unsigned char wifi_rx_cmd8_count;
extern unsigned char wifi_rx_last_cmd;
extern unsigned int wifi_rx_frame_ok_count;
extern unsigned char wifi_rx_ver_reject_count;
extern unsigned char wifi_rx_raw_c6_seen;
extern unsigned char wifi_rx_raw_c8_seen;
extern unsigned int wifi_tx_upload_count;
extern unsigned int wifi_rx_c6_hdr_ok_count;
extern unsigned int wifi_rx_c6_chk_fail_count;
extern unsigned int wifi_rx_c6_short_count;

bit OldTuyaOTAState = FALSE;

void	UI_OpenPage()
{
	static unsigned char	WaitingTime = 3;//上电后延迟进入主页F
	static unsigned char	AlreadyRunning = 0;
	
	if(!AlreadyRunning)//仅在上电后执行一次F
	{
		if(WaitingTime)
		{
			WaitingTime--;
			if(!WaitingTime)
			{
				HomePage(TRUE);
				Screensaver_Count = 0;
				AlreadyRunning = 1;
				upload_set_boot_ready(1);
			}
		}
	}
}
void	ModbusChanelControl(void)
{
	if(TuyaOTAState != OldTuyaOTAState)//OTA状态变化时进行相应初始化F
	{
		OldTuyaOTAState = TuyaOTAState;
		if(TuyaOTAState)
		{
			OTAinit();
			Page_Change_Handler(60);//跳转进OTA页面F
		}
		else
		{
			Modbus_Read_Initial = TRUE;//重新从主控初始化F
			Modbus_Write_Ueser = TRUE;//重写入一次OTA前的运行状态F
			OTAReloading = TRUE;//暂停向wifi模块上传版本信息F
			OTAReload_Delay = TRUE;//OTA结束后延迟返回主页F
		}
	}
	if(TuyaOTAState)
	{
		OTA_ModbusPro();
		TuyaOTADisConnectCnt = (TuyaOTADisConnectCnt<6000)?(TuyaOTADisConnectCnt+1):(6000);//通讯等待计数F
	}
	if(TuyaOTADisConnectCnt>=6000)
		TuyaOTAState = FALSE;
	//达成回复条件立刻回复模块F
	if(OTAStartReply)
	{
		OTAStartReply = FALSE;
		upgrade_package_choose(PACKAGE_SIZE);
	}
	if(OTAReply)
	{
		wifi_uart_write_frame(UPDATE_TRANS_CMD, MCU_TX_VER, 0);
		OTAReply = FALSE;
	}
}

int main(void)
{   
	INIT_CPU();
	PORT_Init();
	T0_Init();		
	T1_Init();	
	T2_Init();	
  
	Modbus_Init();	
	UART4_Init();
	Wifi_Init();			//串口2初始化，WIFI初始化
	
	init_rtc();
//	mcu_get_module_mac(); //查询MAC地址F
	
//	Basefunction_Init();
	System_Parm_Init();		//读flash F-
	WDT_ON();       //打开开门狗    喂狗在定时器T2中
	
	while(1)
	{  
		WDT_RST();
		wifi_uart_service();
		if(!TuyaOTAState)
			Modbus_Salve_Handler1();
		wifi_uart_service();
		
		if(task_10ms_count >= 10)//10ms
		{
			task_10ms_count = 0;
			Control_Function1();				//逻辑控制函数入口F
			Save_Data_Handler();
			ModbusChanelControl();//普通通讯与OTA通讯切换F
			RTCUpdate();//外部RTC时间更新		F
		}
	
		if(task_10000ms_count >= 10000)//10s
		{
			task_10000ms_count = 0;
			
			HomePage_Icon();//主页右上角小图标刷新F
			DecodeErrorHistory();//历史故障记录F
			MODBUS_DBG(("[MODBUS] err=%bu ecnt=%u st=%bu addr=%u u=%bu\r\n",
				(u8)modbus_error, (u16)Modbus_Error_Count,
				(u8)Modbus_Dbg_SendType, (u16)Modbus_Dbg_LastTxAddr,
				(u8)Indoor_Unit_Index));
			printf("[WIFI] rx=%lu ok=%u tx=%u c6=%bu c8=%bu r6=%bu c6h=%u c6f=%u c6s=%u vrj=%bu drop=%u\r\n",
				(unsigned long)wifi_rx_byte_total, (u16)wifi_rx_frame_ok_count,
				(u16)wifi_tx_upload_count,
				wifi_rx_cmd6_count, wifi_rx_cmd8_count,
				wifi_rx_raw_c6_seen, (u16)wifi_rx_c6_hdr_ok_count,
				(u16)wifi_rx_c6_chk_fail_count, (u16)wifi_rx_c6_short_count,
				wifi_rx_ver_reject_count, (u16)wifi_rx_drop_count);
		}
		
		if(APP_1000ms_count >= 1000)//1s
		{
			APP_1000ms_count = 0;
			mcu_get_wifi_work_state();
			UI_OpenPage();//延迟进入主页F
			TimerRunning();//定时F
			Wifi_Resting_Pro();//WIFI模块复位处理F
			OTAReload_DelayPage();//OTA发送完成后延迟返回主页F
			ScreenSaver_Pro();
		}
	}
}
