#include "uart.h"
#include "sys.h"
#include "modbus.h"
#include "wifi.h"
#include "OTA.h"

typedef enum{
	OTAEncode,
	OTARecv,
	OTADecode
}OTAStepEnum;
typedef enum{
	SendStart,
	SendData
}OTASendStepEnum;
typedef enum{
	RecvIdle,
	RecvHead,
	RecvId,
	RecvData
}OTARecvStepEnum;

#define OTA_BUFFSIZE   300
#define  OTA_HEAD		0x5A
#define  OTA_HOST		0xFA
#define  OTA_SLAVE	0x01
u8	OTA_ModbusStep;
u8	OTA_ModbusSendStep;
u8	OTA_ModbusRecvStep;
u8	OTAStartSendTimes = 0;
u16	OTAModbusRecvIndex = 0;
u16	OTAModbusRecvDelay = 0;
u8	OTAModbusRecvLenth = 0;
bit	OTAReply = FALSE;
bit	OTAStartReply = FALSE;
bit TuyaOTAState = FALSE;
bit	OTAReloading = TRUE;
unsigned short TuyaOTADisConnectCnt = 0;

void	OTAEncodePro(void);
void	OTARecvPro(void);
void	OTADecodePro(void);

void OTAinit(void)
{
	u16	i;
	for(i=0;i<OTA_BUFFSIZE;i++)
	{
		Modbus_Buffer[i] = 0;
	}
	OTA_ModbusStep = OTAEncode;
	OTAStartSendTimes = 0;
	TuyaOTADisConnectCnt = 0;
	OTAReply = FALSE;
	OTA_ModbusSendStep = SendStart;
	OTA_ModbusRecvStep = RecvIdle;
}
void OTA_ModbusPro(void)
{
	switch(OTA_ModbusStep)
	{
		case	OTAEncode:
			OTAEncodePro();
		break;
		case	OTARecv:
			OTARecvPro();
		break;
		case	OTADecode:
			OTADecodePro();
		break;
	}
}
void	OTAEncodePro(void)
{
	u16 SendIndex = 0,Crc = 0,i = 0;
	if (!nUart5Sending)	
	{
		if (Send_Count >= 150)//实测从主控回复完成到模块下发新一帧完毕需要约365ms
		{
			Modbus_Buffer[SendIndex++] = OTA_HEAD;
			Modbus_Buffer[SendIndex++] = OTA_HOST;
			Modbus_Buffer[SendIndex++] = OTA_SLAVE;
			if(OTAStartSendTimes < 3)
			{
				OTA_ModbusSendStep = SendStart;
			}
			else if(OTADataUpdate)
			{
				OTA_ModbusSendStep = SendData;
			}
			else
			{
				if(OTAStartSendTimes == 3)
				{
					OTAStartReply = TRUE;
					OTAStartSendTimes += 1;
				}
				return;
			}
			switch(OTA_ModbusSendStep)
			{
				case	SendStart:
					OTAStartSendTimes += 1;
					Modbus_Buffer[SendIndex++] = 0x91;
					Modbus_Buffer[SendIndex++] = 0x13;
					Modbus_Buffer[SendIndex++] = 0x11;
					Modbus_Buffer[SendIndex++] = 0x11;
					Modbus_Buffer[SendIndex++] = 0x00;
					Modbus_Buffer[SendIndex++] = 0x11;
					Modbus_Buffer[SendIndex++] = (u8)(OTAlength>>24);
					Modbus_Buffer[SendIndex++] = (u8)(OTAlength>>16);
					Modbus_Buffer[SendIndex++] = (u8)(OTAlength>>8);
					Modbus_Buffer[SendIndex++] = (u8)OTAlength;
					Modbus_Buffer[SendIndex++] = (u8)(OTAlength>>16);
					Modbus_Buffer[SendIndex++] = (u8)(OTAlength>>8);
					Modbus_Buffer[SendIndex++] = 0x55;
					Modbus_Buffer[SendIndex++] = 0xaa;
				break;
				case	SendData:
					Modbus_Buffer[SendIndex++] = 0x96;
					Modbus_Buffer[SendIndex++] = 137;//(256/2)+2+7
					Modbus_Buffer[SendIndex++] = (u8)(PackageNum>>8);
					Modbus_Buffer[SendIndex++] = (u8)PackageNum;
					for(i=0;i<256;i++)
					{
						Modbus_Buffer[SendIndex++] = OTAdataindex[i];
					}
				break;
			}
			Crc = CalculateModbusCrc(Modbus_Buffer+1,SendIndex-1);
			Modbus_Buffer[SendIndex++] = (unsigned char)Crc;
			Modbus_Buffer[SendIndex++] = (unsigned char)(Crc>>8);
			UART5_SendStr(Modbus_Buffer,SendIndex);
			OTA_ModbusStep = OTARecv;
		}
	}
}
void	OTARecvPro(void)
{
	if((modbus_res_finish)||(OTA_ModbusSendStep == SendStart))
	{
		modbus_res_finish = FALSE;
		OTAModbusRecvLenth = Uart5_Rx[4];
		OTA_ModbusStep = OTADecode;
	}
}
void	OTADecodePro(void)
{
	xdata u16 Crc = 0;
	Crc = CalculateModbusCrc((Uart5_Rx+1),OTAModbusRecvLenth-3);
	if((Uart5_Rx[OTAModbusRecvLenth-2] != (unsigned char)Crc)&&(Uart5_Rx[OTAModbusRecvLenth-1] != (unsigned char)(Crc>>8)))
	{
		OTAModbusRecvLenth = 0;
		OTA_ModbusStep = OTAEncode;
		return;
	}
	if(((u8)(PackageNum>>8) == Uart5_Rx[5])&&((u8)PackageNum == Uart5_Rx[6])&&(!Uart5_Rx[7])&&(!Uart5_Rx[8]))
	{
		OTAReply = TRUE;
		OTADataUpdate = FALSE;
	}
	OTAModbusRecvLenth = 0;
	OTA_ModbusStep = OTAEncode;
}
