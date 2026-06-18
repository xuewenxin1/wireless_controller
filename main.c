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
#include "ErrorHistory.h"
#include "OTA.h"


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
	if(!TuyaOTAState)
//		;
		Modbus_Salve_Handler1();
	else
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
		
		if(task_10ms_count >= 10)//10ms
		{
			task_10ms_count = 0;
//			
			Control_Function1();				//逻辑控制函数入口F
//			Save_Data_Handler();
//			ModbusChanelControl();//普通通讯与OTA通讯切换F
//			RTCUpdate();//外部RTC时间更新		F

		}
	
//		if(task_10000ms_count >= 10000)//10s
//		{
//			task_10000ms_count = 0;
//			
//			HomePage_Icon();//主页右上角小图标刷新F
//			DecodeErrorHistory();//历史故障记录F
//		}
		
		if(APP_1000ms_count >= 1000)//1s
		{
			mcu_get_wifi_work_state();
			APP_1000ms_count = 0;
			UI_OpenPage();//延迟进入主页F
//			TimerRunning();//定时F
			Wifi_Resting_Pro();//WIFI模块复位处理F
//			OTAReload_DelayPage();//OTA发送完成后延迟返回主页F
//			ScreenSaver_Pro();
//			if(!TuyaOTAState)//非OTA状态下F
//				all_data_update();	//线控器向APP发送状态F
		}
	
	}
}
