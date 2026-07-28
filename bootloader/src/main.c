#include "bootloader.h"
#include "bootloader_config.h"
#include "communication_protocol.h"
#include "stm32wbxx.h"
#include "usb_cdc_bootloader.h"

#define LED_PORT GPIOE
#define LED_PIN 4U

static void system_init(void);
static void wait_to_boot_mode(void);
static void enter_to_boot_mode(void);
static void handle_boot_timeout(void);

void system_deinit(void);

static void delay(volatile uint32_t count)
{
  while (count--)
  {
    __NOP();
  }
}

static uint8_t boot_mode = 0;

int main(void)
{
  system_init();

  LED_PORT->ODR |= (1U << LED_PIN);

  boot_mode = 1;

  bootloader_init();
  communication_init();
  usb_cdc_bootloader_init();

  while (1)
  {
    usb_cdc_bootloader_task();
    communication_application();
    // @todo implement the main loop of the bootloader here
  }
}

static void enter_to_boot_mode(void)
{
  if (!boot_mode)
  {
    bootloader_jump_to_application();
  }
}

void system_init(void)
{
  /* ---- 1. HSI16 ---- */
  RCC->CR |= RCC_CR_HSION;
  while (!(RCC->CR & RCC_CR_HSIRDY))
    ;

  /* ---- 2. PLL → 64 МГц ---- */
  RCC->CR &= ~RCC_CR_PLLON;
  while (RCC->CR & RCC_CR_PLLRDY)
    ;

  RCC->PLLCFGR = (2U << RCC_PLLCFGR_PLLSRC_Pos) | (1U << RCC_PLLCFGR_PLLM_Pos) /* M=2 */
                 | (16U << RCC_PLLCFGR_PLLN_Pos)                               /* N=16 */
                 | (1U << RCC_PLLCFGR_PLLR_Pos)                                /* R=2 */
                 | RCC_PLLCFGR_PLLREN;

  (void)RCC->APB1ENR1;     // dummy read для синхронізації
  PWR->CR2 |= PWR_CR2_USV; // USB supply valid

  RCC->APB1ENR1 |= RCC_APB1ENR1_CRSEN;
  CRS->CR |= CRS_CR_AUTOTRIMEN | CRS_CR_CEN;

  RCC->CR |= RCC_CR_PLLON;
  while (!(RCC->CR & RCC_CR_PLLRDY))
    ;

  /* ---- 3. Flash latency 3WS ---- */
  FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY) | (3U << FLASH_ACR_LATENCY_Pos);
  while ((FLASH->ACR & FLASH_ACR_LATENCY) != (3U << FLASH_ACR_LATENCY_Pos))
    ;

  /* ---- 4. SYSCLK → PLL ---- */
  RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | (3U << RCC_CFGR_SW_Pos);
  while ((RCC->CFGR & RCC_CFGR_SWS) != (3U << RCC_CFGR_SWS_Pos))
    ;

  SystemCoreClock = 64000000U;

  /* ---- 5. GPIO ---- */
  RCC->AHB2ENR |= RCC_AHB2ENR_GPIOEEN | RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOHEN;

  LED_PORT->MODER &= ~(3U << (LED_PIN * 2U));
  LED_PORT->MODER |= (1U << (LED_PIN * 2U));

  /* PH3: floating input (MODER = 00b). */
  GPIOH->MODER &= ~(3UL << (3 * 2));

  /* ---- 6. PA9 TX → AF7 ---- */
  GPIOA->MODER &= ~(3U << 18);
  GPIOA->MODER |= (2U << 18);
  GPIOA->AFR[1] &= ~(0xFU << 4);
  GPIOA->AFR[1] |= (7U << 4);

  /* ---- 7. PA10 RX → AF7, pull-up ---- */
  GPIOA->MODER &= ~(3U << 20);
  GPIOA->MODER |= (2U << 20);
  GPIOA->PUPDR &= ~(3U << 20);
  GPIOA->PUPDR |= (1U << 20);
  GPIOA->AFR[1] &= ~(0xFU << 8);
  GPIOA->AFR[1] |= (7U << 8);

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

void system_deinit(void)
{
  USART1->CR1 = 0U; /* TE/RE/RXNEIE/UE all off */
  RCC->APB2ENR &= ~RCC_APB2ENR_USART1EN;

  TIM2->CR1 = 0U;  /* stop counting */
  TIM2->DIER = 0U; /* disable update interrupt */
  RCC->APB1ENR1 &= ~RCC_APB1ENR1_TIM2EN;

  GPIOA->MODER |= (3U << 18) | (3U << 20); /* PA9, PA10 -> analog (reset state) */
  GPIOA->AFR[1] &= ~((0xFU << 4) | (0xFU << 8));
  GPIOA->PUPDR &= ~(3U << 20); /* drop PA10's pull-up */

  /* Clock deinit: leave the app a known, cold-reset-like clock state
   * (HSI16, PLL off) instead of handing it a running PLL it didn't
   * configure itself.
   *
   * ORDER MATTERS: switch SYSCLK (RCC_CFGR.SW) to HSI16 FIRST. PLLON
   * cannot be cleared by software while the PLL is still selected as
   * SYSCLK (hardware-protected on STM32) - doing this in the other
   * order silently no-ops the PLLON clear and hangs forever on the
   * PLLRDY wait below, since PLLRDY then never clears either. */
  RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | (0U << RCC_CFGR_SW_Pos); /* SW = HSI16 */
  while ((RCC->CFGR & RCC_CFGR_SWS) != (0U << RCC_CFGR_SWS_Pos))
    ;

  RCC->CR &= ~RCC_CR_PLLON;
  while (RCC->CR & RCC_CR_PLLRDY)
    ;
}

static void wait_to_boot_mode(void)
{
  if (GPIOH->IDR & (1UL << 3))
  {
    boot_mode = 1;

    bootloader_jump_to_application();
  }
}

static void handle_boot_timeout(void)
{
  static uint32_t timeout_counter = 0;

  if (timeout_counter > 3000U)
  {
    if (bootloader_get_mode() == BOOT_MODE_APPLICATION)
    {
      system_deinit();
      bootloader_jump_to_application();
    }
    else
    {
      timeout_counter = 0;
    }
  }
  else
  {
    timeout_counter++;
  }
}

void USART1_IRQHandler(void)
{
  if (USART1->ISR & USART_ISR_RXNE)
  {
    uint8_t received_byte = (uint8_t)(USART1->RDR & 0xFF);

    communication_handle_request(received_byte);
  }
}

void TIM2_IRQHandler(void)
{
  if (TIM2->SR & TIM_SR_UIF)
  {
    // handle_boot_timeout();
    TIM2->SR &= ~TIM_SR_UIF;
  }
}