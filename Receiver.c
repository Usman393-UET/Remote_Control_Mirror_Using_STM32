/*
 *  RECEIVER — STM32F103C8 @ 72 MHz — bare metal, no HAL
 *
 *  PA0  = TIM2_CH1 -> Servo X (pan)     stop = 1480 us
 *  PA1  = TIM2_CH2 -> Servo Y (tilt)    stop = 1510 us
 *  PA10 = USART1_RX  <- HC-12 TXD       (9600 8N1)
 *  PA2  = USART2_TX  -> USB-TTL RX      (9600 8N1  debug)
 *
 *  Commands:  'U' tilt+  'D' tilt-  'L' pan-  'R' pan+  'S' stop
 *  Only the commanded axis moves; the other stays stopped.
 */

#include <stdint.h>

/* ================= Register map ================= */
#define RCC_BASE     0x40021000UL
#define RCC_CR       (*(volatile uint32_t*)(RCC_BASE + 0x00))
#define RCC_CFGR     (*(volatile uint32_t*)(RCC_BASE + 0x04))
#define RCC_APB2ENR  (*(volatile uint32_t*)(RCC_BASE + 0x18))
#define RCC_APB1ENR  (*(volatile uint32_t*)(RCC_BASE + 0x1C))

#define FLASH_ACR    (*(volatile uint32_t*)0x40022000UL)

#define GPIOA_BASE   0x40010800UL
#define GPIOA_CRL    (*(volatile uint32_t*)(GPIOA_BASE + 0x00))
#define GPIOA_CRH    (*(volatile uint32_t*)(GPIOA_BASE + 0x04))

/* USART1 = HC-12 */
#define USART1_BASE  0x40013800UL
#define USART1_SR    (*(volatile uint32_t*)(USART1_BASE + 0x00))
#define USART1_DR    (*(volatile uint32_t*)(USART1_BASE + 0x04))
#define USART1_BRR   (*(volatile uint32_t*)(USART1_BASE + 0x08))
#define USART1_CR1   (*(volatile uint32_t*)(USART1_BASE + 0x0C))

/* USART2 = debug via USB-TTL */
#define USART2_BASE  0x40004400UL
#define USART2_SR    (*(volatile uint32_t*)(USART2_BASE + 0x00))
#define USART2_DR    (*(volatile uint32_t*)(USART2_BASE + 0x04))
#define USART2_BRR   (*(volatile uint32_t*)(USART2_BASE + 0x08))
#define USART2_CR1   (*(volatile uint32_t*)(USART2_BASE + 0x0C))

/* TIM2 = servo PWM */
#define TIM2_BASE    0x40000000UL
#define TIM2_CR1     (*(volatile uint32_t*)(TIM2_BASE + 0x00))
#define TIM2_CCMR1   (*(volatile uint32_t*)(TIM2_BASE + 0x18))
#define TIM2_CCER    (*(volatile uint32_t*)(TIM2_BASE + 0x20))
#define TIM2_PSC     (*(volatile uint32_t*)(TIM2_BASE + 0x28))
#define TIM2_ARR     (*(volatile uint32_t*)(TIM2_BASE + 0x2C))
#define TIM2_CCR1    (*(volatile uint32_t*)(TIM2_BASE + 0x34))
#define TIM2_CCR2    (*(volatile uint32_t*)(TIM2_BASE + 0x38))

/* ================= Servo tuning ================= */
#define X_STOP   1480u          /* pan  stop  (us)  */
#define Y_STOP   1510u          /* tilt stop  (us)  */
#define SPEED    300u           /* offset from stop — bigger = faster */

/* ================= Clocks ================= */
static void clock_init(void) {
    RCC_CR |= (1u << 16);
    while (!(RCC_CR & (1u << 17))) { }
    FLASH_ACR = (1u << 4) | 2u;
    RCC_CFGR = (4u << 8) | (1u << 16) | (7u << 18);
    RCC_CR  |= (1u << 24);
    while (!(RCC_CR & (1u << 25))) { }
    RCC_CFGR = (RCC_CFGR & ~0x3u) | 2u;
    while ((RCC_CFGR & 0xCu) != 0x8u) { }
}

