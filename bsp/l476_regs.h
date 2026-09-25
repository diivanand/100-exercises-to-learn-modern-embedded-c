// Register definitions for the STM32L476RG -- the handful of peripherals this
// course touches, written by hand from the reference manual (RM0351) and the
// ARMv7-M architecture manual, in exactly the style chapter 11 teaches:
//
//   - one struct per peripheral, `volatile uint32_t` per register, in
//     declaration order with explicit reserved gaps;
//   - a _Static_assert on every offset that matters, citing the manual, so a
//     miscounted gap is a compile error and not a Saturday;
//   - masks and positions as plain macros, named as the manual names them.
//
// The vendor's stm32l476xx.h does the same thing for every peripheral on the
// die in ~20,000 lines. Use it in production -- after this course you will
// know exactly what is in it.

#ifndef BSP_L476_REGS_H
#define BSP_L476_REGS_H

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

// --- RCC: reset and clock control (RM0351 ch. 6) -----------------------------
//
// Nothing on this die moves until RCC gates its clock on. Forgetting the
// enable bit -- and forgetting to READ IT BACK before touching the newly
// clocked peripheral (errata: the enable takes a cycle to propagate) -- is
// the single most common bring-up bug.

struct rcc_regs {
  volatile uint32_t CR;         // 0x00 clock control
  volatile uint32_t ICSCR;      // 0x04
  volatile uint32_t CFGR;       // 0x08 clock configuration
  volatile uint32_t PLLCFGR;    // 0x0C
  volatile uint32_t PLLSAI1CFGR; // 0x10
  volatile uint32_t PLLSAI2CFGR; // 0x14
  volatile uint32_t CIER;       // 0x18
  volatile uint32_t CIFR;       // 0x1C
  volatile uint32_t CICR;       // 0x20
  uint32_t reserved0;           // 0x24
  volatile uint32_t AHB1RSTR;   // 0x28
  volatile uint32_t AHB2RSTR;   // 0x2C
  volatile uint32_t AHB3RSTR;   // 0x30
  uint32_t reserved1;           // 0x34
  volatile uint32_t APB1RSTR1;  // 0x38
  volatile uint32_t APB1RSTR2;  // 0x3C
  volatile uint32_t APB2RSTR;   // 0x40
  uint32_t reserved2;           // 0x44
  volatile uint32_t AHB1ENR;    // 0x48
  volatile uint32_t AHB2ENR;    // 0x4C  GPIO port clocks live here
  volatile uint32_t AHB3ENR;    // 0x50
  uint32_t reserved3;           // 0x54
  volatile uint32_t APB1ENR1;   // 0x58  USART2, TIM2..7, PWR, DAC, IWDG-adjacent
  volatile uint32_t APB1ENR2;   // 0x5C
  volatile uint32_t APB2ENR;    // 0x60  USART1, SYSCFG, TIM1/8/15..17, ADC? no --
                                //       ADC is on AHB2 on this part (RM0351 6.4.17)
};

#define RCC ((struct rcc_regs *)0x40021000u)

_Static_assert(offsetof(struct rcc_regs, AHB2ENR) == 0x4C, "RM0351 6.4.17");
_Static_assert(offsetof(struct rcc_regs, APB1ENR1) == 0x58, "RM0351 6.4.19");
_Static_assert(offsetof(struct rcc_regs, APB2ENR) == 0x60, "RM0351 6.4.21");

#define RCC_AHB2ENR_GPIOAEN (1u << 0)
#define RCC_AHB2ENR_GPIOBEN (1u << 1)
#define RCC_AHB2ENR_GPIOCEN (1u << 2)
#define RCC_AHB2ENR_ADCEN (1u << 13)
#define RCC_AHB1ENR_DMA1EN (1u << 0)
#define RCC_APB1ENR1_USART2EN (1u << 17)
#define RCC_APB1ENR1_PWREN (1u << 28)
#define RCC_APB2ENR_SYSCFGEN (1u << 0)
#define RCC_APB2ENR_USART1EN (1u << 14)

// --- GPIO (RM0351 ch. 8) ------------------------------------------------------

