/**
 * blackbox.h — 故障黑匣子：复位原因 + HardFault 现场快照
 */
#ifndef BLACKBOX_H
#define BLACKBOX_H

#include "stm32f4xx_hal.h"

typedef enum {
    BB_RESET_UNKNOWN = 0,
    BB_RESET_POWER   = 1,   /* 上电/欠压复位 (POR/BOR) */
    BB_RESET_PIN     = 2,   /* NRST 引脚复位 */
    BB_RESET_SOFTWARE= 3,   /* 软件复位 (NVIC_SystemReset) */
    BB_RESET_IWDG    = 4,   /* 独立看门狗复位 —— 通常意味着主循环卡死 */
    BB_RESET_WWDG    = 5,   /* 窗口看门狗复位 */
} BB_ResetCause;

/* 上电调用一次: 锁存本次复位原因(记录在 RTC 备份区, 掉电保持) */
void        BB_Init(void);
BB_ResetCause BB_LastReset(void);
/* 上报用: 转成可读字符串, 如 "IWDG" / "PIN" */
const char* BB_LastResetName(void);
/* HardFault 现场记录: 写入 RTC 备份寄存器后立即复位重启 */
void        BB_RecordFaultAndReset(uint32_t pc, uint32_t lr, uint32_t cfsr);
/* 读回上一次故障现场(随 HELLO 帧上报) */
uint32_t    BB_FaultPC(void);
uint32_t    BB_FaultLR(void);
uint32_t    BB_FaultCFSR(void);
uint8_t     BB_HasFaultRecord(void);

#endif /* BLACKBOX_H */
