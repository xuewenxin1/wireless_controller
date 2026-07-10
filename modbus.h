#ifndef __MODBUS_H
#define __MODBUS_H

#include "sys.h"

#define	INITIAL_NUM	21
#define	CHECK_PARAMETER_NUM	25
#define	CHECK_PARAMETER_NUM_2	45
#define CHECK_PARAMETER_NUM_3   19
#define CHECK_PARAMETER_NUM_4   3
#define CHECK_PARAMETER_NUM_5   600
#define	USER_PARAMETER_NUM	13
#define	SET_PARAMETER_NUM	96

#define MODBUS_BUFFER_LEN	300
#define INDOOR_UNIT_NUM		6
#define INDOOR_UNIT_REG_NUM	30

#define MODBUS_ERROR	 60000

/* 外机开关 VP：4021=屏/App 控制，Modbus 读100 实际状态也回写 4021（与屏同步） */
#define VP_EXT_PWR	0x4021U
/* Modbus 读200 reg215：内机开机台数 */
#define VP_INDOOR_ON_COUNT	0x4016U
#define VP_INDOOR_INSTALLED_COUNT	0x1004U
/* 首台开机内机模式显示（原误用 4016，与开机台数分开） */
#define VP_HOME_INDOOR_MODE	0x4017U

extern u8	Modbus_Buffer[MODBUS_BUFFER_LEN];

extern u8 send_type;
extern u16 En_change_Controller;
extern u8 Read_Or_Write;
extern u8 Analysis_OK;

extern	bit	Modbus_Read_Initial;
extern	bit	Modbus_Read_UpperLimit;
extern	bit	Modbus_Read_LowerLimit;
extern	bit	Modbus_Read_Check;
extern	bit	Modbus_Read_Check_2;
extern	bit	Modbus_Read_User;
extern	bit	Modbus_Read_Set;
extern	bit	Modbus_Write_Ueser;
extern	bit	Modbus_Write_Set;
extern	bit	Modbus_Read_Error;
extern	bit	Modbus_Write_ClearPower;
extern	bit	Modbus_Write_Defrost;
extern	bit	Modbus_Write_5;
extern	bit	Modbus_Write_6;
extern	bit	Modbus_Write_6_All;
extern	u8	Indoor_Unit_Index;

extern	signed	short	SetParameterUpperLimit[SET_PARAMETER_NUM];
extern	signed	short	SetParameterLowerLimit[SET_PARAMETER_NUM];
extern	unsigned	char	IsCheckParameterTemperature[CHECK_PARAMETER_NUM];
extern	unsigned	char	IsCheckParameterTemperature_2[CHECK_PARAMETER_NUM_2];

typedef struct{
    u16 SetTempHeatUpLimited;
    u16 SetTempHeatDownLimited;
    u16 SetTempCoolUpLimited;
    u16 SetTempCoolDownLimited;
    u16 SetTempHotWaterUpLimited;
    u16 SetTempHotWaterDownLimited;
}STR_FIRST_QUERY;

extern STR_FIRST_QUERY FirstQueryMember;

u16 CalculateModbusCrc(u8 *pBuf, u16 nLen);

void Modbus_Init(void);
void Modbus_Salve_Handler1(void);
void Modbus_Analysis_Data1(void);
void Modbus_TriggerIndoorUnitWrite(u8 index, u8 write_all);
void Modbus_TriggerGroupControlWrite(void);
void Modbus_TriggerGroupControlWriteOff(void);
void Modbus_TriggerDefrostParamWrite(u16 vp);
void Modbus_RefreshCheckParamDisplay(void);

#define MODBUS_CHECK_STATUS_LEN	75
void Modbus_GetCheckStatus(u8 *buf, u8 len);
void Modbus_StartManualDefrost(void);
void Modbus_ApplyIndoorTimerVpToUnits(u8 update_on, u8 update_off);
void Modbus_ClearManualDefrost(void);

u16 GetModbusReg(u8 *rx, u16 reg);

void Modbus_TriggerUserWrite(void);
void Modbus_TriggerUserWriteWith(u16 pwr, u16 mode);
void Modbus_MarkSetTempDirty(void);
void Modbus_QueueSetTempUserWrite(void);
void Modbus_FlushSetTempUserWrite(void);
void Modbus_SetTempWriteService(void);
u8 Modbus_UserWriteBusy(void);
u8 Modbus_IndoorWriteBusy(void);
u8 Modbus_UserHoldActive(void);
void Modbus_GetUserPowerMode(u16 *pwr, u16 *mode);
u8 Modbus_GetIndoorInstalledCount(void);

#endif
