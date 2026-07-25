/**
 * @file    main.c
 * @brief   Simple bare-metal LED blink for STM32WB55 (CMSIS register access).
 *
 * Toggles the on-board user LED on the WeAct STM32WB55 Core Board (PE4,
 * active high) — adjust LED_PORT / LED_PIN below for other hardware.
 */

#include "stm32wbxx.h"
#include "stdint.h"
#include "communication_protocol.h"

#define LED_PORT GPIOE
#define LED_PIN 4U

static void delay(volatile uint32_t count)
{
  while (count--)
  {
    __NOP();
  }
}

void system_init(void);

int main(void)
{
  system_init();
  communication_init();
  while (1)
  {
    LED_PORT->ODR ^= (1U << LED_PIN);
    delay(1000000U);
  }
}

void system_init(void)
{
  /* ---- 1. HSI16 ---- */
  RCC->CR |= RCC_CR_HSION;
  while (!(RCC->CR & RCC_CR_HSIRDY));

  /* ---- 2. PLL → 64 МГц ---- */
  RCC->CR &= ~RCC_CR_PLLON;
  while (RCC->CR & RCC_CR_PLLRDY);

  RCC->PLLCFGR = (2U << RCC_PLLCFGR_PLLSRC_Pos)
               | (1U << RCC_PLLCFGR_PLLM_Pos)   /* M=2 */
               | (16U << RCC_PLLCFGR_PLLN_Pos)   /* N=16 */
               | (1U << RCC_PLLCFGR_PLLR_Pos)    /* R=2 */
               | RCC_PLLCFGR_PLLREN;

  RCC->CR |= RCC_CR_PLLON;
  while (!(RCC->CR & RCC_CR_PLLRDY));

  /* ---- 3. Flash latency 3WS ---- */
  FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY) | (3U << FLASH_ACR_LATENCY_Pos);
  while ((FLASH->ACR & FLASH_ACR_LATENCY) != (3U << FLASH_ACR_LATENCY_Pos));

  /* ---- 4. SYSCLK → PLL ---- */
  RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | (3U << RCC_CFGR_SW_Pos);
  while ((RCC->CFGR & RCC_CFGR_SWS) != (3U << RCC_CFGR_SWS_Pos));

  SystemCoreClock = 64000000U;

  /* ---- 5. GPIO ---- */
  RCC->AHB2ENR |= RCC_AHB2ENR_GPIOEEN | RCC_AHB2ENR_GPIOAEN;

  LED_PORT->MODER &= ~(3U << (LED_PIN * 2U));
  LED_PORT->MODER |=  (1U << (LED_PIN * 2U));

  /* ---- 6. PA9 TX → AF7 ---- */
  GPIOA->MODER  &= ~(3U << 18); GPIOA->MODER  |= (2U << 18);
  GPIOA->AFR[1] &= ~(0xFU << 4); GPIOA->AFR[1] |= (7U << 4);

  /* ---- 7. PA10 RX → AF7, pull-up ---- */
  GPIOA->MODER  &= ~(3U << 20); GPIOA->MODER  |= (2U << 20);
  GPIOA->PUPDR  &= ~(3U << 20); GPIOA->PUPDR  |= (1U << 20);
  GPIOA->AFR[1] &= ~(0xFU << 8); GPIOA->AFR[1] |= (7U << 8);

  /* ---- 8. USART1 @ 64 МГц / 115200 ---- */
  RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
  USART1->BRR = 555U;
  USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_UE;


  RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;

  TIM2->PSC = 65 - 1;       
  TIM2->ARR = 1000 - 1;
  TIM2->DIER |= TIM_DIER_UIE; 

  TIM2->CR1 |= TIM_CR1_CEN;


  NVIC_SetPriority(USART1_IRQn, 2);
  NVIC_EnableIRQ(USART1_IRQn);
  
  NVIC_SetPriority(TIM2_IRQn, 0); 
  NVIC_EnableIRQ(TIM2_IRQn);
}

void TIM2_IRQHandler(void)
{
    if (TIM2->SR & TIM_SR_UIF)
    {
     communication_handle_timeout();
      
     TIM2->SR &= ~TIM_SR_UIF;
    }
}
