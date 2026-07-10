//#include "sys.h"
//#include "ErrorHistory.h"
//#include "control.h"

//unsigned short	AllErrorBit[4] = {0};
///* 用于位变化检测的旧值 */
//static unsigned short OldError[4] = {0};
///*==============================
// * 内部状态
// *==============================*/

//u8  s_page     = 0;
//static u16 s_writePtr = 0;
///*==============================
// * 精确闰年日历
// *==============================*/
//static u8 IsLeap(u16 y)
//{
//    return ((y % 4 == 0) && (y % 100 != 0)) || (y % 400 == 0);
//}

//static const u16 DaysToMonth[13] =
//{
//    0,31,59,90,120,151,181,212,243,273,304,334,365
//};

///* 将 DGUS 时间（0x3001 格式）转为秒 */
//static u32 DGUSTimeToTimestamp(u16 *dgTime)
//{
//    u16 y = dgTime[0];   /* 年 */
//    u16 m = dgTime[1];   /* 月 */
//    u16 d = dgTime[2];   /* 日 */
//    u16 h = dgTime[3];   /* 时 */

//    u32 days = 0;
//    u16 yy;

//    /* 计算从 2000-01-01 到 y-m-d 的天数 */
//    for (yy = 2000; yy < y; yy++)
//        days += IsLeap(yy) ? 366 : 365;

//    for (yy = 1; yy < m; yy++)
//    {
//        days += DaysToMonth[yy] - DaysToMonth[yy - 1];
//        if (yy == 2 && IsLeap(y))
//            days++;
//    }

//    days += d - 1;

//    return days * 86400UL + h * 3600UL;
//}


//void UpdateErrorHistory(unsigned short NewError, unsigned char ErrorBuffNum)
//{
//    unsigned char i;
//    unsigned short time[4];

//    if (NewError != OldError[ErrorBuffNum])
//    {
//        /* 从 DGUS 读当前时间（0x3001 开始 4 个 VP） */
//        read_dgus_vp(0x3001, (u8 *)&time, 4);

//        /* 逐位检测上升沿（新故障） */
//        for (i = 0; i < 16; i++)
//        {
//            if (((NewError >> i) & 0x01) >
//                ((OldError[ErrorBuffNum] >> i) & 0x01))
//            {
//                /* 生成故障码（1~64） */
//                u16 errCode = UWOErrorCodeProcess(
//                    ErrorBuffNum * 16 + i + 1
//                );

//                /* 将 DGUS 时间转换为时间戳 */
//                u32 ts = DGUSTimeToTimestamp(time);

//                /* 存入 Flash（最新在最前） */
//                ERR_AddWithTime(errCode, ts);
//            }
//        }

//        OldError[ErrorBuffNum] = NewError;
//    }
//}



//void ERR_AddWithTime(u16 errCode, u32 ts)
//{
//    u16 rec[ERR_REC_WORDS];

//    rec[0] = errCode;
//    rec[1] = (u16)(ts & 0xFFFF);
//    rec[2] = (u16)(ts >> 16);
//    rec[3] = 0;

//    ERR_FlashSaveRec(s_writePtr, rec);

//    s_writePtr++;
//    if (s_writePtr >= ERR_MAX_NUM)
//        s_writePtr = 0;

//    ERR_SavePtr();
//}

//void	ErrorHistoryDisplay(void)
//{
//	ERR_ShowPage(s_page);   /* 直接调用 Flash 显示 */
//}

//void ClearErrorHistory(void)
//{
//    u16 rec[ERR_REC_WORDS] = {0};
//    u16 i;

//    /* 清 Flash 区 */
//    for (i = 0; i < ERR_MAX_NUM; i++)
//        ERR_FlashSaveRec(i, rec);

//    /* 复位写指针 */
//    s_writePtr = 0;
//    ERR_SavePtr();
//}

