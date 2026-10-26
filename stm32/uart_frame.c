/**
 * uart_frame.c — 串口成帧 + CRC16 + DMA 发送
 *
 * 可靠性设计:
 *  1) 所有 float 以小端打包(与 x86/ARM Linux 原生一致, 上位机零转换);
 *  2) DMA 发送 + 忙标志: 上一帧未发完时直接丢弃本帧并计数(宁丢帧不阻塞采集);
 *  3) SEQ 字节循环递增, 上位机可据此统计丢帧率。
 * 带宽预算: RESULT 帧 = (10 + 100) x 4 + 8 = 448 B, 每 80ms 一帧 -> 约 45kbps,
 *          远小于 921600bps x 0.8 有效带宽, 余量 > 15 倍。
 */
#include "uart_frame.h"
#include "pq_config.h"
#include <string.h>
#include <stdlib.h>

#define RESULT_FLOATS   (10U + 2U * PQ_HARM_MAX)
#define TX_BUF_MAX      (8U + RESULT_FLOATS * 4U)

static UART_HandleTypeDef s_huart;
static DMA_HandleTypeDef  s_hdma_tx;
static uint8_t  s_txbuf[2][TX_BUF_MAX];   /* 双缓冲: 一帧在发, 一帧在组 */
static volatile uint8_t s_busy = 0;
static uint8_t  s_seq = 0;
static uint32_t s_drop = 0, s_frames = 0;

/* ---------------- CRC16-MODBUS (逐位实现, 无查表表空间) ---------------- */
static uint16_t crc16_modbus(const uint8_t *data, uint32_t len)
{
    uint16_t crc = 0xFFFFU;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8U; b++)
            crc = (crc & 1U) ? (uint16_t)((crc >> 1) ^ 0xA001U) : (uint16_t)(crc >> 1);
    }
    return crc;   /* 发送时低字节在前 */
}

/* ---------------- 底层: USART2 PA2(TX)/PA3(RX), TX 走 DMA1_Stream6 ---------------- */
HAL_StatusTypeDef UART_Init(uint32_t baud)
{
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = GPIO_PIN_2;                    /* TX 复用推挽 */
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &gpio);

    s_huart.Instance          = USART2;
    s_huart.Init.BaudRate     = baud;
    s_huart.Init.WordLength   = UART_WORDLENGTH_8B;
    s_huart.Init.StopBits     = UART_STOPBITS_1;
    s_huart.Init.Parity       = UART_PARITY_NONE;
    s_huart.Init.Mode         = UART_MODE_TX_RX;
    s_huart.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    s_huart.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&s_huart) != HAL_OK) return HAL_ERROR;

    s_hdma_tx.Instance                 = DMA1_Stream6;
    s_hdma_tx.Init.Channel             = DMA_CHANNEL_4;
    s_hdma_tx.Init.Direction           = DMA_MEMORY_TO_PERIPH;
    s_hdma_tx.Init.PeriphInc           = DMA_PINC_DISABLE;
    s_hdma_tx.Init.MemInc              = DMA_MINC_ENABLE;
    s_hdma_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    s_hdma_tx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    s_hdma_tx.Init.Mode                = DMA_NORMAL;
    s_hdma_tx.Init.Priority            = DMA_PRIORITY_MEDIUM;
    if (HAL_DMA_Init(&s_hdma_tx) != HAL_OK) return HAL_ERROR;
    __HAL_LINKDMA(&s_huart, hdmatx, s_hdma_tx);

    HAL_NVIC_SetPriority(USART2_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
    return HAL_OK;
}

/* 组帧: payload 为原始字节, 返回帧总长 */
static uint32_t build_frame(uint8_t *buf, uint8_t type,
                            const uint8_t *payload, uint16_t len)
{
    buf[0] = FRAME_SYNC0;
    buf[1] = FRAME_SYNC1;
    buf[2] = type;
    buf[3] = (uint8_t)(len & 0xFFU);          /* LEN 小端 */
    buf[4] = (uint8_t)(len >> 8);
    buf[5] = s_seq++;
    memcpy(&buf[6], payload, len);
    uint16_t crc = crc16_modbus(&buf[2], (uint32_t)len + 4U);  /* TYPE..PAYLOAD */
    buf[6 + len]     = (uint8_t)(crc & 0xFFU);
    buf[6 + len + 1U] = (uint8_t)(crc >> 8);
    return (uint32_t)len + 8U;
}

