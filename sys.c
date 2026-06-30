/******************************************************************************

                  版权所有 (C), 2019, 北京迪文科技有限公司

 ******************************************************************************
  文 件 名   : sys.c
  版 本 号   : V2.0
  作    者   : chengjing
  生成日期   : 2019年4月1日
  功能描述   : 温控器数据采集和逻辑控制
  修改历史   :
  1.日    期   : 
    作    者   : 
    修改内容   : 
******************************************************************************/

#include "sys.h"
#include "uart.h"
#include "stdlib.h"
#include "string.h"
#include "control.h"
#include "rtc.h"
#include "ErrorHistory.h"
#include "modbus.h"
#include "upload.h"
#include "stdio.h"

extern void HomePage_UpdateModeAnimation(u8 mode);

static bit Dgus_WaitEn(u16 addr, u16 limit)
{
	while(APP_EN)
	{
		if(!limit--)
		{
			printf("[DGUS] EN TO %x\r\n", addr);
			RAMMODE = 0x00;
			EA = 1;
			return 0;
		}
		WDT_RST();
	}
	return 1;
}

static void Memory_WriteTimerSlot(u32 flash_addr, u32 vp_addr, u16 struct_bytes)
{
	unsigned short slot[MEMORY_TIMER_SLOT_WORDS];
	unsigned char i;
	unsigned char temp[16];

	for(i=0;i<MEMORY_TIMER_SLOT_WORDS;i++)
		slot[i] = 0;
	for(i=0;i<sizeof(temp);i++)
		temp[i] = 0;
	read_dgus_vp(vp_addr, temp, (u16)((struct_bytes + 1) / 2));
	for(i=0;i<struct_bytes;i++)
		((unsigned char *)slot)[i] = temp[i];
	write_dgus_vp(flash_addr, (u8 *)slot, MEMORY_TIMER_SLOT_WORDS);
}

static void Memory_ReadTimerSlot(u32 flash_addr, u32 vp_addr, u16 struct_bytes)
{
	unsigned short slot[MEMORY_TIMER_SLOT_WORDS];
	unsigned char i;
	unsigned char temp[16];

	for(i=0;i<sizeof(temp);i++)
		temp[i] = 0;
	read_dgus_vp(flash_addr, (u8 *)slot, MEMORY_TIMER_SLOT_WORDS);
	for(i=0;i<struct_bytes;i++)
		temp[i] = ((unsigned char *)slot)[i];
	write_dgus_vp(vp_addr, temp, (u16)((struct_bytes + 1) / 2));
}
//NTC 3270K_5K_25℃，下拉4.3K电阻，表值=实际值+60
const u16 TempSensorData[356]={
0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,
0,1,2,3,4,5,6,7,8,9,10,11,12,12,13,14,15,16,16,17,18,18,19,20,20,21,22,
22,23,24,24,25,26,26,27,27,28,29,29,30,30,31,32,32,33,33,34,34,35,35,36,
37,37,38,39,39,40,40,41,41,42,43,44,44,45,45,46,46,47,47,48,48,49,49,50,
51,51,52,52,53,53,54,54,55,56,56,57,57,58,58,59,60,60,61,61,62,62,63,64,
64,65,65,66,67,67,68,69,69,70,70,71,72,72,73,74,75,75,76,77,77,78,79,80,
80,81,82,83,84,84,85,86,87,88,89,90,90,91,91,92,93,94,95,96,97,98,100,101,
102,103,104,105

};


const u8 code mon_table[12]={31,28,31,30,31,30,31,31,30,31,30,31};
//读写Nor Flash命令
u8 code read_flash[8]={0x5A,0x00,0x00,0x00,0x3F,0x00,0x01,0x00};
u8 code write_flash[8]={0xA5,0x00,0x00,0x00,0x3F,0x00,0x01,0x00};
//读写Nor Flash后查询状态
u8 read_flash_status[8]={0};
//线控器软件版本号
float	code Soft_Ver_Num = 2.1;

//T2定时器计数
static u16 data SysTick=0;	

//AD延时计数
u16 AD_Count=0;

//adc采样值
//u16 adc_value[8]={0};
u16 adc_average_channel7 = 0;

u8	xdata page_set[4]={0x5A,0x01,0x00,0x3C};

//u8 	xdata Screen_Flag=0;
//任务处理函数计时
u16 task_10ms_count = 0;
u16 APP_1000ms_count = 0;
u16 task_10000ms_count = 0;

//485通讯接收到头码之后计数
u16	rx485_Count = 0;
u16	Send_Count = 0;
u16	Modbus_Error_Count = 0;

//保存Nor Flash时间计数
u16 Flash_Save_Count=0;

//切换页面
u16	Page_Change[10]={0};
//密码
u32 Password=0;
//室内温度(线控器)F	
u16	Temp_Indoor = 0;

//需要写flash 标志
u8 	Save_Flash_Flag=0;