struct gpio_regs {
  volatile uint32_t MODER;   // 0x00 2 bits/pin: 00 in, 01 out, 10 alt, 11 analog
  volatile uint32_t OTYPER;  // 0x04 0 push-pull, 1 open-drain
  volatile uint32_t OSPEEDR; // 0x08
  volatile uint32_t PUPDR;   // 0x0C 2 bits/pin: 00 none, 01 up, 10 down
  volatile uint32_t IDR;     // 0x10 input data (read-only)
  volatile uint32_t ODR;     // 0x14 output data (read-modify-write -- avoid)
  volatile uint32_t BSRR;    // 0x18 bit set/reset (write-only, atomic -- prefer)
  volatile uint32_t LCKR;    // 0x1C
  volatile uint32_t AFR[2];  // 0x20 alternate function, 4 bits/pin, low then high
  volatile uint32_t BRR;     // 0x28 bit reset
  volatile uint32_t ASCR;    // 0x2C analog switch (L47x only)
};

#define GPIOA ((struct gpio_regs *)0x48000000u)
#define GPIOB ((struct gpio_regs *)0x48000400u)
#define GPIOC ((struct gpio_regs *)0x48000800u)

_Static_assert(offsetof(struct gpio_regs, IDR) == 0x10, "RM0351 8.5.5");
_Static_assert(offsetof(struct gpio_regs, BSRR) == 0x18, "RM0351 8.5.7");
_Static_assert(offsetof(struct gpio_regs, AFR) == 0x20, "RM0351 8.5.9");

// On the NUCLEO-L476RG: LD2 (the green LED) is PA5; B1 (the blue button) is
// PC13, wired to ground when pressed with a pull-up on the board.
#define BSP_LED_PIN 5u
#define BSP_BUTTON_PIN 13u

// --- USART (RM0351 ch. 40) ----------------------------------------------------
//
// USART2 (PA2 = TX, PA3 = RX, alternate function 7) is wired to the ST-Link,
// which presents it to your Mac as a virtual COM port. It is this course's
// stdout: mect prints through it, `./mec listen` reads it.

struct usart_regs {
  volatile uint32_t CR1;  // 0x00 UE, TE, RE, interrupt enables
  volatile uint32_t CR2;  // 0x04 stop bits
  volatile uint32_t CR3;  // 0x08 DMA enables, flow control
  volatile uint32_t BRR;  // 0x0C baud rate: usart_ker_ck / baud, oversampling 16
  volatile uint32_t GTPR; // 0x10
  volatile uint32_t RTOR; // 0x14
  volatile uint32_t RQR;  // 0x18
  volatile uint32_t ISR;  // 0x1C status (read-only)
  volatile uint32_t ICR;  // 0x20 interrupt flag clear (write 1 to clear)
  volatile uint32_t RDR;  // 0x24 receive data (reading clears RXNE)
  volatile uint32_t TDR;  // 0x28 transmit data
};

#define USART1 ((struct usart_regs *)0x40013800u)
#define USART2 ((struct usart_regs *)0x40004400u)

_Static_assert(offsetof(struct usart_regs, BRR) == 0x0C, "RM0351 40.8.4");
_Static_assert(offsetof(struct usart_regs, ISR) == 0x1C, "RM0351 40.8.8");
_Static_assert(offsetof(struct usart_regs, TDR) == 0x28, "RM0351 40.8.11");

#define USART_CR1_UE (1u << 0)
#define USART_CR1_RE (1u << 2)
#define USART_CR1_TE (1u << 3)
#define USART_CR1_RXNEIE (1u << 5)
#define USART_CR1_TCIE (1u << 6)
#define USART_CR1_TXEIE (1u << 7)
#define USART_ISR_RXNE (1u << 5)
#define USART_ISR_TC (1u << 6)
#define USART_ISR_TXE (1u << 7)
#define USART_ICR_TCCF (1u << 6)

// --- SYSCFG + EXTI: pin interrupts (RM0351 ch. 9, 14) --------------------------

struct syscfg_regs {
  volatile uint32_t MEMRMP;    // 0x00
  volatile uint32_t CFGR1;     // 0x04
  volatile uint32_t EXTICR[4]; // 0x08 which PORT feeds each EXTI line
  volatile uint32_t SCSR;      // 0x18
  volatile uint32_t CFGR2;     // 0x1C
  volatile uint32_t SWPR;      // 0x20
  volatile uint32_t SKR;       // 0x24
};

#define SYSCFG ((struct syscfg_regs *)0x40010000u)

_Static_assert(offsetof(struct syscfg_regs, EXTICR) == 0x08, "RM0351 9.2.3");

