#ifndef __SYS_H__
#define __SYS_H__

#include "t5los8051.h"
#include "wifi.h"

typedef unsigned char   u8;
typedef unsigned short  u16;
typedef unsigned long   u32;
typedef char            s8;
typedef short           s16;
typedef long            s32;

/*****************************************
*			云变量地址宏定义              *
*****************************************/
//#define		SWITCH					0x2600
//#define		SPEED					0x2601
//#define		MODE					0x2602
//#define		ALARM_KEY				0x2603
//#define		TIMER_KEY				0x2605
////#define		WATER_VALVE				0x2608
//#define		SPEED_VALVE				0x2609
//#define		TEMP_CURRENT			0x2610
//#define		TEMP_SET				0x2611
//#define		SCREEN_BRIGHTNESS		0x2612
//#define		SOFT_VER				0x2613
//#define		MESSAGE_STATUS			0x2614
//#define		LINKAGE_STATUS			0x2615
//#define		ALARM_VAL				0x2616
//#define		SCREEN_SAVER_TYPE		0x261A
//#define		REMOTE_UPDATE			0x261B
//#define		SKINNING_INSTRUCTION	0x261C
//#define		SCREEN_SAVER_TIME		0x261D
//#define		TIMER_VAL				0x2620
//#define		TIMER1_WEEK				0x2620
//#define		TIMER1_TIME_SEED_MODE	0x2621
//#define		TIMER2_WEEK				0x2632
//#define		TIMER2_TIME_SEED_MODE	0x2633
//#define		TIMER3_WEEK				0x2644
//#define		TIMER3_TIME_SEED_MODE	0x2645
//#define		MESSAGE_DATA			0x2661




/*****************************************
*			UI变量地址宏定义              *
*****************************************/

#define SET_HEAT_TEMP_ADDR				0x2903
#define SET_COOL_TEMP_ADDR				0x2904
#define SET_AIR_TEMP_ADDR				0x2002
#define SET_HOTWATER_TEMP_ADDR				0x2001
#define SET_MODE_ADDR				0x2000
#define SET_RUN_ADDR				0x3005
#define RETURN_KEY_MEMORY        0x2905



//#define		KEY_STATUS				0x2fff
//#define 	KEY_VALUE				0x2703
//#define		TOUCH_EVENT_FLAG		0x2704
//#define 	WARNING_EVENT			0x2708
//#define 	PARM_SET_ERROR			0x270C
//#define 	TIME_DISPLAY			0x2710
//#define		TIME_SET_VALUE			0x2718
//#define		TIMER_WEEK_DISPLAY		0x2720
//#define		TIMER_WEEK_SET			0x2040
//#define		TIMER_DISPLAY_PERIOD	0x2048
//#define		TIMER_SET_TIME_START	0x2050
//#define		TIMER_SET_TIME_END		0x2052
//#define		TIMER_DISPLAY_TIME_ST	0x2054
//#define		TIMER_DISPLAY_TIME_END	0x2055
//#define		TIMER_SET_TEMP			0x2058
//#define		TIMER_SET_SPEED			0x2059
//#define 	TIMER_STATUS			0x2060
//#define		ALARM_DISPLAY_TIME		0x2068
//#define		ALARM_SET_VAL			0x2072
//#define		ALARM_STATUS			0x2075
//#define		PASSWORD_INPUT			0x207A
//#define 	PASSWORD_DISPLAY		0x2080
//#define 	WIFI_DISPLAY			0x2090
//#define		SET_TEMP_VALUE			0x2100
//#define		SET_TEMP_VAL_DEC		0x2101
//#define		SET_TEMP_SET_VALUE		0x2102
//#define		SET_TEMP_SET_VAL_DEC	0x2103
//#define		TEMP_REAL_VALUE			0x2108
//#define		TEMP_REAL_VAL_DEC		0x2109


//三联供
#define		WATER_OUT_TEMP		0x4404	//出水温度
#define		WATER_IN_TEMP		0x4406		//回水温度
#define		WATER_TANK_TEMP		0x440b		//水箱温度
#define		OUTDOOR_AMBIENT_TEMP		0x4400		//室外温度
#define		INDOOR_AMBIENT_TEMP		0x4409		//室内温度







/*****************************************
*	     配置文件变量地址宏定义           *
*****************************************/
//#define 	SOFT_VER_ADDR_SUM		0x2510

#define 	SOFT_VER_ADDR			0x3613