/*****************************************************************************
 函 数 名  : void INIT_CPU(void)
 功能描述  : CPU初始化函数
			根据实际使用外设修改或单独配置
 输入参数  :	
 输出参数  : 
 修改历史  :
  1.日    期   : 2019年4月1日
    作    者   : chengjing
    修改内容   : 创建
*****************************************************************************/ 
void INIT_CPU(void)
{
    EA=0;
    RS0=0;
    RS1=0;
	
    PORTDRV=0x01;   //驱动强度+/-8mA
    IEN0=0x00;      //关闭所有中断
    IEN1=0x00;
    IEN2=0x00;
//    IP0=0x00;      //中断优先级默认
//    IP1=0x00;
	IP0=0x20;//0010 0000
	IP1=0x30;//0011 0000

    WDT_OFF();      //关闭开门狗

    //IO口配置
    P0=0x00;
    P1=0x00;
    P2=0x00;
    P3=0x00;
    P0MDOUT=0x10;
    P1MDOUT=0x00;
    P2MDOUT=0x00; 
    P3MDOUT=0x00;
	
	//UART2配置8N1  115200       有倍频，和UART3不一样
    ADCON=0x80;
    SCON0=0x50;
    SREL0H=0x03;        //FCLK/64*(1024-SREL1)
    SREL0L=0xE4;
//	IP0=0x10;
//	IP1=0x30;

    //UART4配置8N1      115200
    SCON2T=0x80;
    SCON2R=0x80;
    BODE2_DIV_H=0x00;     //FCLK/8*DIV
    BODE2_DIV_L=0xE0;

    //UART5配置8N1      115200
    SCON3T=0x80;
    SCON3R=0x80;
    BODE3_DIV_H=0x00;       //FCLK/8*DIV
    BODE3_DIV_L=0xE0;
//	IP0=0x20;      //中断优先级默认
//    IP1=0x20;

    
    TMOD=0x11;          //16位定时器
    //T0
    TH0=0x00;
    TL0=0x00;
    TR0=0x00;

    //T1
    TH1=0x00;
    TL1=0x00;
    TR1=0x00;
    

    //T2  Autoload模式
    T2CON=0x70;
    TH2=0x00;
    TL2=0x00;
    TRL2H=0xBC;
    TRL2L=0xCD;        //1ms的定时器
}

/*****************************************************************************
 函 数 名  : void PORT_Init(void)
 功能描述  : 端口初始化函数
 输入参数  :	
 输出参数  : 
 修改历史  :
  1.日    期   : 2019年4月1日
    作    者   : chengjing
    修改内容   : 创建
*****************************************************************************/ 
void PORT_Init(void)
{
	P0MDOUT |= 0x02;	//初始化P0.1为输出，485控制
	P1MDOUT |= 0x1F;	//初始化P1.1 P1.2 P1.3 P1.4为输出,P1.0输出
	P2MDOUT |= 0x02;	//初始化P2.1为输出
}

#define	FLASHCALCLEN	16 // 计算flash检验和的单次读取数量(小于256)加大加可速计算 缩小可节省空间F
unsigned short FlashCheckSumCalculate(unsigned short StartAddr, unsigned short Length) // 输入长度小于256*FLASHCALCLEN
{
	unsigned char i, j, Page, Num;
	unsigned short FlashCheckSum_Add = 0;
	unsigned short FlashCheckSum_Buff[FLASHCALCLEN] = {0};

	Page = Length / FLASHCALCLEN; // 完整的页面数量F
	for (i = 0; i < Page; i++)
	{
		read_dgus_vp((u32)(StartAddr + i * FLASHCALCLEN), (u8 *)&FlashCheckSum_Buff, FLASHCALCLEN);
		for (j = 0; j < FLASHCALCLEN; j++)
		{
			FlashCheckSum_Add += FlashCheckSum_Buff[j];
		}
	}
	Num = Length % FLASHCALCLEN; // 剩余部分数量F
	read_dgus_vp((u32)(StartAddr + Page * FLASHCALCLEN), (u8 *)&FlashCheckSum_Buff, Num);
	for (j = 0; j < Num; j++)
	{
		FlashCheckSum_Add += FlashCheckSum_Buff[j];
	}
	FlashCheckSum_Add += 1;
	return FlashCheckSum_Add;
}
/*****************************************************************************
 函 数 名  : void System_Parm_Init(void)
 功能描述  : 系统参数初始化，读取保存Nor Flash区域和22.bin文件系统参数，并配置软件
			版本号等参数。
 输入参数  :	 
 输出参数  : 
 修改历史  :
  1.日    期   : 2019年5月16日
    作    者   : chengjing
    修改内容   : 创建
*****************************************************************************/
void System_Parm_Init(void)
{
	unsigned short	FlashCheckSum = 0;
	
	Read_Nor_Flash();
	delay_ms(2);
	read_dgus_vp(NOR_FLASH_FIRST,(u8*)&FlashCheckSum,1);
	if(FlashCheckSumCalculate(NOR_FLASH_FIRST+1,MEMORY_FLASH_DATA_LEN) != FlashCheckSum)
	{
		Read_Memory_Error();
	}
	else
	{
		Read_Memory();
	}
}


