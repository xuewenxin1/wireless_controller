
#include "ModbusPro1.h"
#include "uart.h"
#include "modbus.h"
#include "WireCtr.h"


#define  nModBus_NUM 1

#define  HEAD_CODE				0x7E
#define  HOST_ADD				 0xF1
#define  SLAVE_ADD				 0x01
#define  FUNCTION_CODE		0x13   //20210309功能码	

typedef enum{
	ModbusEncodeData,   //数据打包 F
	ModbusRecvData,     //数据接收  F
	ModbusDecodeData     //数据解析F
}ModbusStepEnum;

/* MODBUS通信错误检测F */ 
#define  MODBUS_COMMNI_TIMER  60000//120000
#define  MODBUS_COMMNI_TIMER_SC  7000


/* 数据接收与发送、解析与打包  */

#define MODBUS_RECBUF_SIZE   169//140 //110    //数据缓存F
unsigned char  ODUsendOrRecvDataBuf[MODBUS_RECBUF_SIZE];// = {0};	// 室外机发送接收数据 F

//设定参数上、下限 缓存F
#define  SETPARA_UPLIMIT_SIZE 100
#define  SETPARA_DOWNLIMIT_SIZE 100
signed short  SetParaUpLimitBuf[SETPARA_UPLIMIT_SIZE];
signed short SetParaDownLimitBuf[SETPARA_DOWNLIMIT_SIZE];



unsigned char SetParaChange = 1;
#define USER_SET_PARA_ADD_STA 600                                            /*用户设置参数起始地址*/
#define USER_PARA_QUERY_LEN     12 //21                                           /*用户设置参数数据长度*/
#define USER_SET_PARA_LEN       12 //7//3                                            /*参数设置长度*/
#define USER_SET_PARA_ADD_STA 600                                            /*用户设置参数起始地址*/
#define USER_SET_PARA_ADD_END (USER_SET_PARA_ADD_STA+USER_PARA_QUERY_LEN-1)  /*用户设置参数终止地址*/    

#define SET_PARA_LEN  80//64 //51//42                                            /*设置参数数据长度*/
#define SET_PARA_ADD_STA 800                                        /*设置参数起始地址*/
#define SET_PARA_ADD_END (SET_PARA_ADD_STA+SET_PARA_LEN-1)          /*设置参数终止地址*/






//接收数据状态  F
typedef enum{
	DataOk = 0,   //接收OK  
	IDError,		//ID错误  F
	LenError,     //数据长度错误F
	CRCError   //校验和错误F
}ModbusDataStusEnum;
// 接收结构F 
typedef enum{
	RecvIdle,   //空闲F
	RecvHead,	//头码F
	RecvId,		//ID  F
	RecvData	//数据F	
}ModbusRecvStepEnum;

// modbus数据结构F//
typedef struct{
	unsigned char nHeadCode;		//头码F
	unsigned char nDeviceAddr;		// 从机地址 F
	unsigned char nHostAddress;	// 主机地址 F
	unsigned char nReceiveID;		// 功能码F
	unsigned char nSendID;			// 功能码F
	unsigned char nDistributMode;	// 发布模式 F
	unsigned char nAnswerStatus;	// 外机应答状态 F
	unsigned char nDestHighAddr;	// 目标寄存器高地址F
	unsigned char nDestLowAddr;	// 目标寄存器低地址F
	unsigned char nDataLen;		// 数据长度F
	unsigned char nRecvCompleteVal;// 是否接受完成 F
	unsigned char nRecStartAddr;	// 接收数据起始地址 F

	unsigned char nHighAddr;		// 寄存器高地址F
	unsigned char nLowAddr;		// 寄存器低地址F
	unsigned char nHighCoilCnt;	// 线圈个数高字节F
	unsigned char nLowCoilCnt;		// 线圈个数低字节F
	unsigned char nHighRegCnt;		// 寄存器个数高字节F
	unsigned char nLowRegCnt;		// 寄存器个数低字节F
	unsigned char nHighData;		// 数据高字节F
	unsigned char nLowData;		// 数据低字节F
	unsigned char bComError;		// MODBUS通信是否故障F
	unsigned long int nComTim;		// 持续时间计数F
	unsigned short int nDlyCnt;		// 重发延时 F
	unsigned char nIndex;			// 接收 F
	unsigned char bComErrJudgeFlg;	// 通信故障判断标志F
	unsigned char *RecvBuf; //[MODBUS_RECBUF_SIZE];
	unsigned char *SendBuf; //[MODBUS_SENDBUF_SIZE];		// 发送缓存F
	ModbusDataStusEnum eRecvStatus;
	ModbusRecvStepEnum eRecvStep;
	ModbusStepEnum eModbusStep;

}ModbusDataStruct;

ModbusDataStruct strModbusData[nModBus_NUM]={0};

unsigned char n10CodeSendCnt_add600;		//	10指令发送次数F
unsigned char n10CodeSendCnt_add800;		//	10指令发送次数F

unsigned short uParaRefreshCnt=0; //参数刷新延时计数器F


unsigned char needchangeEnableFlag=0;
unsigned char sendEnableFlag=0;
unsigned char SetDataSendFlag=FALSE;			//地址600开始的参数修改F

void ModbusDataEncode(unsigned char nCOM_NUM); // 打包 F
void ModbusDataRecv(unsigned char nCOM_NUM); // 首解 F
void ModbusDataDecode(unsigned char nCOM_NUM); // 解码 F

