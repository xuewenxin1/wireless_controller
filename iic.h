#ifndef __IIC_H
#define __IIC_H

#include "sys.h"


sbit IIC_SCL=P1^7;
sbit IIC_SDA=P1^5;
sbit APDS_INT=P1^6;



#define	SDA_OUT()	P1MDOUT |= 0x20;
#define SDA_IN()	P1MDOUT	&=~0x20; 


#define	APDS9900_I2C_ADDR      		0x39
#define APDS9900_ID_VAL        		0x29


#define APDS9900_ENABLE				0x00
#define PROXIMITY_TIME_CONTROL		0x02
#define WAIT_TIME					0x03
#define PROXIMITY_PULSE_COUNT		0x0E
#define CONTROL						0x0F
#define APDS9900_ID            		0x12
#define STATUS	            		0x13

extern u16 error_val;




void IIC_Init(void);
void IIC_Start(void);
void IIC_Stop(void);
u8 IIC_Wait_Ack(void);
void IIC_Ack(void);
void IIC_NAck(void);
void IIC_Send_Byte(u8 txd);
u8 IIC_Read_Byte(u8 ack);
u8 APDS_Read_Byte(u8 reg,u8 *val);
u8 APDS_Write_Byte(u8 reg,u8 val);
void APDS_9900_Init(void);














#endif

