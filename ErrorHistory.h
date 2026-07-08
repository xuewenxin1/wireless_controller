#define	ERRORHISTORYNUM	100
#define	ERRORHISTORY_PAGE_SIZE	6
#define	ERRORHISTORY_VP_BASE	0x3200
#define	ERRORHISTORY_VP_KEY	0x3224
#define	ERRORHISTORY_WORDS_PER_REC	6
/* 1 字 magic + 100 条×4 字节 = 402 字节 → 201 字（Nor Flash 0x3F00 区内） */
#define	ERRORHISTORY_FLASH_WORDS	201
#define	ERRORHISTORY_FLASH_RECORDS	100
#define	ERRORHISTORY_FLASH_MAGIC	0x4854
#define	ERRORHISTORY_FLASH_MAGIC_OLD	0x4853

extern	unsigned short	AllErrorBit[4];
extern	unsigned char	ErrorHistory[ERRORHISTORYNUM][5];

extern	void	UpdateErrorHistory(unsigned short NewError,unsigned char ErrorBuffNum);
extern	void	ErrorHistoryDisplay(void);
extern	void	ClearErrorHistory(void);
extern	void	DecodeErrorHistory(void);
extern	void	ErrorHistory_PageChange(unsigned short keyValue);
extern	void	ErrorHistory_ResetPage(void);
extern	void	ErrorHistory_LoadFromFlash(void);
extern	void	ErrorHistory_Sanitize(void);
extern	void	ErrorHistory_FlashWrite(u32 flash_vp_addr);
extern	void	ErrorHistory_TryMigrateFlash(void);
extern	void	ErrorHistory_FlashCommitNow(void);
extern	unsigned char	UWOErrorCodeProcess(unsigned char Code);