/*****************************************************************************
 函 数 名  : void Read_Nor_Flash(void)
 功能描述  : 读nor flash系统参数，固定为0x3F00-0x3FFF的值
 输入参数  :	 
 输出参数  : 
 修改历史  :
  1.日    期   : 2019年5月2日
    作    者   : chengjing
    修改内容   : 创建
*****************************************************************************/
void Read_Nor_Flash(void)
{	
	write_dgus_vp(NOR_FLASH,(u8*)read_flash,4);
	do
	{
		delay_ms(5);
		read_dgus_vp(NOR_FLASH,(u8*)read_flash_status,4);

	}while(read_flash_status[0]!=0);
}


/*****************************************************************************
 函 数 名  : void Write_Nor_Flash(void)
 功能描述  : 写nor flash系统参数，固定为0x3F00-0x3FFF的值
 输入参数  :	 
 输出参数  : 
 修改历史  :
  1.日    期   : 2019年5月2日
    作    者   : chengjing
    修改内容   : 创建
*****************************************************************************/
void Write_Nor_Flash(void)
{
	write_dgus_vp(NOR_FLASH,(u8*)write_flash,4);
	do
	{
		delay_ms(5);
		read_dgus_vp(NOR_FLASH,(u8*)read_flash_status,4);		
	}while(read_flash_status[0]!=0);
}

/*****************************************************************************
 函 数 名  : void Save_Data_Handler(void)
 功能描述  : 保存nor flash的值，参数设置完10s后保存，避免短时间重复保存
 输入参数  :	 
 输出参数  : 
 修改历史  :
  1.日    期   : 2019年5月2日
    作    者   : chengjing
    修改内容   : 创建
*****************************************************************************/
void Save_Data_Handler(void)
{
	u16 temp = 0;

	if(Save_Flash_Flag == 1)
	{
		if(Flash_Save_Count > 10000)
		{
			Write_Memory();
			Write_Nor_Flash();
			Save_Flash_Flag = 0;
			Flash_Save_Count = 0;
		}
	}
	
	if(Flash_Save_Count > 60000)//防止溢出F
	{
		Flash_Save_Count = 0;
	}
}

/*****************************************************************************
 函 数 名  : void Ready_To_Save(void)
 功能描述  : 若掉电记忆使能，保存数据置标志，开始计时，参数设置完10s后保存，避免短时间重复保存F*
 输入参数  :	 
 输出参数  : 
 修改历史  :
  1.日    期   : 2019年5月2日
    作    者   : chengjing
    修改内容   : 创建
*****************************************************************************/

void Ready_To_Save(void)
{
	u16 Enable_Memory = 0;
	read_dgus_vp((u32)(0x3508),(u8 *)&Enable_Memory,1);
	if(Enable_Memory == 1)
	{
		Save_Flash_Flag = 1;
		Flash_Save_Count = 0;
	}
	else
	{
		Save_Flash_Flag = 0;
		Flash_Save_Count = 0;
	}
}

void Ready_To_Save_Report(void)
{
	Ready_To_Save();
}

void Brightness_Handler(unsigned char	LEDBrightness)
{
	unsigned char	Brightness_Handler_u8temp[2];
	
	Brightness_Handler_u8temp[0] = LEDBrightness;//亮屏亮度F
	Brightness_Handler_u8temp[1] = 0;//超时亮度F
	write_dgus_vp(LED_CONFIG,Brightness_Handler_u8temp,1);
}