//#define 	ALS						0x2533
//#define 	PASSWORD_ADDR			0x2538
//#define 	TEMPERATURE_COEFFICIENT	0x2550
//#define 	TEMPERATURE_PRECISION	0x2554
//#define 	TEMPERATURE_UPPER		0x2555
//#define 	TEMPERATURE_LOWER		0x2556
//#define 	CHANGE_PAGE				0x2580
//#define 	SCREEN_SAVER			0x258E

/*****************************************
*	     NORFLASH变量地址宏定义           *
*****************************************/


#define		NOR_FLASH_FIRST			0x3F00//0x25E0
#define		MEMORY_HEADER_WORDS		7
#define		MEMORY_LANGUAGE_WORDS	1
#define		MEMORY_TIMER_SLOT_WORDS	16
#define		MEMORY_INDOOR_TIMER_WORDS	16
#define		MEMORY_FLASH_DATA_LEN	255

//#define		RTC_TIME_SAVE			0x25F0



/*****************************************
*	    系统接口变量地址宏定义            *
*****************************************/
#define		NOR_FLASH				0x0008
#define		SOFT_VERSION			0x000F
#define		RTC						0x0010
#define		PIC_NOW					0x0014
#define		TP_STATUS				0x0016
#define		LED_NOW					0x0031
#define		AD_VALUE				0x0032
#define		LED_CONFIG				0x0082
#define		PIC_SET					0x0084
#define 	RTC_Set					0x009C
#define 	RTC_Set_Internet		0xF430


/*****************************************
*			wifi接口变量地址宏定义        *
*****************************************/
#define		RMA						0x0401
#define		EQUIPMENT_MODEL			0x0416
#define		QR_CODE					0x0450
#define		DISTRIBUTION_NETWORK	0x0498
#define		MAC_ADDR				0x0482
#define		WIFI_VER				0x0487
#define		DISTRIBUTION_STATUS		0x04A1
#define		NETWORK_STATUS			0x04A2
#define		RTC_NETWORK				0x04AC
#define		SSID					0x04B0
#define		WIFI_PASSWORD			0x04C0




typedef struct _dev_time
{
	u8 year;
	u8 month;
	u8 day;
	u8 week;
	u8 hour;
	u8 min;
	u8 sec;
	u8 res;
}rtc_time;




//typedef struct _alarm
//{
//	u16 alarm1_time;
//	u16 alarm2_time;
//	u16 alarm1_ring;
//	u16 alarm2_ring;
//}alarm;


#define	TempSize	100

//宏定义
#define	WDT_ON()	MUX_SEL|=0x02		/******开启看门狗*********/
#define	WDT_OFF()	MUX_SEL&=0xFD		/******关闭看门狗*********/
#define	WDT_RST()	MUX_SEL|=0x01		/******喂狗*********/


#define FOSC     206438400UL
#define T1MS    (65536-FOSC/12/1000)

//#define NULL ((void *)0)

//电源开关宏定义
//#define POWER_ON()	P2_1=1;
//#define POWER_OFF()	P2_1=0;



//三个风机阀，一个水阀，Water_Valve为水阀，Low_Speed_Valve为低速，Medium_Speed_Valve为中速，High_Speed_Valve为高速
//sbit Water_Valve=P1^3;
//sbit Low_Speed_Valve=P0^1;
//sbit Medium_Speed_Valve=P1^4;
//sbit High_Speed_Valve=P1^2;
//电源开关引脚
//sbit P2_1=P2^1;
//电压反馈，低电平表示按键有效
//sbit KEY1_Vin_F=P2^2;


extern const u8 code mon_table[12];
extern u8 	code read_flash[8];
extern u8 	code write_flash[8];
extern u8 	read_flash_status[8];


//extern u16 	Temperature_Set_Val;
//extern u16 	Alarm_Select;

//extern alarm	alarm_val;
//extern u16	Alarm_Key[2];

//extern u16 	Real_Hour_Min;
//extern u16	data Key_Count;
//extern u16	data Wait_Count;


//extern u8  	Power_Flag;
//extern u8 	Close_Valve_Flag;
//extern u16	Valve_Config_Flag;
//extern u16 	data led_new;
//extern u16 	led_old;
//extern u8  	code led_off[4];
//extern u8  	led_on[4];

//extern u8 	xdata Wind_Auto_Flag;
//extern u8 	xdata Wind_Auto_Old_Flag;
//extern u16 	Sleep_Count;
//extern u16 	Sleep_Count_M;
//extern u16 	Sleep_Flag;


//extern u8  	code page_set_to_0[4];	
//extern u8  	TP_Status_Old[8];
//extern u8  	TP_Status_New[8];