//void DecodeErrorHistory(void)
//{
//    unsigned char i;
//    unsigned short page;

//    for (i = 0; i < 4; i++)
//        UpdateErrorHistory(AllErrorBit[i], i);

//    page = GetPageID();
//    if (page == 45)
//		ERR_ShowPage(s_page);
////        App_ErrorHistoryDisplay();
//}

//unsigned char	UWOErrorCodeProcess(unsigned char	Code)
//{
//	return		Code;
//}

///*****************************************************************************/
//	//读写T5L片内256KW Flash，mod=0x5A 为读取，mod=0xA5为写入
//	//addr=DGUS变量地址，必须是偶数；addr_flash=flash读取地址，必须是偶数；len=读取字长度，必须是偶数。
//void T5L_Flash(unsigned char mod,unsigned int addr,long addr_flash,unsigned int len)
//{	
//	ADR_H=0x00;
//	ADR_M=0x00;
//	ADR_L=0x04;
//	ADR_INC=0x01;
//	RAMMODE=0x8F;		//启动读Flash
//	while(APP_ACK==0);
//	DATA3=mod;
//	DATA2=(unsigned char)(addr_flash>>16);
//	DATA1=(unsigned char)(addr_flash>>8);
//	DATA0=(unsigned char)(addr_flash&0xFE);
//	APP_EN=1;
//	while(APP_EN==1);	
//	DATA3=(unsigned char)(addr>>8);
//	DATA2=(unsigned char)(addr&0xFE);
//	DATA1=(unsigned char)(len>>8);
//	DATA0=(unsigned char)(len&0xFE);
//	APP_EN=1;
//	while(APP_EN==1);
//	RAMMODE=0x00;
//	wait_ok(0x0004);
//}	//等待数据读取OK



///*==============================
// * 时间戳接口（对接 RTC）
// *==============================*/
//static u32 ERR_GetTsSec(void)
//{
//    /* TODO: 返回秒（epoch=2000-01-01） */
//    return 0;
//}

///*==============================
// * Flash 读写封装
// *==============================*/
//static void ERR_FlashSaveRec(u16 idx, u16 *rec)
//{
//    u32 addr = ERR_FLASH_BASE + (u32)idx * ERR_REC_WORDS * 2u;
//    T5L_Flash(0x01, (u16)rec, addr, ERR_REC_WORDS * 2u);
//}

//static void ERR_FlashLoadRec(u16 idx, u16 *rec)
//{
//    u32 addr = ERR_FLASH_BASE + (u32)idx * ERR_REC_WORDS * 2u;
//    T5L_Flash(0x00, (u16)rec, addr, ERR_REC_WORDS * 2u);
//}

///*==============================
// * 写指针
// *==============================*/
//void ERR_SavePtr(void)
//{
//    T5L_Flash(0x01, (u16)&s_writePtr, ERR_FLASH_PTR_ADDR, 2u);
//}

//static void ERR_LoadPtr(void)
//{
//    T5L_Flash(0x00, (u16)&s_writePtr, ERR_FLASH_PTR_ADDR, 2u);
//    if (s_writePtr >= ERR_MAX_NUM)
//        s_writePtr = 0;
//}



//static void ERR_TsToDate(u32 ts,
//                         u16 *py, u16 *pm, u16 *pd,
//                         u16 *ph, u16 *pn)
//{
//    u32 days = ts / 86400;
//    u32 secs = ts % 86400;
//    u16 y = 2000;

//    *ph = secs / 3600;
//    *pn = (secs % 3600) / 60;

//    while (1)
//    {
//        u16 ydays = IsLeap(y) ? 366 : 365;
//        if (days < ydays) break;
//        days -= ydays;
//        y++;
//    }

//    for (*pm = 1; *pm <= 12; (*pm)++)
//    {
//        u16 mdays = DaysToMonth[*pm] - DaysToMonth[*pm - 1];
//        if (*pm == 2 && IsLeap(y)) mdays++;
//        if (days < mdays) break;
//        days -= mdays;
//    }