/*****************************************************************************
 函 数 名  : void Write_Memory(void)
 功能描述  : 掉电前，存放需要记忆的参数到指定的地址(0x3F00-0x3FFF)	F
 输入参数  :	 	
 输出参数  : 
 修改历史  :
  1.日    期   : 2019年4月26日
    作    者   : chengjing
    修改内容   : 创建
*****************************************************************************/
void Write_Memory(void)
{
//	extern EXTERN_TIMER_T ExternTimer;
//	extern EXTERN_TIMER_T1 ExternTimer1;
//	unsigned short	Write_Memory_u16temp;
//	unsigned short	Write_Memory_Addr = NOR_FLASH_FIRST;
//	
//	Write_Memory_u16temp = 0;
//	read_dgus_vp((u32)(0x3508),(u8*)&Write_Memory_u16temp,1);
//	write_dgus_vp((u32)(++Write_Memory_Addr),(u8*)&Write_Memory_u16temp,1);
//	
//	read_dgus_vp((u32)(0x2001),(u8*)&Write_Memory_u16temp,1);
//	write_dgus_vp((u32)(++Write_Memory_Addr),(u8*)&Write_Memory_u16temp,1);
//	
//	read_dgus_vp((u32)(0x2002),(u8*)&Write_Memory_u16temp,1);
//	write_dgus_vp((u32)(++Write_Memory_Addr),(u8*)&Write_Memory_u16temp,1);
//	
//	read_dgus_vp((u32)(0x2003),(u8*)&Write_Memory_u16temp,1);
//	write_dgus_vp((u32)(++Write_Memory_Addr),(u8*)&Write_Memory_u16temp,1);
//	
//	read_dgus_vp((u32)(0x2005),(u8*)&Write_Memory_u16temp,1);
//	write_dgus_vp((u32)(++Write_Memory_Addr),(u8*)&Write_Memory_u16temp,1);
//	
//	read_dgus_vp((u32)(0x2006),(u8*)&Write_Memory_u16temp,1);
//	write_dgus_vp((u32)(++Write_Memory_Addr),(u8*)&Write_Memory_u16temp,1);
//	
//	read_dgus_vp((u32)(0x201E),(u8*)&Write_Memory_u16temp,1);
//	write_dgus_vp((u32)(++Write_Memory_Addr),(u8*)&Write_Memory_u16temp,1);
//	
//	Write_Memory_u16temp = Language_Num;
//	write_dgus_vp((u32)(++Write_Memory_Addr),(u8*)&Write_Memory_u16temp,1);
//	
//	Memory_WriteTimerSlot((u32)(NOR_FLASH_FIRST + MEMORY_HEADER_WORDS + MEMORY_LANGUAGE_WORDS + 1),
//		(u32)0x4820, (u16)sizeof(ExternTimer));
//	Memory_WriteTimerSlot((u32)(NOR_FLASH_FIRST + MEMORY_HEADER_WORDS + MEMORY_LANGUAGE_WORDS + 1 + MEMORY_TIMER_SLOT_WORDS),
//		(u32)0x4830, (u16)sizeof(ExternTimer1));
//	Write_Memory_Addr = NOR_FLASH_FIRST + MEMORY_HEADER_WORDS + MEMORY_LANGUAGE_WORDS + MEMORY_TIMER_SLOT_WORDS + MEMORY_INDOOR_TIMER_WORDS;
//	
//	Write_Memory_u16temp = Screensaver_Enable;
//	write_dgus_vp((u32)(++Write_Memory_Addr),(u8*)&Write_Memory_u16temp,1);
//	
//	ErrorHistory_FlashWrite((u32)(++Write_Memory_Addr));
//	
//	Write_Memory_u16temp = FlashCheckSumCalculate(NOR_FLASH_FIRST+1,MEMORY_FLASH_DATA_LEN);
//	write_dgus_vp((u32)(NOR_FLASH_FIRST),(u8*)&Write_Memory_u16temp,1);
}
/*****************************************************************************
 函 数 名  : void Read_Memory(void)
 功能描述  : 恢复上电后，从指定地址(0x3F00-0x3FFF)读取记忆参数
 输入参数  :	 	
 输出参数  : 
 修改历史  :
  1.日    期   : 2019年4月26日
    作    者   : chengjing
    修改内容   : 创建
*****************************************************************************/
void Read_Memory(void)
{
//	extern EXTERN_TIMER_T ExternTimer;
//	extern EXTERN_TIMER_T1 ExternTimer1;
//	unsigned short	Read_Memory_u16temp = 0;
//	unsigned short	Read_Memory_u16temp1 = 0;
//	unsigned short	home_mode_saved = 0;
//	unsigned short	Read_Memory_Addr = NOR_FLASH_FIRST;
//	
//	read_dgus_vp((u32)(++Read_Memory_Addr),(u8*)&Read_Memory_u16temp,1);
//	write_dgus_vp((u32)(0x3508),(u8*)&Read_Memory_u16temp,1);
//	
//	Read_Memory_u16temp = 0;
//	read_dgus_vp((u32)(++Read_Memory_Addr),(u8*)&Read_Memory_u16temp,1);
//	write_dgus_vp((u32)(0x2001),(u8*)&Read_Memory_u16temp,1);
//	home_mode_saved = Read_Memory_u16temp;
//	HomePage_UpdateModeAnimation((u8)home_mode_saved);
//	
//	Read_Memory_u16temp = 0;
//	read_dgus_vp((u32)(++Read_Memory_Addr),(u8*)&Read_Memory_u16temp,1);
//	write_dgus_vp((u32)(0x2002),(u8*)&Read_Memory_u16temp,1);
//	
//	Read_Memory_u16temp1 = 0;
//	read_dgus_vp((u32)(++Read_Memory_Addr),(u8*)&Read_Memory_u16temp1,1);
//	write_dgus_vp((u32)(0x2003),(u8*)&Read_Memory_u16temp1,1);
//	
//	Read_Memory_u16temp = 0;
//	read_dgus_vp((u32)(++Read_Memory_Addr),(u8*)&Read_Memory_u16temp,1);
//	write_dgus_vp((u32)(0x2005),(u8*)&Read_Memory_u16temp,1);
////	Temp_Limit(0x2005,'C');
//	
//	Read_Memory_u16temp = 0;
//	read_dgus_vp((u32)(++Read_Memory_Addr),(u8*)&Read_Memory_u16temp,1);
//	write_dgus_vp((u32)(0x2006),(u8*)&Read_Memory_u16temp,1);
////	Temp_Limit(0x2006,'F');
//	
//	Read_Memory_u16temp = 0;
//	read_dgus_vp((u32)(++Read_Memory_Addr),(u8*)&Read_Memory_u16temp,1);
//	if((Read_Memory_u16temp > 100)||(Read_Memory_u16temp < 1))
//		Read_Memory_u16temp = 100;
//	write_dgus_vp((u32)(0x201e),(u8*)&Read_Memory_u16temp,1);
//	Brightness_Handler(Read_Memory_u16temp);
//	
//	Read_Memory_u16temp = 0;
//	read_dgus_vp((u32)(++Read_Memory_Addr),(u8*)&Read_Memory_u16temp,1);
//	if((Read_Memory_u16temp == 23)||(Read_Memory_u16temp == 194))
//	{
//		Language_Num = Read_Memory_u16temp;
//		Language_Change((unsigned char)Language_Num);
//		write_dgus_vp((u32)(0x2018),(u8*)&Language_Num,1);
//	}
//	
//	Memory_ReadTimerSlot((u32)(NOR_FLASH_FIRST + MEMORY_HEADER_WORDS + MEMORY_LANGUAGE_WORDS + 1),
//		(u32)0x4820, (u16)sizeof(ExternTimer));
//	Read_Memory_Addr = NOR_FLASH_FIRST + MEMORY_HEADER_WORDS + MEMORY_LANGUAGE_WORDS + MEMORY_TIMER_SLOT_WORDS;
//	
//	Memory_ReadTimerSlot((u32)(NOR_FLASH_FIRST + MEMORY_HEADER_WORDS + MEMORY_LANGUAGE_WORDS + 1 + MEMORY_TIMER_SLOT_WORDS),
//		(u32)0x4830, (u16)sizeof(ExternTimer1));
//	Read_Memory_Addr = NOR_FLASH_FIRST + MEMORY_HEADER_WORDS + MEMORY_LANGUAGE_WORDS + MEMORY_TIMER_SLOT_WORDS + MEMORY_INDOOR_TIMER_WORDS;
//	
//	read_dgus_vp((u32)(++Read_Memory_Addr),(u8*)&Read_Memory_u16temp,1);
//	if(Read_Memory_u16temp)
//		Read_Memory_u16temp = 1;
//	else
//		Read_Memory_u16temp = 0;
//	Screensaver_Enable = Read_Memory_u16temp;
//	ScreenTimeOut_Set(!Screensaver_Enable);
//	write_dgus_vp((u32)(0x2038),(u8*)&Read_Memory_u16temp,1);
//	
//	ErrorHistory_FlashRead((u32)(++Read_Memory_Addr));
//	ErrorHistory_Sanitize();
}
void Read_Memory_Error(void)
{
//	unsigned short Read_Memory_Error_u16temp;
//	unsigned char i,j;

//	Read_Memory_Error_u16temp = 1;
//	write_dgus_vp((u32)(0x3508), (u8 *)&Read_Memory_Error_u16temp, 1);
//	
//	Read_Memory_Error_u16temp = 1;
//	write_dgus_vp((u32)(0x2001), (u8 *)&Read_Memory_Error_u16temp, 1);

//	Read_Memory_Error_u16temp = 2;
//	write_dgus_vp((u32)(0x2002), (u8 *)&Read_Memory_Error_u16temp, 1);

//	Read_Memory_Error_u16temp = 0;
//	write_dgus_vp((u32)(0x2003), (u8 *)&Read_Memory_Error_u16temp, 1);

//	Read_Memory_Error_u16temp = 26;
//	write_dgus_vp((u32)(0x2005), (u8 *)&Read_Memory_Error_u16temp, 1);

//	Read_Memory_Error_u16temp = 86;
//	write_dgus_vp((u32)(0x2006), (u8 *)&Read_Memory_Error_u16temp, 1);

//	Language_Change((unsigned char)Language_Num);
//	write_dgus_vp((u32)(0x2018), (u8 *)&Language_Num, 1);

//	Read_Memory_Error_u16temp = 100;
//	Brightness_Handler(Read_Memory_Error_u16temp);
//	write_dgus_vp((u32)(0x201E), (u8 *)&Read_Memory_Error_u16temp, 1);

//	Read_Memory_Error_u16temp = FALSE;
//	Screensaver_Enable = Read_Memory_Error_u16temp;
//	ScreenTimeOut_Set(!Screensaver_Enable);
//	write_dgus_vp((u32)(0x2038), (u8 *)&Read_Memory_Error_u16temp, 1);

//	for(i=0;i<ERRORHISTORYNUM;i++)
//	{
//		for(j=0;j<5;j++)
//			ErrorHistory[i][j] = 0;
//	}
//	Modbus_Write_Ueser = TRUE;
}
/*****************************************************************************
 函 数 名  : void T0_Init(void)
 功能描述  : 定时器0初始化	定时间隔1ms
 输入参数  :	 
 输出参数  : 
 修改历史  :
  1.日    期   : 2019年4月2日
    作    者   : chengjing
    修改内容   : 创建
*****************************************************************************/
void T0_Init(void)
{
    TMOD|=0x01;
    TH0=T1MS>>8;        //1ms定时器
    TL0=T1MS;
    ET0=1;              //开启定时器0中断
    EA=1;               //开总中断
    TR0=1;              //开启定时器
}