//extern u16 	R1_value;	
//extern u16 	R2_value;	
//extern u16 	R1_Temperature;
//extern u16 	R2_Temperature;

//extern u16 	Temperature_Real;
//extern u16	Temperatrue_Real_Old;
//extern u16 	xdata sleep_time;
//extern u16 	xdata Sleep_Type;


//extern u8 	xdata Screen_Flag;

extern u16 	AD_Count;
extern u8	xdata page_set[4];
extern u16	task_10ms_count;
extern u16 task_100ms_count;
extern u16	APP_1000ms_count;
extern u16	task_10000ms_count;
extern u16	g_ms_tick;
extern u16 	Flash_Save_Count;

extern u16  rx485_Count;	//485通讯
extern u16	Send_Count;

/* Modbus 轮询间隔(ms)；WiFi 上报步进间隔(ms) */
#define MODBUS_POLL_GAP_MS   100
#define WIFI_UPLOAD_GAP_MS   500

extern u16	Modbus_Error_Count;
extern u16	g_modbus_post_wr_cd;
extern u8	Modbus_Dbg_SendType;
extern u8	Modbus_Dbg_FuncCode;
extern u16	Modbus_Dbg_LastTxAddr;

//extern u16 	Alarm_Status;
//extern u16 	Timer_Status;



//extern u16 	Parm_Change_Count_Val;
//extern u8	Sleep_Val;
//extern u8	Shut_Dwon_Flag;
//extern u16	Water_Valve_Status;
//extern u16 	Speed_Valve_Status;
//extern u16	Remote_Update_Status;



//extern u16	Temperature_Precision;
//extern u16	Temp_Pre;
//extern u16	Temperature_Upper;
//extern u16 	Temperature_Lower;
//extern short Temp_Coef[3];
//extern u16 	time_display[7];
//extern u16 	Second_Updata_Flag;
//extern u8 	time_calibra[8];
//extern u16 	Key_Status;
//extern u16 	Key_Value;
//extern u16	TC_Status;
//extern u16	TC_Status_Old;



//extern u16	Screen_Saver_Parm[3];
//extern u16	ALS_Function;
//extern u16	xdata 	year_real; 
//extern u8 	xdata 	yearH;
//extern u8 	xdata 	yearL;
//extern rtc_time	real_time;
//extern u16 	Temp_time;
//extern u16	Temp_Freq;
//extern u16 	Message_Val;
//extern u16	Page_Change_Val;

//extern u16 	WIFI_Password_Flag;

extern u16	Temp_Indoor;
extern u8 	Save_Flash_Flag;
extern u16	Page_Change[10];
extern u32 	Password;

void delay_ms(u16 n);

void INIT_CPU(void);
void PORT_Init(void);
//void RTC_Init(void);
//u8 	 Set_Temperature_Handler(void);
//u8	 Alarm_Handler(void);

void System_Parm_Init(void);
void Read_Nor_Flash(void);
void Write_Nor_Flash(void);
void Save_Data_Handler(void);
void Ready_To_Save(void);
void Ready_To_Save_Report(void);

//void Key_Handler(void);
//void Key_Scheduling_Event_Handler(void);
//void Enter_Main_Page(void);
//void Standby_Handler(void);
//void Boot_Handler(void);
//void Close_Valve(void);
//void Parm_Set_Error_Handler(void);
//void Warning_Event_Handler(void);



//void Screen_Saver_Handler(u16 type);
//void Sleep_Handler(void);
//void Brightness_Handler(void);
void Brightness_Handler(unsigned char	LEDBrightness);
void Write_Memory(void);
void Read_Memory(void);
void Read_Memory_Error(void);

void T0_Init(void);
void T1_Init(void);
void T2_Init(void);
void read_dgus_vp(u32 addr,u8* buf,u16 len);
void write_dgus_vp(u32 addr,u8* buf,u16 len);
void Page_Change_Handler(u8 n);
void	Language_Change(unsigned char num);
void	ScreenTimeOut_Set(bit enable);

//u16 Get_ADC_Value(void);
//void Get_R_Value(u16 n);
//u8 	 FindTab(u16 *pTab,u8 Tablong,u16 dat);
//void Time_Calibration(void);
//u8 	 Is_Leap_Year(u16 year);
//u8 	 RTC_Get_Week(u8 year,u8 month,u8 day);
//void Time_Update(void);

/* delay_ms 仅 sys.c 内部使用，见 Read_Nor_Flash 等 */

//void wait_ok(unsigned int addr);
extern u8 g_mb_switch_wr_src;

#endif