//    *py = y;
//    *pd = days + 1;
//}

///*==============================
// * 新增故障
// *==============================*/
//void ERR_Add(u16 errCode)
//{
//    u16 rec[ERR_REC_WORDS];
//    u32 ts = ERR_GetTsSec();

//    rec[0] = errCode;
//    rec[1] = (u16)(ts & 0xFFFF);
//    rec[2] = (u16)(ts >> 16);
//    rec[3] = 0;

//    ERR_FlashSaveRec(s_writePtr, rec);

//    s_writePtr++;
//    if (s_writePtr >= ERR_MAX_NUM)
//        s_writePtr = 0;

//    ERR_SavePtr();
//}

///*==============================
// * 显示单条到 0x3200~0x3205
// *==============================*/
//static void ERR_WriteOneRecord(u8 i, u16 *rec, u8 isEmpty)
//{
//    u16 vpBase = ERR_VP_DISP_BASE + i * ERR_VP_DISP_STRIDE;
//    u16 val;
//	u32 ts;
//	u16 y, m, d, hh, mm;

//    if (isEmpty)
//    {
//        /* 空记录：DGUS 端配置 0xFFFF → "----" */
//        val = 0xFFFF;
//        write_dgus_vp(vpBase + ERR_VP_INDEX,    (u8*)&val, 2);
//        write_dgus_vp(vpBase + ERR_VP_MONTH,    (u8*)&val, 2);
//        write_dgus_vp(vpBase + ERR_VP_DAY,      (u8*)&val, 2);
//        write_dgus_vp(vpBase + ERR_VP_HOUR,     (u8*)&val, 2);
//        write_dgus_vp(vpBase + ERR_VP_MINUTE,   (u8*)&val, 2);
//        write_dgus_vp(vpBase + ERR_VP_ERRCODE,  (u8*)&val, 2);
//        return;
//    }

//    /* 序号 */
//    val = s_page * ERR_PER_PAGE + i + 1;
//    write_dgus_vp(vpBase + ERR_VP_INDEX, (u8*)&val, 2);

//    /* 时间 */
//    ts = ((u32)rec[2] << 16) | rec[1];
//    
//    ERR_TsToDate(ts, &y, &m, &d, &hh, &mm);

//    write_dgus_vp(vpBase + ERR_VP_MONTH,   (u8*)&m,  2);
//    write_dgus_vp(vpBase + ERR_VP_DAY,     (u8*)&d,  2);
//    write_dgus_vp(vpBase + ERR_VP_HOUR,    (u8*)&hh, 2);
//    write_dgus_vp(vpBase + ERR_VP_MINUTE,  (u8*)&mm, 2);

//    /* 故障号 */
//    write_dgus_vp(vpBase + ERR_VP_ERRCODE,(u8*)&rec[0], 2);
//}

///*==============================
// * 显示某一页
// *==============================*/
//void ERR_ShowPage(u8 page)
//{
//    u8 i;
//    u16 rec[ERR_REC_WORDS];
//    u16 idx;

//    if (page > (ERR_MAX_NUM - 1) / ERR_PER_PAGE)
//        page = 0;

//    s_page = page;

//    for (i = 0; i < ERR_PER_PAGE; i++)
//    {
//        idx = (s_writePtr - 1 - (page * ERR_PER_PAGE + i));
//        if (idx >= ERR_MAX_NUM)
//            idx += ERR_MAX_NUM;

//        ERR_FlashLoadRec(idx, rec);

//        if (rec[0] == 0 || rec[0] == 0xFFFF)
//            ERR_WriteOneRecord(i, rec, 1);
//        else
//            ERR_WriteOneRecord(i, rec, 0);
//    }
//}

///*==============================
// * 初始化
// *==============================*/
//void ERR_Init(void)
//{
//    u16 flag = 0;

