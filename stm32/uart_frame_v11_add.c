/* uart_frame_v11_add.c — v1.1 协议增量补丁（说明用节选, 完整实现已合入 uart_frame.c/.h）
 *
 * 协议 v1.0 -> v1.1 的兼容性设计(答辩素材):
 *  - 只新增帧类型, 不改动任何既有帧的字段布局 -> 旧上位机遇到未知 TYPE
 *    走"按长度跳过"分支, 天然向后兼容;
 *  - 新增数据全部放在 PAYLOAD 内, LEN 字段自描述 -> 不需要协议大版本升级;
 *  - TIME/EVENT 均可被忙时丢弃( transmit 的宁丢不阻塞策略不变)。
 */

/* uart_frame.h 增量: -------------------------------------------------
 * #define FRAME_TYPE_TIME    0x04U   对时帧
 * #define FRAME_TYPE_EVENT   0x05U   事件帧
 * uint8_t UART_SendTime(uint32_t utc);
 * uint8_t UART_SendEvent(uint32_t utc, float thdi_trig, float irms,
 *                        uint16_t seq, uint8_t locked);
 * ------------------------------------------------------------------- */

/* uart_frame.c 增量: */
uint8_t UART_SendTime(uint32_t utc)
{
    uint8_t payload[4];
    memcpy(payload, &utc, 4U);            /* Cortex-M 小端, 与协议一致 */
    uint32_t n = build_frame(s_txbuf[0], FRAME_TYPE_TIME, payload, 4U);
    return transmit(s_txbuf[0], n);
}

uint8_t UART_SendEvent(uint32_t utc, float thdi_trig, float irms,
                       uint16_t seq, uint8_t locked)
{
    uint8_t payload[16];
    memcpy(&payload[0],  &utc, 4U);
    memcpy(&payload[4],  &thdi_trig, 4U);
    memcpy(&payload[8],  &irms, 4U);
    payload[12] = (uint8_t)(seq & 0xFFU);
    payload[13] = (uint8_t)(seq >> 8);
    payload[14] = locked;
    payload[15] = 0U;                     /* 保留 */
    uint32_t n = build_frame(s_txbuf[0], FRAME_TYPE_EVENT, payload, 16U);
    return transmit(s_txbuf[0], n);
}

/* main.c 集成(分析完成后, 约4行):
 *   if (EVENT_OnResult(s_result.thdi, s_result.irms, s_result.locked,
 *                      (uint16_t)s_win_cnt, utc_now)) {
 *       UART_SendEvent(utc_now, EVENT_LastTrigThdi(), s_result.irms,
 *                      (uint16_t)s_win_cnt, s_result.locked);
 *   }
 * 对时: RTC 读取函数每 10s 调用一次 UART_SendTime(rtc_get_utc()); */
