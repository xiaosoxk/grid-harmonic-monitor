/**
 * event_rec.c — 事件录波触发器（上升沿检测 + 确认 + 冷却）
 *
 * 触发状态机:
 *   IDLE --(thdi>阈值)--> SUSPECT(计数) --(连续 EVENT_CONFIRM_WINDOWS 窗)--> FIRED
 *   FIRED: 发事件元数据 + 事件前后各一窗波形, 进入 COOLDOWN(约20s)
 *   COOLDOWN 结束或 thdi 回落后回到 IDLE。
 * 设计要点(答辩素材):
 *  1) 确认窗防毛刺: 单窗超限可能是瞬时冲击或算法抖动, 连续2窗确认再录;
 *  2) 冷却期防风暴: 持续畸变工况下若每窗都录会打爆串口与存储,
 *     20s 冷却期内的同类事件只记第一条;
 *  3) 上升沿语义: 记录的是"畸变发生"这个时刻, 持续越限不重复录,
 *     恢复后的再次越限会重新触发 —— 一次投切一条事件。
 * 集成(main.c 分析完成后):
 *   if (EVENT_OnResult(s_result.thdi, s_result.irms, s_result.locked,
 *                      (uint16_t)s_win_cnt, utc_now)) {
 *       UART_SendEvent(utc_now, EVENT_LastTrigThdi(), s_result.irms,
 *                      (uint16_t)s_win_cnt, s_result.locked);
 *       UART_SendWave(cur_wave_buf, ...);   /* 触发窗波形 */
 *       UART_SendWave(prev_wave_buf, ...);  /* 事件前窗波形 */
 *   }
 */
#include "event_rec.h"

typedef enum { EV_IDLE = 0, EV_SUSPECT, EV_COOLDOWN } EvState;

static EvState  s_state = EV_IDLE;
static uint16_t s_confirm_cnt = 0;
static uint16_t s_cooldown_cnt = 0;
static float    s_trig_thdi = 0.0f;
static uint16_t s_trig_seq = 0;

uint8_t EVENT_OnResult(float thdi, float irms, uint8_t locked,
                       uint16_t seq, uint32_t utc)
{
    (void)irms; (void)locked; (void)utc;   /* 随事件帧扩展可携带更多现场量 */
    uint8_t fired = 0;

    switch (s_state) {
    case EV_IDLE:
        if (thdi > EVENT_THDI_HIGH) {
            s_confirm_cnt = 1U;
            s_state = EV_SUSPECT;
        }
        break;

    case EV_SUSPECT:
        if (thdi > EVENT_THDI_HIGH) {
            if (++s_confirm_cnt >= EVENT_CONFIRM_WINDOWS) {
                s_trig_thdi = thdi;          /* 锁存触发时刻现场 */
                s_trig_seq  = seq;
                fired = 1U;
                s_cooldown_cnt = EVENT_COOLDOWN_WINDOWS;
                s_state = EV_COOLDOWN;
            }
        } else {
            s_state = EV_IDLE;               /* 未确认即回落: 毛刺 */
        }
        break;

    case EV_COOLDOWN:
        if (s_cooldown_cnt > 0U) {
            s_cooldown_cnt--;
        } else {
            s_state = (thdi > EVENT_THDI_HIGH) ? EV_SUSPECT : EV_IDLE;
            s_confirm_cnt = 1U;
        }
        break;

    default:
        s_state = EV_IDLE;
        break;
    }
    return fired;
}

float    EVENT_LastTrigThdi(void) { return s_trig_thdi; }
uint16_t EVENT_LastTrigSeq(void)  { return s_trig_seq; }
