#ifndef UNUSED_SUPPRESS_H
#define UNUSED_SUPPRESS_H

/*
 * 0 = 不编译当前未引用代码，消除 Keil L16/L57 链接警告
 * 1 = 保留全部（调试/恢复功能时改为 1）
 */
#define UNUSED_KEEP_CODE  0

#ifndef DEBUG_SUPPRESS_FAULT_UPLOAD
#define DEBUG_SUPPRESS_FAULT_UPLOAD  0
#endif
#ifndef DEBUG_SUPPRESS_STATUS_UPLOAD
#define DEBUG_SUPPRESS_STATUS_UPLOAD 0
#endif

#endif