/*****************************************************************************
 函 数 名  : void T1_Init(void)
 功能描述  : 定时器1初始化	定时间隔1ms
 输入参数  :	 
 输出参数  : 
 修改历史  :
  1.日    期   : 2019年4月2日
    作    者   : chengjing
    修改内容   : 创建
*****************************************************************************/
void T1_Init(void)
{
    TMOD|=0x01;
    TH1=T1MS>>8;        //1ms定时器
    TL1=T1MS;
    ET1=1;              //开启定时器0中断
    EA=1;               //开总中断
    TR1=1;              //开启定时器
}



/*****************************************************************************
 函 数 名  : void T2_Init(void)
 功能描述  : 定时器2初始化	定时间隔1ms
 输入参数  :	 
 输出参数  : 
 修改历史  :
  1.日    期   : 2019年4月2日
    作    者   : chengjing
    修改内容   : 创建
*****************************************************************************/
void T2_Init(void)
{
    T2CON=0x70;
    TH2=0x00;
    TL2=0x00;
    TRL2H=0xBC;
    TRL2L=0xCD;        //1ms的定时器
    IEN0|=0x20;     //开启定时器2
    TR2=0x01;
    EA=1;
}






/*****************************************************************************
 函 数 名  : void read_dgus_vp(u32 addr,u8* buf,u16 len)
 功能描述  : 读dgus地址的值
 输入参数  :	 addr：dgus地址值  len：读数据长度
 输出参数  : buf：数据保存缓存区
 修改历史  :
  1.日    期   : 2019年6月20日
    作    者   : chengjing
    修改内容   : 修改读写dgus流程，不使用嵌套
*****************************************************************************/
void read_dgus_vp(u32 addr, u8 *buf, u16 len)
{
	u16 OS_addr = 0;
	u16 OS_addr_offset = 0;
	u16 OS_len = 0, OS_len_offset = 0;
	u32 LenLimit;
	if(0==len)
		return;
	LenLimit = 0xffffU - addr + 1;
	if(LenLimit < len)
	{
		len = LenLimit;
	}
	OS_addr = addr >> 1;
	OS_addr_offset = addr & 0x01;
#ifdef INTVPACTION	
	EA = 0;
#endif
	ADR_H = 0;
	ADR_M = (u8)(OS_addr >> 8);
	ADR_L = (u8)OS_addr;
	
	ADR_INC = 1;	
	RAMMODE = 0xAF; 
#if 1
	while (!APP_ACK);			
	if (OS_addr_offset)
	{
		APP_EN = 1;
		while (APP_EN);
		*buf++ = DATA1;
		*buf++ = DATA0;
		len--;
	}
	OS_len = len >> 1;
	OS_len_offset = len & 0x01;
	while (OS_len--)
	{
		APP_EN = 1;
		while (APP_EN);
		*buf++ = DATA3;
		*buf++ = DATA2;
		*buf++ = DATA1;
		*buf++ = DATA0;
	}
	if (OS_len_offset)
	{
		APP_EN = 1;
		while (APP_EN);
		*buf++ = DATA3;
		*buf++ = DATA2;
	}
	RAMMODE = 0x00;
#ifdef INTVPACTION	
	EA = 1;
#endif
#endif
//	{
//		u16 dgus_to = 800;
//		while (!APP_ACK)
//		{
//			if(!--dgus_to)
//			{
//				RAMMODE = 0x00;
//				EA = 1;
//				printf("[DGUS] read TO %x\r\n", (u16)addr);
//				return;
//			}
//			WDT_RST();
//		}
//	}
//	if (OS_addr_offset)
//	{
//		APP_EN = 1;
//		if(!Dgus_WaitEn((u16)addr, 800))
//			return;
//		*buf++ = DATA1;
//		*buf++ = DATA0;
//		len--;
//	}
//	OS_len = len >> 1;
//	OS_len_offset = len & 0x01;
//	while (OS_len--)
//	{
//		APP_EN = 1;
//		if(!Dgus_WaitEn((u16)addr, 800))
//			return;
//		*buf++ = DATA3;
//		*buf++ = DATA2;
//		*buf++ = DATA1;
//		*buf++ = DATA0;
//	}
//	if (OS_len_offset)
//	{
//		APP_EN = 1;
//		if(!Dgus_WaitEn((u16)addr, 800))
//			return;
//		*buf++ = DATA3;
//		*buf++ = DATA2;
//	}
//	RAMMODE = 0x00;
//	EA = 1;
}



