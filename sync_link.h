#ifndef SYNC_LINK_H
#define SYNC_LINK_H

/* 外机开关同步：VP 0x4021 / 0x2002，实现见 app_core.c */
#define SYNC_SRC_APP  'A'
#define SYNC_SRC_SCR  'S'

void SyncLink_ApplySwitch(unsigned char pwr, unsigned char src);

#endif