//    T5L_Flash(0x5A, (u16)&flag, ERR_VP_INIT_FLAG, 2u);

//    if (flag != ERR_INIT_MARK)
//    {
//        s_writePtr = 0;
//        ERR_SavePtr();

//        flag = ERR_INIT_MARK;
//        T5L_Flash(0xA5, (u16)&flag, ERR_VP_INIT_FLAG, 2u);
//    }
//    else
//    {
//        ERR_LoadPtr();
//    }

//    s_page = 0;
//    ERR_ShowPage(s_page);
//}

///*==============================
// * 切页
// *==============================*/
//void ERR_OnPageKey(u16 keyValue)
//{
//    u8 maxPage = (ERR_MAX_NUM - 1) / ERR_PER_PAGE;

//    if (keyValue == 1 && s_page > 0)
//        s_page--;
//    else if (keyValue == 2 && s_page < maxPage)
//        s_page++;

//    ERR_ShowPage(s_page);
//}
#include "sys.h"
#include "ErrorHistory.h"
#include "control.h"
#include "upload.h"
#include "rtc.h"
#include "app_core.h"
#include "unused_suppress.h"

unsigned short	AllErrorBit[4] = {0};
unsigned char	ErrorHistory[ERRORHISTORYNUM][5] = {0};
static unsigned char	s_page = 0;

static unsigned char ErrorHistory_IsValidCode(unsigned char fault_code)
{
	return (fault_code >= 1 && fault_code <= 64) ? 1 : 0;
}

static unsigned char ErrorHistory_ClampByte(u16 v, u8 maxv)
{
	if(v > maxv)
		return maxv;
	return (u8)v;
}

/* 记录格式: [0]月 [1]日 [2]时 [3]分 [4]故障码 */
static void ErrorHistory_ReadTime(u8 *rec)
{
	u16 year = 0;
	u16 month_vp = 0;
	u16 day = 0;
	u16 hour = 0;
	u16 minute = 0;

	read_dgus_vp((u32)0x3010, (u8 *)&year, 1);
	read_dgus_vp((u32)0x3011, (u8 *)&month_vp, 1);
	read_dgus_vp((u32)0x3012, (u8 *)&day, 1);
	read_dgus_vp((u32)0x3013, (u8 *)&hour, 1);
	read_dgus_vp((u32)0x3014, (u8 *)&minute, 1);
	rec[0] = (u8)RTC_MonthFromDisplayVp(year, month_vp);
	rec[1] = ErrorHistory_ClampByte(day, 31);
	rec[2] = ErrorHistory_ClampByte(hour, 23);
	rec[3] = ErrorHistory_ClampByte(minute, 59);
}

static void ErrorHistory_PackRec(u8 *out, u8 *rec)
{
	u32 v;

	if(!ErrorHistory_IsValidCode(rec[4]))
	{
		out[0] = 0;
		out[1] = 0;
		out[2] = 0;
		out[3] = 0;
		return;
	}
	v = (u32)(rec[4] & 0x3F);
	v |= ((u32)(ErrorHistory_ClampByte(rec[3], 59) & 0x3F) << 6);
	v |= ((u32)(ErrorHistory_ClampByte(rec[2], 23) & 0x1F) << 12);
	v |= ((u32)(ErrorHistory_ClampByte(rec[1], 31) & 0x1F) << 17);
	v |= ((u32)(ErrorHistory_ClampByte(rec[0], 12) & 0x0F) << 22);
	out[0] = (u8)v;
	out[1] = (u8)(v >> 8);
	out[2] = (u8)(v >> 16);
	out[3] = (u8)(v >> 24);
}