/*****************************************************************************
 函 数 名  : void write_dgus_vp(u32 addr,u8* buf,u16 len)
 功能描述  : 写dgus地址数据
输入参数  :	 addr：写地址值	buf：写入的数据保存缓存区 len：字长度
 输出参数  : 
 修改历史  :
  1.日    期   : 2019年6月20日
    作    者   : chengjing
    修改内容   : 修改读写dgus流程，不使用嵌套
*****************************************************************************/
void write_dgus_vp(u32 addr, u8 *buf, u16 len)
{
	u16 OS_addr = 0;
	u16 OS_addr_offset = 0;
	u16 OS_len = 0,OS_len_offset = 0;
	u16 LenLimit;
	if(0==len)
		return;
	LenLimit = 0xffffU - addr + 1;
	if(LenLimit < len)
	{
		len = LenLimit;
	}
	OS_addr = addr >> 1;
	OS_addr_offset = addr & 0x01;
#ifdef INTVPACTION	
	EA = 0;
#endif
//	EA = 0;
	ADR_H = 0;
	ADR_M = (u8)(OS_addr >> 8);
	ADR_L = (u8)OS_addr;
	
	ADR_INC = 0x01; 
	RAMMODE = 0x83;
	#if 0
	{
		u16 dgus_to = 800;
		while (!APP_ACK)
		{
			if(!--dgus_to)
			{
				RAMMODE = 0x00;
				EA = 1;
				printf("[DGUS] write TO %x\r\n", (u16)addr);
				return;
			}
			WDT_RST();
		}
	}
	if (OS_addr_offset)
	{
		DATA1 = *buf++;
		DATA0 = *buf++;
		APP_EN = 1;
		if(!Dgus_WaitEn((u16)addr, 800))
			return;
		len--;
	}
	OS_len = len >> 1;
	OS_len_offset = len & 0x01;
	RAMMODE = 0x8F;
	while (OS_len--)
	{
		DATA3 = *buf++;
		DATA2 = *buf++;
		DATA1 = *buf++;
		DATA0 = *buf++;
		APP_EN = 1;
		if(!Dgus_WaitEn((u16)addr, 800))
			return;
	}
	if (OS_len_offset)
	{
		RAMMODE = 0x8C;
		DATA3 = *buf++;
		DATA2 = *buf++;
		APP_EN = 1;
		if(!Dgus_WaitEn((u16)addr, 800))
			return;
	}
	RAMMODE = 0x00;
	EA = 1;
	#else
	while (!APP_ACK);
	if (OS_addr_offset)
	{
		DATA1 = *buf++;
		DATA0 = *buf++;
		APP_EN = 1;
		while (APP_EN);
		len--;
	}
	OS_len = len >> 1;
	OS_len_offset = len & 0x01;
	RAMMODE = 0x8F;
	while (OS_len--)
	{
		DATA3 = *buf++;
		DATA2 = *buf++;
		DATA1 = *buf++;
		DATA0 = *buf++;
		APP_EN = 1;
		while (APP_EN);
	}
	if (OS_len_offset)
	{
		RAMMODE = 0x8C;
		DATA3 = *buf++;
		DATA2 = *buf++;
		APP_EN = 1;
		while (APP_EN);
	}
	RAMMODE = 0x00;
#ifdef INTVPACTION	
	EA = 1;
#endif
	#endif
}