struct exti_regs {
  volatile uint32_t IMR1;   // 0x00 interrupt mask
  volatile uint32_t EMR1;   // 0x04
  volatile uint32_t RTSR1;  // 0x08 rising edge select
  volatile uint32_t FTSR1;  // 0x0C falling edge select
  volatile uint32_t SWIER1; // 0x10 software trigger
  volatile uint32_t PR1;    // 0x14 pending -- WRITE 1 TO CLEAR (11.04!)
};

#define EXTI ((struct exti_regs *)0x40010400u)

_Static_assert(offsetof(struct exti_regs, PR1) == 0x14, "RM0351 14.5.6");

// --- ADC1 (RM0351 ch. 16) -------------------------------------------------------

struct adc_regs {
  volatile uint32_t ISR;   // 0x00
  volatile uint32_t IER;   // 0x04
  volatile uint32_t CR;    // 0x08 ADEN, ADSTART, ADCAL, and the DEEPPWD trap
  volatile uint32_t CFGR;  // 0x0C
  volatile uint32_t CFGR2; // 0x10
  volatile uint32_t SMPR1; // 0x14 sample time
  volatile uint32_t SMPR2; // 0x18
  uint32_t reserved0;      // 0x1C
  volatile uint32_t TR1;   // 0x20
  volatile uint32_t TR2;   // 0x24
  volatile uint32_t TR3;   // 0x28
  uint32_t reserved1;      // 0x2C
  volatile uint32_t SQR1;  // 0x30 sequence: length and first conversions
  volatile uint32_t SQR2;  // 0x34
  volatile uint32_t SQR3;  // 0x38
  volatile uint32_t SQR4;  // 0x3C
  volatile uint32_t DR;    // 0x40 the result
};

#define ADC1 ((struct adc_regs *)0x50040000u)

_Static_assert(offsetof(struct adc_regs, SQR1) == 0x30, "RM0351 16.6.11");
_Static_assert(offsetof(struct adc_regs, DR) == 0x40, "RM0351 16.6.15");

// Common ADC registers (shared between ADC1/2/3), at 0x300 past ADC1.
struct adc_common_regs {
  volatile uint32_t CSR; // 0x00
  uint32_t reserved0;    // 0x04
  volatile uint32_t CCR; // 0x08 clock mode, VREFEN, TSEN (temperature sensor)
  volatile uint32_t CDR; // 0x0C
};

#define ADC_COMMON ((struct adc_common_regs *)0x50040300u)

#define ADC_CR_ADEN (1u << 0)
#define ADC_CR_ADSTART (1u << 2)
#define ADC_CR_ADCAL (1u << 31)
#define ADC_CR_ADVREGEN (1u << 28)
#define ADC_CR_DEEPPWD (1u << 29)
#define ADC_ISR_ADRDY (1u << 0)
#define ADC_ISR_EOC (1u << 2)
#define ADC_CCR_VREFEN (1u << 22)
#define ADC_CCR_TSEN (1u << 23)

// Factory calibration values, burned into system memory per device
// (datasheet DS10198 sec. 3.15.1). VREFINT was measured at VDDA = 3.0 V.
#define VREFINT_CAL (*(const volatile uint16_t *)0x1FFF75AAu)
#define TS_CAL1 (*(const volatile uint16_t *)0x1FFF75A8u) // at 30 degC, 3.0 V
#define TS_CAL2 (*(const volatile uint16_t *)0x1FFF75CAu) // at 130 degC, 3.0 V

// --- DMA1 (RM0351 ch. 11) -------------------------------------------------------

struct dma_channel_regs {
  volatile uint32_t CCR;   // configuration
  volatile uint32_t CNDTR; // number of data to transfer
  volatile uint32_t CPAR;  // peripheral address
  volatile uint32_t CMAR;  // memory address
  uint32_t reserved0;
};

struct dma_regs {
  volatile uint32_t ISR;                // 0x00 status for all 7 channels
  volatile uint32_t IFCR;               // 0x04 write 1 to clear
  struct dma_channel_regs CH[7];        // 0x08 channels 1..7 (index 0 = CH1)
  uint32_t reserved0[5];                // 0x94
  volatile uint32_t CSELR;              // 0xA8 request routing (L4-specific)
};

#define DMA1 ((struct dma_regs *)0x40020000u)

_Static_assert(offsetof(struct dma_regs, CH) == 0x08, "RM0351 11.6.3");
_Static_assert(offsetof(struct dma_regs, CSELR) == 0xA8, "RM0351 11.6.7");