static void ErrorHistory_UnpackRec(u8 *rec, u8 *in)
{
	u32 v;

	v = (u32)in[0] | ((u32)in[1] << 8) | ((u32)in[2] << 16) | ((u32)in[3] << 24);
	if((v & 0x3F) == 0)
	{
		rec[0] = 0;
		rec[1] = 0;
		rec[2] = 0;
		rec[3] = 0;
		rec[4] = 0;
		return;
	}
	rec[4] = (u8)(v & 0x3F);
	rec[3] = (u8)((v >> 6) & 0x3F);
	rec[2] = (u8)((v >> 12) & 0x1F);
	rec[1] = (u8)((v >> 17) & 0x1F);
	rec[0] = (u8)((v >> 22) & 0x0F);
}

static void ErrorHistory_UnpackRecLegacy(u8 *rec, u8 *in)
{
	u32 v;
	u8 month;
	u8 day;
	u8 hour;

	v = (u32)in[0] | ((u32)in[1] << 8) | ((u32)in[2] << 16) | ((u32)in[3] << 24);
	if((v & 0x3F) == 0)
	{
		rec[0] = 0;
		rec[1] = 0;
		rec[2] = 0;
		rec[3] = 0;
		rec[4] = 0;
		return;
	}
	hour = (u8)((v >> 6) & 0x1F);
	day = (u8)((v >> 11) & 0x1F);
	month = (u8)((v >> 16) & 0x0F);
	rec[0] = month;
	rec[1] = day;
	rec[2] = hour;
	rec[3] = 0;
	rec[4] = (u8)(v & 0x3F);
}

static void ErrorHistory_ConvertLegacyRawRec(u8 *rec)
{
	u8 month;
	u8 day;
	u8 hour;
	u8 fault_code;

	if(!ErrorHistory_IsValidCode(rec[4]))
	{
		rec[0] = 0;
		rec[1] = 0;
		rec[2] = 0;
		rec[3] = 0;
		rec[4] = 0;
		return;
	}
	month = rec[1];
	day = rec[2];
	hour = rec[3];
	fault_code = rec[4];
	rec[0] = month;
	rec[1] = day;
	rec[2] = hour;
	rec[3] = 0;
	rec[4] = fault_code;
}

void ErrorHistory_FlashWrite(u32 flash_vp_addr)
{
	u8 packed[ERRORHISTORYNUM * 4];
	u8 i;
	u16 magic = ERRORHISTORY_FLASH_MAGIC;

	for(i = 0; i < ERRORHISTORYNUM; i++)
		ErrorHistory_PackRec(packed + i * 4, ErrorHistory[i]);
	write_dgus_vp(flash_vp_addr, (u8 *)&magic, 1);
	write_dgus_vp(flash_vp_addr + 1, packed, (u16)(ERRORHISTORY_FLASH_WORDS - 1));
}

static u32 ErrorHistory_FlashVpAddr(void)
{
	return (u32)(NOR_FLASH_FIRST + MEMORY_HEADER_WORDS + MEMORY_LANGUAGE_WORDS
		+ MEMORY_TIMER_SLOT_WORDS + MEMORY_INDOOR_TIMER_WORDS + 1 + 1);
}

void ErrorHistory_FlashCommitNow(void)
{
	u16 enable_memory = 0;

	ErrorHistory_FlashWrite(ErrorHistory_FlashVpAddr());
	read_dgus_vp((u32)(0x3508), (u8 *)&enable_memory, 1);
	if(enable_memory == 1)
	{
		Write_Memory();
		Write_Nor_Flash();
		Save_Flash_Flag = 0;
		Flash_Save_Count = 0;
	}
}

void ErrorHistory_TryMigrateFlash(void)
{
	u16 magic = 0;
	u16 enable_memory = 0;
	u32 addr;

	addr = ErrorHistory_FlashVpAddr();
	read_dgus_vp(addr, (u8 *)&magic, 1);
	if(magic == ERRORHISTORY_FLASH_MAGIC)
		return;

	ErrorHistory_Sanitize();
	ErrorHistory_FlashWrite(addr);

	read_dgus_vp((u32)(0x3508), (u8 *)&enable_memory, 1);
	if(enable_memory == 1)
	{
		Write_Memory();
		Write_Nor_Flash();
		Save_Flash_Flag = 0;
		Flash_Save_Count = 0;
	}
}