/*****************************************************************************
 函 数 名  : void T0_ISR_PC(void)    interrupt 1
 功能描述  : 定时器0处理函数，毫秒增加
 输入参数  :	 
 输出参数  : 
 修改历史  :
  1.日    期   : 2019年4月2日
    作    者   : chengjing
    修改内容   : 创建
*****************************************************************************/
void T0_ISR_PC(void)    interrupt 1
{
    EA=0;
    TH0=T1MS>>8;
    TL0=T1MS;
	
	Flash_Save_Count++;
	AD_Count++;
	rx485_Count++;
	task_10ms_count++;	
	APP_1000ms_count++;	
//	printf("APP_1000ms_count1 = %d\r\n",APP_1000ms_count);
	task_10000ms_count++;	
	Send_Count++;
	if(LockKeyCountEnalbe)
	{
		if(LockKeyCount < 65535)
			LockKeyCount++;
	}
	if(modbus_error)
	{
		if(Modbus_Error_Count < 65535)
			Modbus_Error_Count++;
	}
	if(Screensaver_Count<SCREENSAVETIME)
		Screensaver_Count++;
//	printf("test3\r\n");
    EA=1;
}


/*****************************************************************************
 函 数 名  : void T1_ISR_PC(void)    interrupt 3
 功能描述  : 定时器1处理函数，提供RTC计时和处理
 输入参数  :	 
 输出参数  : 
 修改历史  :
  1.日    期   : 2019年4月2日
    作    者   : chengjing
    修改内容   : 创建
*****************************************************************************/
void T1_ISR_PC(void)    interrupt 3
{
    EA=0;
    TH1=T1MS>>8;
    TL1=T1MS;
	
	ulSystickCount++; //RTC更新计时，每500ms更新一次F
    EA=1;
}