/* ================= TIM2: 50 Hz PWM, 1 us resolution =================
 *  Timer clk = 72 MHz (APB1 div 2, timer clk doubled)
 *  PSC = 71    -> 1 MHz   (1 count = 1 us)
 *  ARR = 19999 -> 20 ms   (50 Hz)
 *  CCRx = pulse width in microseconds
 */
static void tim2_init(void) {
    RCC_APB2ENR |= (1u << 2);           /* GPIOA */
    RCC_APB1ENR |= (1u << 0);           /* TIM2  */

    /* PA0, PA1 : AF push-pull 50 MHz -> 0xB each */
    GPIOA_CRL &= ~0x000000FFu;
    GPIOA_CRL |=  (0xBu << 0) | (0xBu << 4);

    TIM2_PSC  = 72u - 1u;
    TIM2_ARR  = 20000u - 1u;
    TIM2_CCR1 = X_STOP;
    TIM2_CCR2 = Y_STOP;

    /* PWM mode 1 + preload on CH1 and CH2 -> 0x68 each */
    TIM2_CCMR1 = (0x68u << 0) | (0x68u << 8);

    /* enable CH1, CH2 */
    TIM2_CCER  = (1u << 0) | (1u << 4);

    /* ARPE=1, CEN=1 */
    TIM2_CR1   = (1u << 7) | (1u << 0);
}

/* ================= USART1 (RX only, HC-12) ================= */
static void usart1_init(void) {
    RCC_APB2ENR |= (1u << 14);

    GPIOA_CRH &= ~(0xFu << 4);              /* PA9  : AF PP 50 MHz */
    GPIOA_CRH |=  (0xBu << 4);

    GPIOA_CRH &= ~(0xFu << 8);              /* PA10 : input floating */
    GPIOA_CRH |=  (0x4u << 8);

    USART1_BRR = 7500u;                     /* 72e6 / 9600 */
    USART1_CR1 = (1u << 13) | (1u << 2);    /* UE | RE */
}

/* ================= USART2 (TX only, debug) ================= */
static void usart2_init(void) {
    RCC_APB2ENR |= (1u << 2);               /* GPIOA */
    RCC_APB1ENR |= (1u << 17);              /* USART2 */

    GPIOA_CRL &= ~(0xFu << 8);              /* PA2 : AF PP 50 MHz */
    GPIOA_CRL |=  (0xBu << 8);

    USART2_BRR = 3750u;                     /* 36e6 / 9600 */
    USART2_CR1 = (1u << 13) | (1u << 3);    /* UE | TE */
}

static void dbg_char(char c) {
    while (!(USART2_SR & (1u << 7))) { }
    USART2_DR = (uint32_t)c;
}
static void dbg_str(const char *s) { while (*s) dbg_char(*s++); }

/* ================= main ================= */
int main(void) {
    clock_init();
    tim2_init();
    usart1_init();
    usart2_init();

    dbg_str("RECEIVER READY\r\n");

    uint32_t xPulse = X_STOP;
    uint32_t yPulse = Y_STOP;

    while (1) {
        if (USART1_SR & (1u << 5)) {                    /* RXNE */
            char c = (char)(USART1_DR & 0xFFu);

            switch (c) {
                /* tilt: Y moves, X stays stopped */
                case 'U': xPulse = X_STOP;         yPulse = Y_STOP + SPEED; break;
                case 'D': xPulse = X_STOP;         yPulse = Y_STOP - SPEED; break;
                /* pan: X moves, Y stays stopped */
                case 'L': xPulse = X_STOP - SPEED; yPulse = Y_STOP;         break;
                case 'R': xPulse = X_STOP + SPEED; yPulse = Y_STOP;         break;
                /* stop everything */
                case 'S':
                default : xPulse = X_STOP;         yPulse = Y_STOP;         break;
            }

            TIM2_CCR1 = xPulse;
            TIM2_CCR2 = yPulse;

            dbg_char(c);
            dbg_str("\r\n");
        }
    }
}

/* Stub — only needed if CubeIDE generated syscalls.c */
int _write(int file, char *ptr, int len) { (void)file; (void)ptr; return len; }