static uint8_t transmit(const uint8_t *buf, uint32_t n)
{
    if (s_busy) { s_drop++; return 1; }       /* 上一帧未发完: 丢弃, 不阻塞采集链 */
    s_busy = 1;
    s_frames++;
    memcpy(s_txbuf[1], buf, n);               /* 拷到发送缓冲, 源缓冲立即可复用 */
    HAL_UART_Transmit_DMA(&s_huart, s_txbuf[1], (uint16_t)n);
    return 0;
}

uint8_t UART_SendResult(const PqResult *r)
{
    uint8_t payload[TX_BUF_MAX];
    float  *f = (float *)payload;             /* ARM Cortex-M4 小端, 直接内存布局 */
    uint32_t k = 0;
    f[k++] = r->freq;   f[k++] = r->irms;  f[k++] = r->urms;
    f[k++] = r->p;      f[k++] = r->q;     f[k++] = r->s;
    f[k++] = r->pf;     f[k++] = r->dpf;
    f[k++] = r->thdi;   f[k++] = r->thdu;
    for (uint32_t h = 1U; h <= PQ_HARM_MAX; h++) f[k++] = r->i_harm[h];
    for (uint32_t h = 1U; h <= PQ_HARM_MAX; h++) f[k++] = r->u_harm[h];

    uint32_t n = build_frame(s_txbuf[0], FRAME_TYPE_RESULT,
                             payload, (uint16_t)(RESULT_FLOATS * 4U));
    return transmit(s_txbuf[0], n);
}

void UART_SendWave(const int16_t *iu, uint32_t n_halfwords)
{
    uint32_t n = build_frame(s_txbuf[0], FRAME_TYPE_WAVE,
                             (const uint8_t *)iu, (uint16_t)(n_halfwords * 2U));
    (void)transmit(s_txbuf[0], n);
}

void UART_SendHello(void)
{
    uint8_t payload[8] = {1, 4, 7, 0,   /* 协议版本1.4.7 */
                          (uint8_t)(PQ_HARM_MAX), (uint8_t)PQ_FFT_LEN >> 8,
                          (uint8_t)(PQ_FFT_LEN & 0xFFU), (uint8_t)PQ_CYCLES_PER_WIN};
    uint32_t n = build_frame(s_txbuf[0], FRAME_TYPE_HELLO, payload, 8U);
    (void)transmit(s_txbuf[0], n);
}

uint32_t UART_GetTxDropCount(void)  { return s_drop; }
uint32_t UART_GetTxFrameCount(void) { return s_frames; }

/* ---------------- v1.1 新增: 对时帧与事件帧 ---------------- */
/* 对时帧: 设备周期性(如每10s)上报自身UTC秒, 上位机比对偏差并记录;
 * 上位机 CSV 因此可以携带双时间戳: 设备发生时刻 + 主机接收时刻 */
uint8_t UART_SendTime(uint32_t utc)
{
    uint8_t payload[4];
    memcpy(payload, &utc, 4U);            /* Cortex-M 小端, 与协议一致 */
    uint32_t n = build_frame(s_txbuf[0], FRAME_TYPE_TIME, payload, 4U);
    return transmit(s_txbuf[0], n);
}

/* 事件帧(元数据16B): utc + 触发THDi + 触发Irms + 窗口序号 + 锁定态
 * 波形不塞进本帧, 紧随其后发送 2 帧 WAVE(事件前窗+触发窗),
 * 大小帧分离让每个帧都在既有缓冲上限内, 协议向后兼容 */
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

/* ---------------- HAL 回调/中断 ---------------- */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) s_busy = 0;
}
void USART2_IRQHandler(void) { HAL_UART_IRQHandler(&s_huart); }
void DMA1_Stream6_IRQHandler(void) { HAL_DMA_IRQHandler(&s_hdma_tx); }
