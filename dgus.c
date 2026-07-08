/******************************************************************************
* 文 件 名   : dgus.c
* 版 本 号   : V1.0
* 作    者   : pinot
* 生成日期   : 2019年11月03日
* 功能描述   : 读写DGUS变量存储器
* 修改历史   :
* 日    期   :
* 作    者   :
* 修改内容   :
******************************************************************************/

/*****************************************************************************
自定义头文件*/
#include "dgus.h"

/*****************************************************************************
读DGUS寄存器*/
/*****************************************************************************
 函 数 名  : ReadDGUS
 功能描述  : 读DGUS寄存器
 输入参数  : uint32_t Addr  DGUS寄存器地址
             uint8_t* pBuf  接收缓冲区
             uint16_t Len   读取数据字节长度(先高字再低字)
 输出参数  : 无
 修改历史  :
 日    期  : 2019年11月04日
 作    者  :
 修改内容  : 创建
*****************************************************************************/
void ReadDGUS(uint32_t Addr, uint8_t *pBuf, uint16_t Len)
{
  uint8_t Aoffset;
  if(NULL == pBuf){return;}
  if(0 == Len){return;}
  if((Addr+Len/2) > (0xFFFF*2)){return;}
  EA = 0;
  Aoffset = Addr&0x01;                 /*取bit0作为奇偶判断*/
  Addr  = Addr >> 1;                   /*配置地址*/
  ADR_H = (uint8_t)(Addr >> 16);
  ADR_M = (uint8_t)(Addr >> 8);
  ADR_L = (uint8_t)(Addr);
  RAMMODE = 0x00;
  ADR_INC = 0x01;
  APP_REQ = 1;                         /*占用DGUS读写*/
  while(!APP_ACK);
  APP_RW  = 1;                         /*读变量存储器*/

  if(1 == Aoffset)
  {                    /*地址为奇数*/
    if(1 == Len){ APP_DATA3=0,APP_DATA2=0,APP_DATA1=1,APP_DATA0=0;}
    else{         APP_DATA3=0,APP_DATA2=0,APP_DATA1=1,APP_DATA0=1;}
    APP_EN  = 1;
    while(APP_EN);
    if(1 == Len){ *pBuf++=DATA1;Len=Len-1; }
    else{         *pBuf++=DATA1;*pBuf++=DATA0;Len=Len-2;}
  }
  while(1)                             /*地址为偶数*/
  {
    if(0 == Len) break;
    if(Len < 4)
    {
      switch(Len)
      {
        case 3: APP_DATA3=1,APP_DATA2=1,APP_DATA1=1,APP_DATA0=0;break;
        case 2: APP_DATA3=1,APP_DATA2=1,APP_DATA1=0,APP_DATA0=0;break;
        case 1: APP_DATA3=1,APP_DATA2=0,APP_DATA1=0,APP_DATA0=0;break;
      }
      APP_EN  = 1;
      while(APP_EN);
      switch(Len)
      {
        case 3: *pBuf++=DATA3;*pBuf++=DATA2;*pBuf++=DATA1;break;
        case 2: *pBuf++=DATA3;*pBuf++=DATA2;break;
        case 1: *pBuf++=DATA3;break;
      }
      break;
      }
    else
    {
      APP_DATA3=1,APP_DATA2=1,APP_DATA1=1,APP_DATA0=1;
      APP_EN  = 1;
      while(APP_EN);
      *pBuf++=DATA3;*pBuf++=DATA2;*pBuf++=DATA1;*pBuf++=DATA0;
      Len = Len - 4;
    }
  }

  RAMMODE = 0x00;                      /*不占用时必须清零*/
  EA = 1;
}

/*****************************************************************************
写DGUS寄存器*/
/*****************************************************************************
 函 数 名  : WriteDGUS
 功能描述  : 读DGUS寄存器
 输入参数  : uint32_t Addr  DGUS寄存器地址
             uint8_t* pBuf  接收缓冲区
             uint16_t Len   写入数据字节长度(先高字再低字)
 输出参数  : 无
 修改历史  :
 日    期  : 2019年11月04日
 作    者  :
 修改内容  : 创建
*****************************************************************************/
void WriteDGUS(uint32_t Addr, uint8_t *pBuf, uint16_t Len)
{
  uint8_t Aoffset;
  if(NULL == pBuf){return;}
  if(0 == Len){return;}
  if((Addr+Len/2) > (0xFFFF*2)){return;}
  EA = 0;
  Aoffset = Addr&0x01;                 /*取bit0作为奇偶判断*/
  Addr  = Addr >> 1;                   /*配置地址*/
  ADR_H = (uint8_t)(Addr >> 16);
  ADR_M = (uint8_t)(Addr >> 8);
  ADR_L = (uint8_t)(Addr);
  RAMMODE = 0x00;
  ADR_INC = 0x01;
  APP_REQ = 1;                         /*占用DGUS读写*/
  while(!APP_ACK);
  APP_RW  = 0;                         /*写变量存储器*/

  if(1 == Aoffset)
  {                    /*地址为奇数*/
    if(1 == Len){ APP_DATA3=0,APP_DATA2=0,APP_DATA1=1,APP_DATA0=0;}
    else{         APP_DATA3=0,APP_DATA2=0,APP_DATA1=1,APP_DATA0=1;}
    if(1 == Len){ DATA1=*pBuf++;Len=Len-1; }
    else{         DATA1=*pBuf++;DATA0=*pBuf++;Len=Len-2;}
    APP_EN  = 1;
    while(APP_EN);
  }
  while(1)                             /*地址为偶数*/
  {
    if(0 == Len) break;
    if(Len < 4)
    {
      switch(Len)
      {
        case 3: APP_DATA3=1,APP_DATA2=1,APP_DATA1=1,APP_DATA0=0;break;
        case 2: APP_DATA3=1,APP_DATA2=1,APP_DATA1=0,APP_DATA0=0;break;
        case 1: APP_DATA3=1,APP_DATA2=0,APP_DATA1=0,APP_DATA0=0;break;
      }
      switch(Len)
      {
        case 3: DATA3=*pBuf++;DATA2=*pBuf++;DATA1=*pBuf++;break;
        case 2: DATA3=*pBuf++;DATA2=*pBuf++;break;
        case 1: DATA3=*pBuf++;break;
      }
      APP_EN  = 1;
      while(APP_EN);
      break;
    }
    else
    {
      APP_DATA3=1,APP_DATA2=1,APP_DATA1=1,APP_DATA0=1;
      DATA3=*pBuf++;DATA2=*pBuf++;DATA1=*pBuf++;DATA0=*pBuf++;
      APP_EN  = 1;
      while(APP_EN);
      Len = Len - 4;
    }
  }

  RAMMODE = 0x00;                      /*不占用时必须清零*/
  EA = 1;
}
