/**
 * blackbox.c — 故障黑匣子（复位原因 + HardFault 现场，掉电保持）
 *
 * 原理:
 *  1) 复位原因: F407 的 RCC->CSR 记录了本次复位来源(IWDG/WWDG/SOFT/PIN/POR/BOR),
 *     读后必须置 RMVF 清除, 否则标志会累积; 原因码存入 RTC 备份寄存器
 *     (VBAT 纽扣电池供电域, 主电源掉电不丢失);
 *  2) HardFault 现场: 用 naked 函数接管 HAL 弱符号 HardFault_Handler,
 *     判断 MSP/PSP 后把栈帧里的 PC(stacked[6])、LR(stacked[5]) 与
 *     SCB->CFSR( fault 状态寄存器)写入备份寄存器, 随后立即复位自愈;
 *  3) 重启后由 HELLO 帧把 (复位原因, 故障PC/LR/CFSR) 上报上位机 ——
 *     "设备死了我知道它为什么死" 的最低成本实现。
 *
 * 集成: main 初始化处调用 BB_Init(); 工程若已有 HardFault_Handler 实现需合并。
 * 注意: 备份寄存器写入前需使能 PWR 时钟并清除 DBP 写保护(见 bb_write_bkp)。
 */
#include "blackbox.h"

#define BB_REG_CAUSE   RTC_BKP_DR0   /* 复位原因 + 故障有效标志 */
#define BB_REG_PC      RTC_BKP_DR1
#define BB_REG_LR      RTC_BKP_DR2
#define BB_REG_CFSR    RTC_BKP_DR3
#define BB_FAULT_MAGIC 0xB1AC0DE0UL  /* 故障记录有效性魔数 */

static BB_ResetCause s_cause = BB_RESET_UNKNOWN;

/* ---- 备份域写保护开关 ---- */
static void bb_backup_unlock(void)
{
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();          /* 清 DBP 位, 允许写 RTC 备份区 */
}
/* BKP0R..BKP19R 地址连续、间隔4字节(RM0090), 按下标取地址而非数组名 */
static volatile uint32_t *bb_bkp_reg(uint32_t idx)
{
    return (volatile uint32_t *)((uint32_t)&(RTC->BKP0R) + 4U * idx);
}
static void bb_write_bkp(uint32_t idx, uint32_t val) { *bb_bkp_reg(idx) = val; }
static uint32_t bb_read_bkp(uint32_t idx)            { return *bb_bkp_reg(idx); }

void BB_Init(void)
{
    uint32_t flags = RCC->CSR;

    if (flags & RCC_CSR_IWDGRST)      s_cause = BB_RESET_IWDG;
    else if (flags & RCC_CSR_WWDGRST) s_cause = BB_RESET_WWDG;
    else if (flags & RCC_CSR_SFTRST)  s_cause = BB_RESET_SOFTWARE;
    else if (flags & RCC_CSR_PORRST ||
             flags & RCC_CSR_BORRST)  s_cause = BB_RESET_POWER;
    else if (flags & RCC_CSR_PINRST)  s_cause = BB_RESET_PIN;
    else                              s_cause = BB_RESET_UNKNOWN;

    /* 读后清除全部复位标志(否则下次读到的是旧标志) */
    __HAL_RCC_PWR_CLK_ENABLE();
    SET_BIT(RCC->CSR, RCC_CSR_RMVF);
}

BB_ResetCause BB_LastReset(void)   { return s_cause; }

const char *BB_LastResetName(void)
{
    switch (s_cause) {
    case BB_RESET_POWER:    return "POR/BOR";
    case BB_RESET_PIN:      return "NRST-PIN";
    case BB_RESET_SOFTWARE: return "SOFT";
    case BB_RESET_IWDG:     return "IWDG(主循环卡死?)";
    case BB_RESET_WWDG:     return "WWDG";
    default:                return "UNKNOWN";
    }
}

void BB_RecordFaultAndReset(uint32_t pc, uint32_t lr, uint32_t cfsr)
{
    bb_backup_unlock();
    bb_write_bkp(0, BB_FAULT_MAGIC);   /* BKPxR 下标 0..3 对应 RTC_BKP_DR0..DR3 */
    bb_write_bkp(1, pc);
    bb_write_bkp(2, lr);
    bb_write_bkp(3, cfsr);
    NVIC_SystemReset();                /* 记录完成后立即复位自愈 */
}

uint32_t BB_FaultPC(void)   { return bb_read_bkp(1); }
uint32_t BB_FaultLR(void)   { return bb_read_bkp(2); }
uint32_t BB_FaultCFSR(void) { return bb_read_bkp(3); }
uint8_t  BB_HasFaultRecord(void)
{
    return (bb_read_bkp(0) == BB_FAULT_MAGIC) ? 1U : 0U;
}

/* ---- HardFault 接管: naked 汇编取栈帧, C 函数记录后复位 ---- */
__attribute__((naked)) void HardFault_Handler(void)
{
    __asm volatile(
        "tst   lr, #4          \n"   /* EXC_RETURN bit2: 0=MSP 1=PSP */
        "ite   eq              \n"
        "mrseq r0, msp         \n"
        "mrsne r0, psp         \n"
        "mov   r1, lr          \n"
        "b     BB_HardFault_C  \n"
    );
}

void BB_HardFault_C(uint32_t stacked[], uint32_t exc_lr)
{
    /* 栈帧布局: r0,r1,r2,r3,r12,lr,pc,xpsr -> PC=stacked[6], LR=stacked[5] */
    uint32_t pc   = stacked[6];
    uint32_t lr   = stacked[5];
    uint32_t cfsr = SCB->CFSR;
    (void)exc_lr;
    BB_RecordFaultAndReset(pc, lr, cfsr);
    while (1) { }                 /* 不可达: Record 内部已复位 */
}
