#include "stdint.h"

#define FRAME_SOF (0xE7U)
#define FRAME_EOF (0x7EU)
#define MAX_PAYLOAD (64U)

#define COMM_RESPONSE_CODE_SIZE 1U
#define COMM_TIMEOUT 1000U


#pragma region Transport_Layer
typedef enum
{
    COMM_SOF = 0,
    COMM_ID,
    COMM_SIZE,
    COMM_PAYLOAD,
    COMM_CRC,
    COMM_EOF,
} comm_state_t;

typedef struct
{
    uint8_t id;
    uint8_t payload_size;
    uint8_t *payload;
    uint8_t crc;
} message_frame_t;

typedef struct
{
    uint8_t state;
    uint8_t data_ready;
    uint8_t payload_counter;
    uint8_t txSize;
} communication_t;

typedef enum {
    COMM_DATA_IS_READY = 0U,
    COMM_WAITING_FOR_DATA = 1U
} comm_data_state_t;

// Communication application layer
#pragma region Application_Layer
typedef enum
{
    CMD_ECHO,
    CMD_SYSTEM_INFO,
    CMD_GET_STATUS,
    CMD_START_UPDATE,
    CMD_SEND_DATA_BATCH,
    CMD_END_UPDATE,
} comm_cmd_t;
#pragma endregion

void communication_init(void);

void communication_application(void);

void communication_add_responce(message_frame_t *frame, uint8_t *responce, uint8_t size);

void communication_reset(void);

void communication_send_responce(uint8_t *str, uint8_t size);

void communication_handle_request(unsigned char byte);

void communication_application(void);

void communication_handle_timeout(void);
