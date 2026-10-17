/*
 *  REMOTE  —  STM32F103C8T6 @ 72 MHz  —  bare metal, no HAL
 *
 *  PA0 = UP    PA1 = DOWN   PA2 = LEFT   PA3 = RIGHT  (active low, pull-up)
 *  PA4 = POWER toggle (active low)
 *  PC13 = LED (active low)
 *  PA9  = USART1_TX -> HC-12 RXD      (9600 8N1)
 *
 *  Sends one char over HC-12 while a direction is held:
 *     'U' up  'D' down  'L' left  'R' right  'S' stop
 */

#include <stdint.h>

/* ================= Register map ================= */
#define RCC_BASE     0x40021000UL
#define RCC_CR       (*(volatile uint32_t*)(RCC_BASE + 0x00))
#define RCC_CFGR     (*(volatile uint32_t*)(RCC_BASE + 0x04))
#define RCC_APB2ENR  (*(volatile uint32_t*)(RCC_BASE + 0x18))

#define FLASH_ACR    (*(volatile uint32_t*)0x40022000UL)

#define GPIOA_BASE   0x40010800UL
#define GPIOA_CRL    (*(volatile uint32_t*)(GPIOA_BASE + 0x00))
#define GPIOA_CRH    (*(volatile uint32_t*)(GPIOA_BASE + 0x04))
#define GPIOA_IDR    (*(volatile uint32_t*)(GPIOA_BASE + 0x08))
#define GPIOA_ODR    (*(volatile uint32_t*)(GPIOA_BASE + 0x0C))
#define GPIOA_BSRR   (*(volatile uint32_t*)(GPIOA_BASE + 0x10))

#define GPIOC_BASE   0x40011000UL
#define GPIOC_CRH    (*(volatile uint32_t*)(GPIOC_BASE + 0x04))
#define GPIOC_BSRR   (*(volatile uint32_t*)(GPIOC_BASE + 0x10))

#define USART1_BASE  0x40013800UL
#define USART1_SR    (*(volatile uint32_t*)(USART1_BASE + 0x00))
#define USART1_DR    (*(volatile uint32_t*)(USART1_BASE + 0x04))
#define USART1_BRR   (*(volatile uint32_t*)(USART1_BASE + 0x08))
#define USART1_CR1   (*(volatile uint32_t*)(USART1_BASE + 0x0C))

#define STK_CTRL     (*(volatile uint32_t*)0xE000E010UL)
#define STK_LOAD     (*(volatile uint32_t*)0xE000E014UL)
#define STK_VAL      (*(volatile uint32_t*)0xE000E018UL)

/* ================= SysTick 1 ms ================= */
static volatile uint32_t g_ticks = 0;
void SysTick_Handler(void) { g_ticks++; }

static void delay_ms(uint32_t ms) {
    uint32_t s = g_ticks;
    while ((g_ticks - s) < ms) { __asm volatile ("nop"); }
}

/* ================= Clocks: HSE 8 MHz -> 72 MHz ================= */
static void clock_init(void) {
    RCC_CR |= (1u << 16);                                   /* HSE ON        */
    while (!(RCC_CR & (1u << 17))) { }                      /* wait HSERDY   */

    FLASH_ACR = (1u << 4) | 2u;                             /* prefetch + 2WS*/

    RCC_CFGR = (4u << 8) | (1u << 16) | (7u << 18);         /* AHB/1 APB1/2 APB2/1 PLL=HSE x9 */
    RCC_CR  |= (1u << 24);                                  /* PLL ON        */
    while (!(RCC_CR & (1u << 25))) { }

    RCC_CFGR = (RCC_CFGR & ~0x3u) | 2u;                     /* SW = PLL      */
    while ((RCC_CFGR & 0xCu) != 0x8u) { }                   /* wait SWS      */
}

/* ================= GPIO ================= */
static void gpio_init(void) {
    RCC_APB2ENR |= (1u << 2) | (1u << 4);                   /* GPIOA | GPIOC */

    /* PA0..PA4 : input + pull-up  (CNF=10 MODE=00 -> 0x8 each) */
    GPIOA_CRL &= ~0x000FFFFFu;
    GPIOA_CRL |=  0x00088888u;
    GPIOA_ODR |=  0x0000001Fu;                              /* ODR=1 -> pull-up */

    /* PA9 : AF push-pull 50 MHz (MODE=11 CNF=10 -> 0xB) */
    GPIOA_CRH &= ~(0xFu << 4);
    GPIOA_CRH |=  (0xBu << 4);

    /* PC13 : output push-pull 2 MHz (MODE=10 CNF=00 -> 0x2) */
    GPIOC_CRH &= ~(0xFu << 20);
    GPIOC_CRH |=  (0x2u << 20);
    GPIOC_BSRR = (1u << 13);                                /* LED off       */
}

/* ================= USART1 (TX only) ================= */
static void usart1_init(void) {
    RCC_APB2ENR |= (1u << 14);
    USART1_BRR = 7500u;                                     /* 72e6/9600     */
    USART1_CR1 = (1u << 13) | (1u << 3);                    /* UE | TE       */
}

static void uart_tx(char c) {
    while (!(USART1_SR & (1u << 7))) { }                    /* wait TXE      */
    USART1_DR = (uint32_t)c;
}

/* ================= main ================= */
int main(void) {
    clock_init();

    STK_LOAD = 72000u - 1u;                                 /* 1 ms tick     */
    STK_VAL  = 0;
    STK_CTRL = 7u;                                          /* ENABLE|TICKINT|CORE */

    gpio_init();
    usart1_init();

    uint8_t  remoteOn = 0;
    uint8_t  pwrPrev  = 1;
    uint32_t lastSend = 0;

    while (1) {
        /* ---- power button: falling-edge detect ---- */
        uint8_t pwrNow = (uint8_t)((GPIOA_IDR >> 4) & 1u);

        if (pwrNow == 0 && pwrPrev == 1) {
            delay_ms(30);                                   /* debounce */
            if (!((GPIOA_IDR >> 4) & 1u)) {
                remoteOn = !remoteOn;

                if (remoteOn)
                    GPIOC_BSRR = (1u << (13 + 16));         /* PC13 LOW  -> LED ON  */
                else {
                    GPIOC_BSRR = (1u << 13);                /* PC13 HIGH -> LED OFF */
                    uart_tx('S');                           /* stop servos          */
                }
            }
        }
        pwrPrev = pwrNow;

        /* ---- direction: resend every 50 ms while remote is ON ---- */
        if (remoteOn && (g_ticks - lastSend >= 50u)) {
            lastSend = g_ticks;

            uint8_t b = (uint8_t)(GPIOA_IDR & 0x0Fu);

            if      (!(b & 0x1u)) uart_tx('L');
            else if (!(b & 0x2u)) uart_tx('D');
            else if (!(b & 0x4u)) uart_tx('R');
            else if (!(b & 0x8u)) uart_tx('U');
            else                  uart_tx('S');             /* nothing pressed -> stop */
        }

        delay_ms(5);
    }
}

/* Stub — only needed if CubeIDE generated syscalls.c */
int _write(int file, char *ptr, int len) { (void)file; (void)ptr; return len; }