unsigned char FirstDataFlag=FALSE,SecondDataFlag=FALSE,ThirdDataFlag=FALSE;  //上限查询标志F
unsigned char FirstDataFlag1=FALSE,SecondDataFlag1=FALSE,ThirdDataFlag1=FALSE;  //下限查询标志F
//数据解析与打包  F//
unsigned char sendflag;
unsigned short nPowerOnDelayCnt=0;   //上电延时F

unsigned short CalculateRegAddr(unsigned char nHighAddr, unsigned char nLowAddr);
unsigned short CalculateModbusCrc(unsigned char *pBuf, unsigned char nLen);

unsigned char   GetComErr(unsigned char num);   //线控器与主控的通讯故障F

/* MODBUS协议任务及初始化*/
void ModbusProInit(void)
{
	unsigned char i;
//keng	UartInit9600();

	for (i=0;i<MODBUS_RECBUF_SIZE;i++)
	{
		ODUsendOrRecvDataBuf[i] = 0;
	}

	for(i=0;i<nModBus_NUM;i++)
	{
		strModbusData[i].nHostAddress = 0xF1;	// 主机地址 F
		strModbusData[i].nReceiveID = 0;		// 功能码F
		strModbusData[i].nSendID = 0x03;			// 功能码F
		strModbusData[i].nDistributMode = 0;	// 发布模式 F
		strModbusData[i].nAnswerStatus = 0;	// 外机应答状态 F
		strModbusData[i].nDestHighAddr = 0;	// 目标寄存器高地址F
		strModbusData[i].nDestLowAddr = 0;	// 目标寄存器低地址F
		strModbusData[i].nDataLen = 0;		// 数据长度F
		strModbusData[i].nRecvCompleteVal = 0;// 是否接受完成 F
		strModbusData[i].nRecStartAddr = 0;	// 接收数据起始地址 F

		strModbusData[i].nDeviceAddr = 0x01;
		strModbusData[i].nReceiveID = 0;
		strModbusData[i].nHighAddr = 0;
		strModbusData[i].nLowAddr = 0;
		strModbusData[i].nHighCoilCnt = 0;
		strModbusData[i].nLowCoilCnt = 0;
		strModbusData[i].nHighRegCnt = 0;
		strModbusData[i].nLowRegCnt = 0;
		strModbusData[i].nHighData = 0;
		strModbusData[i].nLowData = 0;
		strModbusData[i].nDlyCnt = 0;
		strModbusData[i].eRecvStatus = DataOk;
		strModbusData[i].nComTim = 0;	// 持续时间计数F
		strModbusData[i].bComError = 0;	// MODBUS通信是否故障F
		strModbusData[i].bComErrJudgeFlg = 0;	// 通信故障判断标志F
		strModbusData[i].eRecvStatus = 0;
		strModbusData[i].eRecvStep = 0;
		strModbusData[i].RecvBuf= ODUsendOrRecvDataBuf;
		strModbusData[i].SendBuf =ODUsendOrRecvDataBuf;
		strModbusData[i].eModbusStep = ModbusEncodeData;
	}

	for (i=0;i<SETPARA_UPLIMIT_SIZE;i++)
	{
		SetParaUpLimitBuf[i]=0;
	}
	
	for (i=0;i<SETPARA_DOWNLIMIT_SIZE;i++)
	{
		SetParaDownLimitBuf[i]=0;
	}
}
void ModbusProTimer(void)
{
	unsigned char i;
//keng	IncUart0SendIde();
	for(i=0;i<nModBus_NUM;i++)
	{
		switch(strModbusData[i].eModbusStep)
		{
			case ModbusEncodeData:				//数据打包F
				ModbusDataEncode(i);			//打包发送F
				strModbusData[i].bComErrJudgeFlg = 1;	
			break;

			case ModbusRecvData:				//接收数据F
			 	ModbusDataRecv(i);    			//接收数据首解码F 
			break;

			case ModbusDecodeData:				//数据解析F
				ModbusDataDecode(i);			 //解析F
				if (strModbusData[i].eRecvStatus != DataOk)
    			{
    				strModbusData[i].nDataLen = 0;
    				strModbusData[i].nDlyCnt = 0;
    				strModbusData[i].eRecvStep = RecvIdle;
    				strModbusData[i].eModbusStep = ModbusEncodeData;	
    				strModbusData[i].nRecvCompleteVal = EERCODE;// 是否接受完成 F
    			}
    			else
    			{
    				strModbusData[i].bComErrJudgeFlg = 0;
    			}
			break;
		}
	}

	for(i=0;i<nModBus_NUM;i++)
	{
		//通讯故障检测F
		if(strModbusData[i].bComErrJudgeFlg)
		{
			if(strModbusData[i].nComTim >= MODBUS_COMMNI_TIMER)
			{
				strModbusData[i].nComTim = 0;
				strModbusData[i].bComError = 1;
			}
			else
			{
				strModbusData[i].nComTim++;
			}
		}
		else
		{
			strModbusData[i].nComTim = 0;				// 故障持续时间清零F
			strModbusData[i].bComError = 0;
		}	
	}
}

unsigned char  GetComErr(unsigned char num)	//线控器与主控的通讯故障F
{
	return strModbusData[num].bComError;
}



