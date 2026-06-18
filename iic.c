/******************************************************************************

                  版权所有 (C), 2019, 北京迪文科技有限公司

 ******************************************************************************
  文 件 名   : iic.c
  版 本 号   : V1.0
  作    者   : chengjing
  生成日期   : 2019年5月5日
  功能描述   : iic功能函数，实现人体接近感应
  修改历史   :
  1.日    期   : 
    作    者   : 
    修改内容   : 
******************************************************************************/

#include "iic.h"


u16 error_val=0;



void IIC_Init(void)
{
	P1MDOUT |= 0xA0;
}



void IIC_Start(void)
{
	SDA_OUT();     		//sda线输出
	IIC_SCL=0;
	delay_us(4);
	IIC_SDA=1;
	delay_us(4);	
	IIC_SCL=1;
	delay_us(4);
 	IIC_SDA=0;			//START:when CLK is high,DATA change form high to low 
	delay_us(4);
	IIC_SCL=0;			//钳住I2C总线，准备发送或接收数据 
}


void IIC_Stop(void)
{
	SDA_OUT();			//sda线输出
	IIC_SCL=0;
	IIC_SDA=0;			//STOP:when CLK is high DATA change form low to high
 	delay_us(4);
	IIC_SCL=1; 
	IIC_SDA=1;			//发送I2C总线结束信号
	delay_us(4);							   	
}

//0：成功  1：失败
u8 IIC_Wait_Ack(void)
{
	u16 ucErrTime=0;
	SDA_IN();      		//SDA设置为输入  
	IIC_SDA=1;
	delay_us(1);	
	IIC_SCL=1;
	delay_us(1);	
	while(IIC_SDA)
	{
		ucErrTime++;
		if(ucErrTime>500)		//250
		{
			IIC_Stop();
			return 1;
		}
	}
	IIC_SCL=0;			//时钟输出0 	   
	return 0;  
} 


void IIC_Ack(void)
{
	IIC_SCL=0;
	SDA_OUT();
	IIC_SDA=0;
	delay_us(2);
	IIC_SCL=1;
	delay_us(2);
	IIC_SCL=0;
}


void IIC_NAck(void)
{
	IIC_SCL=0;
	SDA_OUT();
	IIC_SDA=1;
	delay_us(2);
	IIC_SCL=1;
	delay_us(2);
	IIC_SCL=0;
}	


void IIC_Send_Byte(u8 txd)
{                        
    u8 t;   
	SDA_OUT(); 	    
    IIC_SCL=0;			//拉低时钟开始数据传输
    for(t=0;t<8;t++)
    {              
        IIC_SDA=(txd&0x80)>>7;
        txd<<=1; 	  
		delay_us(2);   
		IIC_SCL=1;
		delay_us(4); 
		IIC_SCL=0;	
		delay_us(2);
    }	 
}

//返回收到数据
u8 IIC_Read_Byte(u8 ack)
{
	u8 i,receive=0;
	SDA_IN();				//SDA设置为输入
    for(i=0;i<8;i++)
	{
        IIC_SCL=0;
        delay_us(2);
		IIC_SCL=1;
        receive<<=1;
        if(IIC_SDA)receive++;
		delay_us(1); 
    }
    if(!ack)
        IIC_NAck();			//发送nACK
    else
        IIC_Ack(); 			//发送ACK   
    return receive;
}



//0：成功  1：失败
u8 APDS_Read_Byte(u8 reg,u8 *val)
{
	IIC_Start();
	IIC_Send_Byte((APDS9900_I2C_ADDR<<1)|0x00);
	return;
	if(IIC_Wait_Ack())
	{
		return 1;
	}
	IIC_Send_Byte(reg);
	if(IIC_Wait_Ack())
	{
		return 1;
	}
	IIC_Start();
	IIC_Send_Byte((APDS9900_I2C_ADDR<<1)|0x01);
	if(IIC_Wait_Ack())
	{
		return 1;
	}
	*val=IIC_Read_Byte(0);				//读取数据,发送nACK 
	IIC_Stop();							//产生一个停止条件 
	return 0;
}


//0：成功  1：失败
u8 APDS_Write_Byte(u8 reg,u8 val)
{
	IIC_Start(); 
	IIC_Send_Byte((APDS9900_I2C_ADDR<<1)|0x00);				//发送器件地址+写命令	
	if(IIC_Wait_Ack())
	{
		return 1;
	}
	IIC_Send_Byte(reg);				//写寄存器地址
	if(IIC_Wait_Ack())
	{
		return 1;
	} 
	IIC_Send_Byte(val);				//发送数据
	if(IIC_Wait_Ack())
	{
		return 1;
	}
    IIC_Stop();
	return 0;
}



void APDS_9900_Init(void)
{
	u8 id;
	u8 *pid;
	u8 reg_val;
	IIC_Init();	
	if(APDS_Read_Byte(APDS9900_ID,pid))
	{				
		return;
	}
	id=*pid;
	if(id==APDS9900_ID_VAL)
	{
		
	}	
	reg_val=0;
	APDS_Write_Byte(APDS9900_ENABLE,reg_val);		//失能使能寄存器
	reg_val=0xFF;
	APDS_Write_Byte(PROXIMITY_TIME_CONTROL,reg_val);			//接近时间配置  2.72ms
	reg_val=0xFF;
	APDS_Write_Byte(WAIT_TIME,reg_val);			//接近等待时间
	reg_val=0xFF;
	APDS_Write_Byte(PROXIMITY_PULSE_COUNT,reg_val);		//接近脉冲计数
	reg_val=0x20;
	APDS_Write_Byte(CONTROL,reg_val);				//控制寄存器
	reg_val=0x2D;
	APDS_Write_Byte(APDS9900_ENABLE,reg_val);		//失能使能寄存器
}