void ErrorHistory_FlashRead(u32 flash_vp_addr)
{
	u16 magic = 0;
	u8 packed[ERRORHISTORYNUM * 4];
	u8 i, j;

	read_dgus_vp(flash_vp_addr, (u8 *)&magic, 1);
	if(magic == ERRORHISTORY_FLASH_MAGIC)
	{
		read_dgus_vp(flash_vp_addr + 1, packed, (u16)(ERRORHISTORY_FLASH_WORDS - 1));
		for(i = 0; i < ERRORHISTORYNUM; i++)
			ErrorHistory_UnpackRec(ErrorHistory[i], packed + i * 4);
		return;
	}
	if(magic == ERRORHISTORY_FLASH_MAGIC_OLD)
	{
		read_dgus_vp(flash_vp_addr + 1, packed, (u16)(ERRORHISTORY_FLASH_WORDS - 1));
		for(i = 0; i < ERRORHISTORYNUM; i++)
			ErrorHistory_UnpackRecLegacy(ErrorHistory[i], packed + i * 4);
		return;
	}
	/* 兼容旧版 85 条×5 字节原始布局: 年/月/日/时/码 */
	read_dgus_vp(flash_vp_addr, (u8 *)&ErrorHistory, 214);
	for(i = 0; i < 85; i++)
		ErrorHistory_ConvertLegacyRawRec(ErrorHistory[i]);
	for(i = 85; i < ERRORHISTORYNUM; i++)
	{
		for(j = 0; j < 5; j++)
			ErrorHistory[i][j] = 0;
	}
	for(i = 0; i < 85; i++)
	{
		if(!ErrorHistory_IsValidCode(ErrorHistory[i][4]))
		{
			for(j = i; j < ERRORHISTORYNUM; j++)
			{
				u8 k;
				for(k = 0; k < 5; k++)
					ErrorHistory[j][k] = 0;
			}
			break;
		}
	}
}

void ErrorHistory_LoadFromFlash(void)
{
	ErrorHistory_FlashRead(ErrorHistory_FlashVpAddr());
	ErrorHistory_Sanitize();
	ErrorHistoryDisplay();
}

void ErrorHistory_Sanitize(void)
{
	unsigned char i, j, k;

	for(i = 0; i < ERRORHISTORYNUM; i++)
	{
		if(!ErrorHistory_IsValidCode(ErrorHistory[i][4]))
		{
			for(j = i; j < ERRORHISTORYNUM; j++)
			{
				for(k = 0; k < 5; k++)
					ErrorHistory[j][k] = 0;
			}
			break;
		}
	}
}

void ErrorHistory_Init(void)
{
	ErrorHistory_LoadFromFlash();
}

#if UNUSED_KEEP_CODE
void ErrorHistory_Init_unused(void)
{
	ErrorHistory_Sanitize();
	s_page = 0;
	App_ErrorHistoryTryMigrateFlash();
	App_ErrorHistoryDisplay();
}
#endif

static unsigned char ErrorHistory_GetCount(void)
{
	unsigned char i;

	for (i = 0; i < ERRORHISTORYNUM; i++)
	{
		if (ErrorHistory[i][4] == 0)
			break;
	}
	return i;
}

static unsigned char ErrorHistory_GetMaxPage(void)
{
	unsigned char count;

	count = ErrorHistory_GetCount();
	if (count == 0)
		return 0;
	return (count - 1) / ERRORHISTORY_PAGE_SIZE;
}

