#include "stm32wbxx.h"
#include <string.h>

#include "communication_protocol.h"
#include "bootloader.h"


static volatile message_frame_t message_frame;
static volatile communication_t communication;

static volatile uint8_t communication_rx_buff[MAX_PAYLOAD];
static volatile uint8_t communication_tx_buff[MAX_PAYLOAD] = {0};

static volatile uint32_t timeout_counter = COMM_TIMEOUT;

static uint8_t crc8_update(uint8_t crc, uint8_t byte)
{
  crc ^= byte;
  for (uint8_t bit = 0U; bit < 8U; bit++)
  {
    if (crc & 0x80U)
    {
      crc = (uint8_t)((crc << 1) ^ 0x07U);
    }
    else
    {
      crc = (uint8_t)(crc << 1);
    }
  }
  return crc;
}

static uint8_t calculate_crc(const volatile message_frame_t *frame)
{
  uint8_t crc = crc8_update(0U, frame->id);
  crc = crc8_update(crc, frame->payload_size);
  for (uint8_t i = 0U; i < frame->payload_size; i++)
  {
    crc = crc8_update(crc, frame->payload[i]);
  }
  return crc;
}

static void transport_send_char(char c)
{
#if defined(BOOTLOADER_COMM_USE_USB)
  (void)c;
#else
  while (!(USART1->ISR & USART_ISR_TXE));
  USART1->TDR = c;
#endif
}

static void transport_send_bytes(uint8_t *str, uint8_t size)
{
#if defined(BOOTLOADER_COMM_USE_USB)
  (void)str;
  (void)size;
#else
  while (size--)
  {
    transport_send_char((char)*str++);
  }
#endif
}

void communication_init(void)
{
  memset((void *)&message_frame, 0, sizeof(message_frame_t));
  memset((void *)&communication, 0, sizeof(communication_t));

  communication.state = COMM_SOF;

  message_frame.payload = (uint8_t *)&communication_rx_buff;
}

void communication_send_responce(uint8_t *str, uint8_t size)
{
  transport_send_bytes(str, size);
}

void communication_add_responce(message_frame_t *frame, uint8_t *responce, uint8_t size)
{
  communication_tx_buff[0] = FRAME_SOF;
  communication_tx_buff[1] = frame->id;
  communication_tx_buff[2] = size;

  if (size > 0)
  {
    memcpy(&communication_tx_buff[3], responce, size);
  }

  communication_tx_buff[3 + size] = calculate_crc(frame);

  communication_tx_buff[4 + size] = FRAME_EOF;


  transport_send_bytes(communication_tx_buff, size + 5);

  memset(frame, 0, sizeof(message_frame_t));
  memset((void *)&communication, 0, sizeof(communication_t));
  communication.data_ready = COMM_WAITING_FOR_DATA;
}

void communication_reset(void)
{
  memset((void *)&message_frame, 0, sizeof(message_frame_t));
  memset((void *)&communication, 0, sizeof(communication_t));
  communication.data_ready = COMM_WAITING_FOR_DATA;
  communication.state = COMM_SOF;
  communication.payload_counter = 0;
}

void communication_handle_request(unsigned char byte)
{
  switch (communication.state)
  {
  case COMM_SOF:
    if (byte == FRAME_SOF)
    {
      communication.payload_counter = 0;
      communication.state = COMM_ID;
    }
    break;

  case COMM_ID:
    message_frame.id = byte;
    communication.state = COMM_SIZE;
    break;

  case COMM_SIZE:
    if (byte > MAX_PAYLOAD)
    {
      communication.state = COMM_SOF;
      break;
    }
    message_frame.payload_size = byte;
    communication.state = (byte == 0U) ? COMM_CRC : COMM_PAYLOAD;
    break;

  case COMM_PAYLOAD:
    message_frame.payload[communication.payload_counter++] = byte;

    if (communication.payload_counter == message_frame.payload_size)
    {
      communication.state = COMM_CRC;
    }
    break;

  case COMM_CRC:
    message_frame.crc = byte;
    if (byte == calculate_crc(&message_frame))
    {
      communication.state = COMM_EOF;
    }
    else
    {
      communication.state = COMM_SOF;
    }
    break;

  case COMM_EOF:
    if (byte == FRAME_EOF)
    {
      communication.data_ready = COMM_DATA_IS_READY;
    }

    communication.state = COMM_SOF;
    break;

  default:
    communication.state = COMM_SOF;
    break;
  }
  timeout_counter = COMM_TIMEOUT;
}

void communication_application(void)
{
  if (communication.data_ready != COMM_DATA_IS_READY)
  {
    return;
  }
  message_frame_t frame;


  NVIC_DisableIRQ(USART1_IRQn);
  memcpy(&frame, (void *)&message_frame, sizeof(message_frame_t));
  NVIC_EnableIRQ(USART1_IRQn);

  switch (frame.id)
  {
  case CMD_ECHO:
    communication_add_responce(&frame, frame.payload, frame.payload_size);
    break;

  case CMD_SYSTEM_INFO:
  {
    uint8_t responce[3] = {0x01, 0x03, 0x07};
    communication_add_responce(&frame, responce, sizeof(responce));
  }
  break;

  case CMD_GET_STATUS:
  {
    uint8_t responce[1] = {0xFF};
    communication_add_responce(&frame, responce,  sizeof(responce));
  }
  break;

  case CMD_START_UPDATE:
  {
    uint8_t responce = bootloader_start_update();
    communication_add_responce(&frame, &responce,  sizeof(responce));
  }
  break;

  case CMD_ERASE_FLASH:
  {
    uint8_t responce = bootloader_erase_flash();
    communication_add_responce(&frame, &responce,  sizeof(responce));
  }
  break;

  case CMD_SEND_DATA_BATCH:
  {
    // uint8_t responce = bootloader_update_batch(frame.payload, frame.payload_size);

    // communication_add_responce(&frame, &responce,  sizeof(responce));
  }

  case CMD_END_UPDATE:
  {
  //  uint8_t responce = bootloader_stop_update();
  //  communication_add_responce(&frame, &responce,  sizeof(responce));
  }
  break;

  default:
    memset((void *)&communication, 0, sizeof(communication_t));
    communication.data_ready = COMM_WAITING_FOR_DATA;
    break;
  }
}

void communication_handle_timeout(void)
{
  if (timeout_counter)
  {
    timeout_counter--;
  }
  else
  {
    communication_reset();
  }
}