void ModbusDataEncode(unsigned char nCOM_NUM)
{
	unsigned short nCrc = 0,i=0;
	unsigned char nSendIndex = 0;
	unsigned char nDataLen=0,nDataLen1=0,nDataLen2=0;
// 	static xdata  unsigned short nDelayCnt=0;

	//kengif (!getUart0SendStatus())	
	//keng{
	//keng	if (getUart0SendIde()>=150)
		//keng{
		//keng	if (LineControl == nCOM_NUM)
		//keng	{
				nSendIndex = 0;
				strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = HEAD_CODE;
				strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = SLAVE_ADD;    // 从机地址F
				strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = HOST_ADD;     // 主机地址F
				if (nPowerOnDelayCnt >= 15) //上电延时1.5S  F
				{
//					switch(strModbusData[nCOM_NUM].nSendID)
//					{
//						case 0x03:
//						//keng	if (CheckParaQuery || SetParaQuery)
//						//keng	{
//								strModbusData[nCOM_NUM].nSendID = 0x03;
//						//keng	} 
//						//keng	else
//						//keng	{
//						//keng		strModbusData[nCOM_NUM].nSendID = 0x10;
//						//keng	}
//							
//							break;
//						case 0x10:

//							//kengif ((!SetParaChange) &&(!SetDataChangeFlag))
//							//keng{
//								strModbusData[nCOM_NUM].nSendID = 0x03;
//							//keng}
//			
//							break;
//						default:
//							strModbusData[nCOM_NUM].nSendID = 0x03;
//							break;
//					}
				}
				

				sendflag = 1;
				
        		switch(strModbusData[nCOM_NUM].nSendID)
        		{
        			case 0x10:		// 设置预置多个寄存器 F 
		
        			   
                       if(SetParaChange)
                       {
												strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = strModbusData[nCOM_NUM].nSendID;    // 功能码F
												strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = (u8)((SET_PARA_ADD_STA+SetParaAdrrDlt)/256);	// 寄存器起始高地址F 
												strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = (u8)((SET_PARA_ADD_STA+SetParaAdrrDlt)%256);    // 寄存器起始低地址F  
												strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = 1;                  // 寄存器个数F
												strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = GET_U16_HIGH_BYTE(SetPara.Buf[SetParaAdrrDlt]);
												strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = GET_U16_LOW_BYTE(SetPara.Buf[SetParaAdrrDlt]);
                        }
												else//(SetDataChangeFlag)
												{
													UserSetDataUpdate();
													
													strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = strModbusData[nCOM_NUM].nSendID;    // 功能码F
													strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = (u8)(USER_SET_PARA_ADD_STA/256);	// 寄存器起始高地址F 
													strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = (u8)(USER_SET_PARA_ADD_STA%256);    // 寄存器起始低地址F  
													strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = (u8)(USER_SET_PARA_LEN-5);          // 寄存器个数F
													strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = GET_U16_HIGH_BYTE(unUserSetPara.member.SetMode);  // 开关机与模式F
													strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = GET_U16_LOW_BYTE(unUserSetPara.member.SetMode);
													strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = GET_U16_HIGH_BYTE(unUserSetPara.member.SetTempHeat);   // 制热设定温度F
													strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = GET_U16_LOW_BYTE(unUserSetPara.member.SetTempHeat); 
													strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = GET_U16_HIGH_BYTE(unUserSetPara.member.SetTempCool);    //制冷设定温度 F
													strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = GET_U16_LOW_BYTE(unUserSetPara.member.SetTempCool);
													strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = GET_U16_HIGH_BYTE(unUserSetPara.member.SetTempHumidity);    // 热水设定温度F
													strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = GET_U16_LOW_BYTE(unUserSetPara.member.SetTempHumidity);
													strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = GET_U16_HIGH_BYTE(unUserSetPara.member.RoomTemp);    // 线控器环境温度F
													strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = GET_U16_LOW_BYTE(unUserSetPara.member.RoomTemp);
													strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = GET_U16_HIGH_BYTE(unUserSetPara.member.AuxiliaryFunction);    // 辅助功能F
													strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = GET_U16_LOW_BYTE(unUserSetPara.member.AuxiliaryFunction);
													strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = GET_U16_HIGH_BYTE(unUserSetPara.member.SystemStatus);    // 系统状态F
													strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = GET_U16_LOW_BYTE(unUserSetPara.member.SystemStatus);
													//补充607~611数据F
						// 							strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = 0;    // 故障代码F
						// 							strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = 0;
						// 							strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = 0;    // 水箱温度F
						// 							strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = 0;
						// 							strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = 0;    // 进水温度F
						// 							strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = 0;
						// 							strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = 0;    // 出水温度F
						// 							strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = 0;
						// 							strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = GET_U16_HIGH_BYTE(unUserSetPara.member.Wifi);    // wifi
						// 							strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = GET_U16_LOW_BYTE(unUserSetPara.member.Wifi);
													
												}
												break;


        		    case 0x03:		// 读保持寄存器F
        		    default:

											if (!UnWireCtrInitDataOk)
											{
												strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = strModbusData[nCOM_NUM].nSendID;	// 功能码F
												strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = (u8)(INTIAL_DATA_ADD_STA/256);	// 寄存器起始高地址F 
												strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = (u8)(INTIAL_DATA_ADD_STA%256);    // 寄存器起始低地址F  
												strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = INTIAL_DATA_LEN;                // 寄存器个数F 
											}
											else if (SetParaUpLimit)
											{
												nDataLen = unWireCtlrInitData.Buf[14];
												nDataLen1 =  nDataLen/32;   //判断是否大于32,每次读32个数据F
												nDataLen2 =  nDataLen%32;   //读取的数据长度余数是多少F
												if (FirstDataFlag==TRUE)
												{
													strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = strModbusData[nCOM_NUM].nSendID;	// 功能码F
													strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = (u8)(SET_PARA_MAX_ADD_STA/256);	// 寄存器起始高地址F 
													strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = (u8)(SET_PARA_MAX_ADD_STA%256);    // 寄存器起始低地址F  	
													if (nDataLen1>0)
													{
														strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = 0x20;                // 寄存器个数F 
													}
													else
													{
														strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = nDataLen;                // 寄存器个数F 
													}
												}
												
												if ((nDataLen1>=1)&&(SecondDataFlag==TRUE))//接收正确F
												{
													strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = strModbusData[nCOM_NUM].nSendID;	// 功能码F
													strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = (u8)((SET_PARA_MAX_ADD_STA+32)/256);	// 寄存器起始高地址F 
													strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = (u8)((SET_PARA_MAX_ADD_STA+32)%256);    // 寄存器起始低地址F  
													if(nDataLen1>1)
													{
														strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = 0x20;                // 寄存器个数F 
													}
													else
													{
														strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = nDataLen2;                // 寄存器个数F 
													}	
												}

												if ((nDataLen1>=2)&&(ThirdDataFlag==TRUE)) //接收正确
												{
													strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = strModbusData[nCOM_NUM].nSendID;	// 功能码F
													strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = (u8)((SET_PARA_MAX_ADD_STA+64)/256);	// 寄存器起始高地址F 
													strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = (u8)((SET_PARA_MAX_ADD_STA+64)%256);    // 寄存器起始低地址F  
													strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = nDataLen2;                // 寄存器个数F 
												}
											}
											else if (SetParaDownLimit)
											{
												nDataLen = unWireCtlrInitData.Buf[14];
												nDataLen1 =  nDataLen/32;
												nDataLen2 =  nDataLen%32;
												if (FirstDataFlag1==TRUE)
												{
													strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = strModbusData[nCOM_NUM].nSendID;	// 功能码F
													strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = (u8)(SET_PARA_MIN_ADD_STA/256);	// 寄存器起始高地址F 
													strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = (u8)(SET_PARA_MIN_ADD_STA%256);    // 寄存器起始低地址F  	
													if (nDataLen1>0)
													{
														strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = 0x20;                // 寄存器个数F 
													}
													else{
														strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = nDataLen;                // 寄存器个数F 
													}
												}
												
												if ((nDataLen1>=1)&&(SecondDataFlag1==TRUE))//接收正确F
												{
													strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = strModbusData[nCOM_NUM].nSendID;	// 功能码F
													strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = (u8)((SET_PARA_MIN_ADD_STA+32)/256);	// 寄存器起始高地址F 
													strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = (u8)((SET_PARA_MIN_ADD_STA+32)%256);    // 寄存器起始低地址F  
													if(nDataLen1>1)
													{
														strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = 0x20;                // 寄存器个数F 
													}
													else
													{
														strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = nDataLen2;                // 寄存器个数F 
													}	
												}
												
												if ((nDataLen1>=2)&&(ThirdDataFlag1==TRUE)) //接收正确
												{
													strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = strModbusData[nCOM_NUM].nSendID;	// 功能码F
													strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = (u8)((SET_PARA_MIN_ADD_STA+64)/256);	// 寄存器起始高地址F 
													strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = (u8)((SET_PARA_MIN_ADD_STA+64)%256);    // 寄存器起始低地址F  
													strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = nDataLen2;                // 寄存器个数F 
												}
											}
											else if (CheckParaQuery)
											{
												strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = strModbusData[nCOM_NUM].nSendID;	// 功能码F
												strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = (u8)(QUERY_PARA_ADD_STA/256);     	// 寄存器起始高地址F 
												strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = (u8)(QUERY_PARA_ADD_STA%256);         // 寄存器起始低地址F  
															strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = QUERY_PARA_NUM;                       // 寄存器个数F 
											}
											else if(SetParaQuery)
											{
												strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = strModbusData[nCOM_NUM].nSendID;	// 功能码F
												strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = (u8)(SET_PARA_ADD_STA/256);     	// 寄存器起始高地址F 
												strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = (u8)(SET_PARA_ADD_STA%256);         // 寄存器起始低地址F  
												strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = SET_PARA_LEN;                       // 寄存器个数F 
											}
											else
											{
												strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = strModbusData[nCOM_NUM].nSendID;	// 功能码F
												strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = (u8)(USER_SET_PARA_ADD_STA/256);	// 寄存器起始高地址F 
												strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = (u8)(USER_SET_PARA_ADD_STA%256);    // 寄存器起始低地址F  
												strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = USER_PARA_QUERY_LEN;                // 寄存器个数F 	

												if (!sendEnableFlag)
												{
													sendflag = 0;
												}
											}	
													break;
        		}
				
				nCrc = CalculateModbusCrc(strModbusData[nCOM_NUM].SendBuf, nSendIndex);
				strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = GET_U16_LOW_BYTE(nCrc);	// 校验码低字节F
				strModbusData[nCOM_NUM].SendBuf[nSendIndex++] = GET_U16_HIGH_BYTE(nCrc);		// 校验码高字节F
				
				if (sendflag)
				{
					//keng   Uart0SendData(strModbusData[nCOM_NUM].SendBuf, nSendIndex);
					UART5_SendStr(strModbusData[nCOM_NUM].SendBuf, nSendIndex);
					strModbusData[nCOM_NUM].eModbusStep = ModbusRecvData;
				}
			}

	//keng	}