void	UpdateErrorHistory(unsigned short NewError,unsigned char ErrorBuffNum)
{
	static unsigned short	OldError[4] = {0};
	unsigned char	i,j,k;
	u8 time_rec[4];
	
	if(NewError != OldError[ErrorBuffNum])//检测到故障有变化F
	{
		ErrorHistory_ReadTime(time_rec);
		for(i=0;i<16;i++)
		{
			if(((NewError >> i) & 0x01) > ((OldError[ErrorBuffNum] >> i) & 0x01))//找到新产生的故障位置F
			{
				for(j = ERRORHISTORYNUM - 1; j > 0; j--)
				{
					for(k=0;k<5;k++)
					{
						ErrorHistory[j][k] = ErrorHistory[j-1][k];//将所有故障记录下移一行F
					}
				}
				for(k=0;k<4;k++)
				{
					ErrorHistory[0][k] = time_rec[k];
				}
				ErrorHistory[0][4] = UWOErrorCodeProcess(ErrorBuffNum*16+i+1);//写入最新一条故障F
			}
		}
		OldError[ErrorBuffNum] = NewError;
		Ready_To_Save();
		upload_history_faults();
	}
}
void	ErrorHistoryDisplay(void)
{
	unsigned char	i,j;
	unsigned char	rowIdx;
	unsigned short	ErrorHistoryDisplay_Temp[6]={0};
	
	for(i=0;i<ERRORHISTORY_PAGE_SIZE;i++)
	{
		rowIdx = (unsigned char)(s_page * ERRORHISTORY_PAGE_SIZE + i);
		if((rowIdx < ERRORHISTORYNUM) && ErrorHistory[rowIdx][4])
		{
			ErrorHistoryDisplay_Temp[0] = rowIdx + 1;
			for(j=1;j<ERRORHISTORY_WORDS_PER_REC;j++)
			{
				ErrorHistoryDisplay_Temp[j] = ErrorHistory[rowIdx][j-1];
			}
		}
		else
		{
			for(j=0;j<ERRORHISTORY_WORDS_PER_REC;j++)
			{
				ErrorHistoryDisplay_Temp[j] = 0;
			}
		}
		write_dgus_vp((u32)(ERRORHISTORY_VP_BASE + (i * ERRORHISTORY_WORDS_PER_REC)),(u8*)&ErrorHistoryDisplay_Temp,ERRORHISTORY_WORDS_PER_REC);
	}
}
void	ErrorHistory_PageChange(unsigned short keyValue)
{
	unsigned char maxPage;

	maxPage = ErrorHistory_GetMaxPage();
	if(keyValue == 1)
	{
		if(s_page > 0)
			s_page--;
	}
	else if(keyValue == 2)
	{
		if(s_page < maxPage)
			s_page++;
	}
	App_ErrorHistoryDisplay();
}
void	ErrorHistory_ResetPage(void)
{
	s_page = 0;
}
void	ClearErrorHistory(void)
{
	unsigned char	i,j;
	
	for(i=0;i<ERRORHISTORYNUM;i++)
	{
		for(j=0;j<5;j++)
		{
			ErrorHistory[i][j] = 0;
		}
	}
	s_page = 0;
	ErrorHistory_FlashCommitNow();
	upload_history_faults();
}
void	DecodeErrorHistory(void)
{
	unsigned char	i;
	unsigned short	DecodeErrorHistory_Page;
	
	for(i=0;i<4;i++)
	{
		UpdateErrorHistory(AllErrorBit[i],i);//四个寄存器轮询是否有新故障F
	}
	DecodeErrorHistory_Page = GetPageID();
	if((DecodeErrorHistory_Page==45)||(DecodeErrorHistory_Page==46)||(DecodeErrorHistory_Page==47)||(DecodeErrorHistory_Page==48)||(DecodeErrorHistory_Page==49))//在查询历史故障页面持续刷新F
	{
		App_ErrorHistoryDisplay();
	}
}
unsigned char	UWOErrorCodeProcess(unsigned char	Code)
{
	return		Code;
}