#define DMA_CCR_EN (1u << 0)
#define DMA_CCR_TCIE (1u << 1)
#define DMA_CCR_DIR_FROM_MEM (1u << 4)
#define DMA_CCR_MINC (1u << 7)

// --- IWDG: independent watchdog (RM0351 ch. 32) ---------------------------------

struct iwdg_regs {
  volatile uint32_t KR;  // 0x00 key: 0xCCCC start, 0xAAAA feed, 0x5555 unlock
  volatile uint32_t PR;  // 0x04 prescaler
  volatile uint32_t RLR; // 0x08 reload
  volatile uint32_t SR;  // 0x0C status
  volatile uint32_t WINR; // 0x10
};

#define IWDG ((struct iwdg_regs *)0x40003000u)

#define IWDG_KEY_START 0xCCCCu
#define IWDG_KEY_FEED 0xAAAAu
#define IWDG_KEY_UNLOCK 0x5555u

// --- Cortex-M4 core peripherals (ARMv7-M, not RM0351) ---------------------------

struct systick_regs {
  volatile uint32_t CSR;   // 0x00 enable, tick interrupt, clock source, COUNTFLAG
  volatile uint32_t RVR;   // 0x04 reload value (24-bit)
  volatile uint32_t CVR;   // 0x08 current value (write to clear)
  volatile uint32_t CALIB; // 0x0C
};

#define SYSTICK ((struct systick_regs *)0xE000E010u)

#define SYSTICK_CSR_ENABLE (1u << 0)
#define SYSTICK_CSR_TICKINT (1u << 1)
#define SYSTICK_CSR_CLKSOURCE_CPU (1u << 2)
#define SYSTICK_CSR_COUNTFLAG (1u << 16)

// NVIC: one enable/pending bit per IRQ number, 32 per register.
struct nvic_regs {
  volatile uint32_t ISER[8]; // 0xE000E100 set-enable
  uint32_t reserved0[24];
  volatile uint32_t ICER[8]; // 0xE000E180 clear-enable
  uint32_t reserved1[24];
  volatile uint32_t ISPR[8]; // 0xE000E200 set-pending
  uint32_t reserved2[24];
  volatile uint32_t ICPR[8]; // 0xE000E280 clear-pending
};

#define NVIC ((struct nvic_regs *)0xE000E100u)

_Static_assert(offsetof(struct nvic_regs, ICER) == 0x80, "ARMv7-M B3.4.4");
_Static_assert(offsetof(struct nvic_regs, ISPR) == 0x100, "ARMv7-M B3.4.5");

static inline void nvic_enable_irq(uint32_t irqn) {
  NVIC->ISER[irqn / 32u] = 1u << (irqn % 32u); // write-1-to-set: no RMW needed
}

// IRQ numbers this course uses (RM0351 table 58).
#define IRQN_DMA1_CH7 17u
#define IRQN_ADC1_2 18u
#define IRQN_TIM2 28u
#define IRQN_USART1 37u
#define IRQN_USART2 38u
#define IRQN_EXTI15_10 40u

// System control block: just the members the course touches.
struct scb_regs {
  volatile uint32_t CPUID; // 0x00
  volatile uint32_t ICSR;  // 0x04
  volatile uint32_t VTOR;  // 0x08 vector table offset
  volatile uint32_t AIRCR; // 0x0C
  volatile uint32_t SCR;   // 0x10 sleep configuration
  volatile uint32_t CCR;   // 0x14
  volatile uint8_t SHPR[12]; // 0x18 system handler priorities
  volatile uint32_t SHCSR; // 0x24
};

#define SCB ((struct scb_regs *)0xE000ED00u)
#define SCB_CPACR (*(volatile uint32_t *)0xE000ED88u) // FPU access control

_Static_assert(offsetof(struct scb_regs, VTOR) == 0x08, "ARMv7-M B3.2.5");
_Static_assert(offsetof(struct scb_regs, SHCSR) == 0x24, "ARMv7-M B3.2.13");

// --- interrupt control ----------------------------------------------------------

static inline void irq_disable(void) {
  __asm volatile("cpsid i" ::: "memory");
}

static inline void irq_enable(void) {
  __asm volatile("cpsie i" ::: "memory");
}

static inline void wait_for_interrupt(void) {
  __asm volatile("wfi");
}

#endif // BSP_L476_REGS_H