//keng	}

//keng}




//接收码首解  F//
unsigned char RecErrCnt=0;
void ModbusDataRecv(unsigned char nCOM_NUM)
{
	if (LineControl == nCOM_NUM)
	{
	//keng	strModbusData[nCOM_NUM].nIndex = GetUart0RecvIndex();
        
		switch (strModbusData[nCOM_NUM].eRecvStep)
		{
			case RecvIdle:
			//kengif (!getUart0SendStatus())
			//keng{
				strModbusData[nCOM_NUM].nDlyCnt = 0;
				strModbusData[nCOM_NUM].nDataLen = 0;
                
				Uart0RecvData(strModbusData[nCOM_NUM].RecvBuf, MODBUS_RECBUF_SIZE);
                
				strModbusData[nCOM_NUM].eRecvStep = RecvHead;
				//RecErrCnt = 0;
		//keng	}
			break;
		case RecvHead:
			//keng if (strModbusData[nCOM_NUM].nIndex < 3)
			//keng { 
			//keng	if (++strModbusData[nCOM_NUM].nDlyCnt >= 1000)
			//keng	{
			//keng		strModbusData[nCOM_NUM].nDlyCnt = 0;
			//keng		strModbusData[nCOM_NUM].eRecvStep = RecvIdle;
			//keng		strModbusData[nCOM_NUM].eModbusStep = ModbusEncodeData;
					
			//keng		strModbusData[nCOM_NUM].nRecvCompleteVal = EERCODE;// 是否接受完成 F
			//keng		RecErrCnt++;
			//keng		sendEnableFlag = 0;
			//keng	}
			//keng}
			//kengelse
			//keng{
				if (strModbusData[nCOM_NUM].RecvBuf[0]==HEAD_CODE && strModbusData[nCOM_NUM].RecvBuf[1]==HOST_ADD && strModbusData[nCOM_NUM].RecvBuf[2]==SLAVE_ADD)   // 地址F
				{
					strModbusData[nCOM_NUM].eRecvStep = RecvId;
					
					strModbusData[nCOM_NUM].nDlyCnt = 0;
					//RecErrCnt = 0;
				}
				else
				{	
					strModbusData[nCOM_NUM].eRecvStep = RecvIdle;

					strModbusData[nCOM_NUM].nDlyCnt = 0;
				}
			//keng}
			
			break;
			
		case RecvId:
			//kengif (strModbusData[nCOM_NUM].nIndex > 6)
			//keng{
				if (strModbusData[nCOM_NUM].RecvBuf[3]==  FUNCTION_CODE)
				{
					strModbusData[nCOM_NUM].nDataLen = (u8)(strModbusData[nCOM_NUM].RecvBuf[6]*2 + 9);
					strModbusData[nCOM_NUM].eRecvStep = RecvData;
					strModbusData[nCOM_NUM].nDlyCnt = 0;
				}
				else
				{
					strModbusData[nCOM_NUM].eRecvStep = RecvIdle;

					strModbusData[nCOM_NUM].nDlyCnt = 0;
				}
			//keng}
			
			if (++strModbusData[nCOM_NUM].nDlyCnt >= 1000)
			{
				strModbusData[nCOM_NUM].nDlyCnt = 0;
				strModbusData[nCOM_NUM].eRecvStep = RecvIdle;
				strModbusData[nCOM_NUM].eModbusStep = ModbusEncodeData;
				
				strModbusData[nCOM_NUM].nRecvCompleteVal = EERCODE;// 是否接受完成 F
				sendEnableFlag = 0;
			}
			
			break;
		case RecvData:
			if (++strModbusData[nCOM_NUM].nDlyCnt >= 1000)
			{
				strModbusData[nCOM_NUM].nDlyCnt = 0;
				strModbusData[nCOM_NUM].eRecvStep = RecvIdle;
				strModbusData[nCOM_NUM].eModbusStep = ModbusEncodeData;
				
				strModbusData[nCOM_NUM].nRecvCompleteVal = EERCODE;// 是否接受完成 F
				sendEnableFlag = 0;
			}
			
			if (strModbusData[nCOM_NUM].nIndex >= strModbusData[nCOM_NUM].nDataLen)
			{
				strModbusData[nCOM_NUM].nDlyCnt = 0;
				strModbusData[nCOM_NUM].eRecvStep = RecvIdle;
				strModbusData[nCOM_NUM].eModbusStep = ModbusDecodeData;
				
				return;
			}
			break;
			
		default:
			
			if (++strModbusData[nCOM_NUM].nDlyCnt >= 1000)
			{
				strModbusData[nCOM_NUM].nDlyCnt = 0;
				strModbusData[nCOM_NUM].eRecvStep = RecvIdle;
				strModbusData[nCOM_NUM].eModbusStep = ModbusEncodeData;
				strModbusData[nCOM_NUM].nRecvCompleteVal = EERCODE;// 是否接受完成 F
				sendEnableFlag = 0;
			}
			break;
		}
	}
}	