/*****************************************************************************
 函 数 名  : void delay_ms(u16 n)
 功能描述  : 延时函数，使用定时器2硬件延时，//尽量不用F
 输入参数  :	 
 输出参数  : 
 修改历史  :
  1.日    期   : 2019年4月2日
    作    者   : chengjing
    修改内容   : 创建
*****************************************************************************/
void delay_ms(u16 n)
{
    SysTick=n;
    while(SysTick);   
}

/*****************************************************************************
 函 数 名  : void T2_ISR_PC(void)    interrupt 5
 功能描述  : 定时器2中断处理函数，提供延时函数计数和喂狗
 输入参数  :	 
 输出参数  : 
 修改历史  :
  1.日    期   : 2019年4月2日
    作    者   : chengjing
    修改内容   : 创建
*****************************************************************************/
void T2_ISR_PC(void)    interrupt 5
{
	EA=0;
    TF2=0;    
    SysTick--;
	EA=1;
}

/*****************************************************************************
延时us*//*振荡周期T=1/206438400*/
void DelayUs(u16 n)
{
  u16 i,j;
  for(i=0;i<n;i++)
    for(j=0;j<15;j++);
}
/*****************************************************************************
延时ms*/
void DelayMs(u16 n)
{
  u16 i,j;
  for(i=0;i<n;i++)
    for(j=0;j<7400;j++);
}

/*****************************************************************************
 函 数 名  : void Page_Change_Handler(u8 n)
 功能描述  : 页面切换函数
 输入参数  :	  n:需切换的页面数
 输出参数  : 
 修改历史  :
  1.日    期   : 2019年4月26日
    作    者   : chengjing
    修改内容   : 创建
*****************************************************************************/ 
void Page_Change_Handler(u8 n)
{
//	u16 wait = 400;
//	u8 pic_status[2];

//	page_set[0]=0x5A;
//	page_set[1]=0x01;
//	page_set[2]=0x00;
//	page_set[3]=n;
//	write_dgus_vp(PIC_SET,page_set,2);
//	do
//	{
//		WDT_RST();
//		delay_ms(5);
//		pic_status[0] = 0x5A;
//		read_dgus_vp((u32)(PIC_SET),(u8 *)&pic_status,1);
//		if(pic_status[0] == 0)
//			break;
//	}while(--wait);
	page_set[0]=0x5A;
	page_set[1]=0x01;
	page_set[2]=0x00;
	page_set[3]=n;
	write_dgus_vp(PIC_SET,page_set,2);
	do
	{
    delay_ms(5);
	  read_dgus_vp((u32)(PIC_SET),(u8 *)&page_set,1);
	}while(page_set[0]!=0);
}

void	Language_Change(unsigned char num)
{
	unsigned char	Language_Change_Buffer[4];
	
	Language_Change_Buffer[0] = 0x5A;
	Language_Change_Buffer[1] = 0;
	Language_Change_Buffer[2] = 0;
	Language_Change_Buffer[3] = num;
	write_dgus_vp(0xDE,Language_Change_Buffer,2);
}
void	ScreenTimeOut_Set(bit enable)
{
	unsigned char	ScreenTimeOut_Set_Temp[4];
	
	read_dgus_vp((u32)(0x0080),(u8 *)&ScreenTimeOut_Set_Temp,2);
	if(enable)
		ScreenTimeOut_Set_Temp[3] |= 0x04;
	else
		ScreenTimeOut_Set_Temp[3] &= 0xFB;
	ScreenTimeOut_Set_Temp[0] = 0x5A;
	write_dgus_vp((u32)(0x0080),(u8*)&ScreenTimeOut_Set_Temp,2);
}

////检查指定DGUS变量地址的DATA3清零的话就退出
//void wait_ok(unsigned int addr)
//{	
//	ADR_H=0x00;
//	ADR_M=(unsigned char)(addr>>8);
//	ADR_L=(unsigned char)(addr);
//	ADR_INC=0x00;
//	do
//	{ 
//		for(addr=0;addr<1000;addr++)	//释放变量空间一段时间
//			DATA2=DATA1;
//		RAMMODE=0xAF;
//		while(APP_ACK==0);
//		APP_EN=1;
//		while(APP_EN==1);
//		RAMMODE=0x00;
//	}	
//	while(DATA3!=0);
//}