//数据解码F   //
void ModbusDataDecode(unsigned char nCOM_NUM)	
{
	unsigned short j = 0;
	unsigned short i = 0;
	unsigned short nRegAddr = 0;
	unsigned short nDataCnt = 0;
	unsigned short nCrc = 0;
    
	if (LineControl == nCOM_NUM)
	{
		strModbusData[nCOM_NUM].nReceiveID = strModbusData[nCOM_NUM].RecvBuf[3];

		if (strModbusData[nCOM_NUM].nReceiveID != 0x13)
		{
			strModbusData[nCOM_NUM].eRecvStatus = IDError;
			return;
		}
        
        strModbusData[nCOM_NUM].nDataLen = strModbusData[nCOM_NUM].RecvBuf[6]*2 + 9; 
        
		// 接收校验F
		nCrc = CalculateModbusCrc(strModbusData[nCOM_NUM].RecvBuf, (u8)(strModbusData[nCOM_NUM].nDataLen-2));
		if ((strModbusData[nCOM_NUM].RecvBuf[strModbusData[nCOM_NUM].nDataLen-2]!=GET_U16_LOW_BYTE(nCrc)) || (strModbusData[nCOM_NUM].RecvBuf[strModbusData[nCOM_NUM].nDataLen-1]!=GET_U16_HIGH_BYTE(nCrc)))
		{
			strModbusData[nCOM_NUM].eRecvStatus = CRCError;
            
			return;
		}
        
		if (strModbusData[nCOM_NUM].RecvBuf[6] != strModbusData[nCOM_NUM].SendBuf[6])
		{
			strModbusData[nCOM_NUM].eRecvStatus = LenError;
            
			return;
		}
		
		RecErrCnt = 0;

        nDataCnt = strModbusData[nCOM_NUM].RecvBuf[6];	// 数据长度F
		nRegAddr = CalculateRegAddr(strModbusData[nCOM_NUM].RecvBuf[4],strModbusData[nCOM_NUM].RecvBuf[5]);
		
		switch(strModbusData[nCOM_NUM].nSendID)
        {
		case 0x03:
			if(nRegAddr>=INTIAL_DATA_ADD_STA && nRegAddr<=INTIAL_DATA_ADD_END)
			{
				for(i=0,j=0;j<nDataCnt;j++)
				{
					unWireCtlrInitData.Buf[j+(nRegAddr-INTIAL_DATA_ADD_STA)] = ((signed short)strModbusData[nCOM_NUM].RecvBuf[7+(i++)]<<8) + (signed short)strModbusData[nCOM_NUM].RecvBuf[7+(i++)];
				}	
				UnWireCtrInitDataOk = TRUE;
// 				SetParaUpLimit = TRUE;
// 				if (getonOffV())
// 				{
// 					SetDataSendFlag = TRUE;
// 					SetDataChangeFlag = TRUE;  //写开关机制冷给线控器F
// 				}
				if (unWireCtlrInitData.Buf[14]>0)
				{
					SetParaUpLimit = TRUE;
					FirstDataFlag =TRUE;
				}
				
			}
//上限查询//			
			if(nRegAddr>=SET_PARA_MAX_ADD_STA && nRegAddr<=(SET_PARA_MAX_ADD_STA+31))//SET_PARA_MAX_ADD_END)
			{
				for(i=0,j=0;j<nDataCnt;j++)
				{
					SetParaUpLimitBuf[j+(nRegAddr-SET_PARA_MAX_ADD_STA)] = ((signed short)strModbusData[nCOM_NUM].RecvBuf[7+(i++)]<<8) + (signed short)strModbusData[nCOM_NUM].RecvBuf[7+(i++)];
				}
				FirstDataFlag =FALSE;
				if ((unWireCtlrInitData.Buf[14]/32)>=1)
				{
					SecondDataFlag = TRUE;
				}
				else{
					SetParaUpLimit = FALSE;
					if (unWireCtlrInitData.Buf[14]>0)
					{
						SetParaDownLimit = TRUE;
						FirstDataFlag1 = TRUE;
					}
				}
			}
			if(nRegAddr>=SET_PARA_MAX_ADD_STA+32 && nRegAddr<=(SET_PARA_MAX_ADD_STA+63))//SET_PARA_MAX_ADD_END)
			{
				for(i=0,j=32;j<(nDataCnt+32);j++)
				{
					SetParaUpLimitBuf[j+(nRegAddr-SET_PARA_MAX_ADD_STA-32)] = ((signed short)strModbusData[nCOM_NUM].RecvBuf[7+(i++)]<<8) + (signed short)strModbusData[nCOM_NUM].RecvBuf[7+(i++)];
				}
				SecondDataFlag = FALSE;
				if ((unWireCtlrInitData.Buf[14]/32)>=2)
				{
					ThirdDataFlag = TRUE;
				}
				else
				{
					SetParaUpLimit = FALSE;
					//SetParaDownLimit = TRUE;
					
					SetParaDownLimit = TRUE;
					FirstDataFlag1 = TRUE;
				
				}
			}
			if(nRegAddr>=SET_PARA_MAX_ADD_STA+64 && nRegAddr<=(SET_PARA_MAX_ADD_END))//SET_PARA_MAX_ADD_END)
			{
				for(i=0,j=64;j<(nDataCnt+64);j++)
				{
					SetParaUpLimitBuf[j+(nRegAddr-SET_PARA_MAX_ADD_STA-64)] = ((signed short)strModbusData[nCOM_NUM].RecvBuf[7+(i++)]<<8) + (signed short)strModbusData[nCOM_NUM].RecvBuf[7+(i++)];
				}
				ThirdDataFlag = FALSE;
				SetParaUpLimit = FALSE;		
				SetParaDownLimit = TRUE;	
				FirstDataFlag1 = TRUE;
			}
//下限查询//
			if(nRegAddr>=SET_PARA_MIN_ADD_STA && nRegAddr<=(SET_PARA_MIN_ADD_STA+31))//SET_PARA_MAX_ADD_END)
			{
				for(i=0,j=0;j<nDataCnt;j++)
				{
					SetParaDownLimitBuf[j+(nRegAddr-SET_PARA_MIN_ADD_STA)] = ((signed short)strModbusData[nCOM_NUM].RecvBuf[7+(i++)]<<8) + (signed short)strModbusData[nCOM_NUM].RecvBuf[7+(i++)];
				}
				FirstDataFlag1 =FALSE;
				if ((unWireCtlrInitData.Buf[14]/32)>=1)
				{
					SecondDataFlag1 = TRUE;
				}
				else{
					
					SetParaDownLimit = FALSE;
				}
			}
			if(nRegAddr>=SET_PARA_MIN_ADD_STA+32 && nRegAddr<=(SET_PARA_MIN_ADD_STA+63))//SET_PARA_MAX_ADD_END)
			{
				for(i=0,j=32;j<(nDataCnt+32);j++)
				{
					SetParaDownLimitBuf[j+(nRegAddr-SET_PARA_MIN_ADD_STA-32)] = ((signed short)strModbusData[nCOM_NUM].RecvBuf[7+(i++)]<<8) + (signed short)strModbusData[nCOM_NUM].RecvBuf[7+(i++)];
				}
				SecondDataFlag1 = FALSE;
				if ((unWireCtlrInitData.Buf[14]/32)>=2)
				{
					ThirdDataFlag1 = TRUE;
				}
				else{
					//SetParaUpLimit = FALSE;
					SetParaDownLimit = FALSE;
				}
			}
			if(nRegAddr>=SET_PARA_MIN_ADD_STA+64 && nRegAddr<=(SET_PARA_MIN_ADD_END))//SET_PARA_MAX_ADD_END)
			{
				for(i=0,j=64;j<(nDataCnt+64);j++)
				{
					SetParaDownLimitBuf[j+(nRegAddr-SET_PARA_MIN_ADD_STA-64)] = ((signed short)strModbusData[nCOM_NUM].RecvBuf[7+(i++)]<<8) + (signed short)strModbusData[nCOM_NUM].RecvBuf[7+(i++)];
				}
				ThirdDataFlag1 = FALSE;
				//SetParaUpLimit = FALSE;
				SetParaDownLimit = FALSE;
			}
		

			if(nRegAddr>=QUERY_PARA_ADD_STA && nRegAddr<=QUERY_PARA_ADD_END)
			{
				for(i=0,j=0;j<nDataCnt;j++)
				{
					CheckPara.buf[j+(nRegAddr-QUERY_PARA_ADD_STA)] = ((signed short)strModbusData[nCOM_NUM].RecvBuf[7+(i++)]<<8) + (signed short)strModbusData[nCOM_NUM].RecvBuf[7+(i++)];
				}
				CheckParaQuery = FALSE;
			}

			if(nRegAddr>=USER_SET_PARA_ADD_STA && nRegAddr<=USER_SET_PARA_ADD_END)
			{
 				if (nPowerOnDelayCnt >= 50)
				{
					for(i=0,j=0;j<nDataCnt;j++)
					{
						unUserSetPara.Buf[j+(nRegAddr-USER_SET_PARA_ADD_STA)] = ((signed short)strModbusData[nCOM_NUM].RecvBuf[7+(i++)]<<8) + (signed short)strModbusData[nCOM_NUM].RecvBuf[7+(i++)];
					}
				}
				
				if(!SetDataChangeFlag)
				{
					UserSetDataInitOK = TRUE;
					//UserSetDataRefresh();
				}

				sendEnableFlag = 0;
				needchangeEnableFlag = 1;
			}
			
			if(nRegAddr>=SET_PARA_ADD_STA && nRegAddr<=SET_PARA_ADD_END)
			{
				for(i=0,j=0;j<nDataCnt;j++)
				{
					SetPara.Buf[j+(nRegAddr-SET_PARA_ADD_STA)] = ((signed short)strModbusData[nCOM_NUM].RecvBuf[7+(i++)]<<8) + (signed short)strModbusData[nCOM_NUM].RecvBuf[7+(i++)];
				}
				
				SetParaQuery = FALSE;
			}
			break;
			
		case 0x10:
			if(nRegAddr>=USER_SET_PARA_ADD_STA && nRegAddr<=USER_SET_PARA_ADD_END)
			{
				 	if(n10CodeSendCnt_add600<255)
				 	{
				 		n10CodeSendCnt_add600++;
					}
 					if (n10CodeSendCnt_add600>=3)
					{
						if ((((unUserSetPara.member.SetTempCool)/10 )== getTempSetV()
							&&(((unUserSetPara.member.SetTempHeat)/10 )== getTempSetV())
							&&(((unUserSetPara.member.SetTempHumidity)/10 )== getTempSetV()))
							&&(((unUserSetPara.member.SetMode&0x000F))== getModeSet())
					        &&(((unUserSetPara.member.SetMode&0x0080)>>7)== getonOffV()))
						{
							n10CodeSendCnt_add600 = 0;
							SetDataSendFlag = FALSE;
							SetDataChangeFlag = FALSE;
						}
					}    
			}
			
			if(nRegAddr>=SET_PARA_ADD_STA && nRegAddr<=SET_PARA_ADD_END)
			{
// 				SetParaChange = FALSE;
				if(n10CodeSendCnt_add800<3)
				{
					n10CodeSendCnt_add800++;
				}
 				if (n10CodeSendCnt_add800>=3)
				{
					n10CodeSendCnt_add800 = 0;
					SetDataSendFlag = FALSE;
					SetParaChange = FALSE;
				}
			}
			needchangeEnableFlag = 0;
			break;
			
		default:
			break;
        }
//  		if(nRegAddr>=INTIAL_DATA_ADD_STA && nRegAddr<=INTIAL_DATA_ADD_END)
//  		{//*初始化数据查询*/
//  			for (i=nRegAddr-INTIAL_DATA_ADD_STA; i<(nRegAddr-INTIAL_DATA_ADD_STA+nDataCnt); i++)
//  			{
//  				;
//  			}
//  		}
//  		else if(nRegAddr>=SET_PARA_MAX_ADD_STA && nRegAddr<=SET_PARA_MAX_ADD_END)
//  		{//*设置参数上限查询*/
//  			for (i=nRegAddr-SET_PARA_MAX_ADD_STA; i<(nRegAddr-SET_PARA_MAX_ADD_STA+nDataCnt); i++)
//  			{
//  				;
//  			}
//  		}
//  		else if(nRegAddr>=SET_PARA_MIN_ADD_STA && nRegAddr<=SET_PARA_MIN_ADD_END)
//  		{ //*设置参数下限查询*/
//  			for (i=nRegAddr-SET_PARA_MIN_ADD_STA; i<(nRegAddr-SET_PARA_MIN_ADD_STA+nDataCnt); i++)
//  			{
//  				;
//  			}
//  		}
//  		else if(nRegAddr>=USER_SET_PARA_ADD_STA && nRegAddr<=USER_SET_PARA_ADD_END)
//  		{ //*线控器设置查询*/
//  			for(i=0,j=0;j<nDataCnt;j++)
//  			{
//  				unUserSetPara.Buf[j+(nRegAddr-USER_SET_PARA_ADD_STA)] = ((u16)strModbusData[nCOM_NUM].RecvBuf[7+(i++)]<<8) + (u16)strModbusData[nCOM_NUM].RecvBuf[7+(i++)];
//  			}
// 			if(!UserSetDataInitOK)
//  			{
//  				UserSetDataInitOK = TRUE;
//  				UserSetDataInit();
//  			}
//  		}
//  		else if(nRegAddr>=QUERY_PARA_ADD_STA && nRegAddr<=QUERY_PARA_ADD_END)
//  		{//*运行参数查询*/	
//  			for (i=nRegAddr-QUERY_PARA_ADD_STA; i<(nRegAddr-QUERY_PARA_ADD_STA+nDataCnt); i++)
//  			{
//  				;
//  			}
//  		}
//  		else if(nRegAddr>=SET_PARA_ADD_STA && nRegAddr<=SET_PARA_ADD_END)
//  		{ //*设置参数查询*/
//  			for(i=0,j=0;j<nDataCnt;j++)
//  			{
//  				SetPara.Buf[j+(nRegAddr-SET_PARA_ADD_STA)] = ((u16)strModbusData[nCOM_NUM].RecvBuf[7+(i++)]<<8) + (u16)strModbusData[nCOM_NUM].RecvBuf[7+(i++)];
//  			}
//  		}
	
		        
		strModbusData[nCOM_NUM].nDataLen = 0;
		strModbusData[nCOM_NUM].eRecvStatus = DataOk;
		strModbusData[nCOM_NUM].eModbusStep = ModbusEncodeData;
		strModbusData[nCOM_NUM].nRecvCompleteVal = strModbusData[nCOM_NUM].nReceiveID;// 是否接受完成 F
		strModbusData[nCOM_NUM].nReceiveID = 0;
	}	
}

unsigned short CalculateRegAddr(unsigned char nHighAddr, unsigned char nLowAddr)
{
	unsigned short nData = 0;
	
	nData = nHighAddr;
	nData <<= 8;
	nData |= nLowAddr;
	
	return nData;
}

unsigned short CalculateModbusCrc(unsigned char *pBuf, unsigned char nLen)
{
	u8 i, j;
	unsigned short nData;
	unsigned short nFlag;
	
	nData = 0xFFFF;
	for (i=0; i<nLen; i++)
	{
		nData = nData ^ pBuf[i];
		for (j=0; j<8; j++)
		{
			nFlag = nData & 0x01;
			nData = nData >> 1;
			nData = nData & 0x7fff;
			
			if (nFlag != 0)
			{
				nData = nData ^ 0xa001;
			}
		}
	}
	
	return nData;
}


void  BaudrateChangPro(void)
{
	if (RecErrCnt>5)
	{
		RecErrCnt = 0;
		if (0==Baudrate)
		{
			ModbusProInit();//(BAUDRATE1200);
		}
		else if (1==Baudrate)
		{
			ModbusProInit();//(BAUDRATE9600);
		}
		else
		{

		}	
	}
}















                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          