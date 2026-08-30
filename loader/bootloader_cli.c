/*
 * bootloader_cli.c
 *
 * Single-file CLI host application for talking to the STM32WBxx USB-CDC
 * bootloader whose frame/CRC handling is implemented in
 * communication_protocol.c (communication_handle_request / calculate_crc /
 * communication_add_responce).
 *
 * Frame format (matches the firmware exactly):
 *
 *   [ SOF ][ ID ][ SIZE ][ PAYLOAD (SIZE bytes) ][ CRC8 ][ EOF ]
 *
 *   CRC8 is computed over ID, SIZE, and PAYLOAD using the same
 *   bit-by-bit poly-0x07 algorithm as crc8_update()/calculate_crc()
 *   in the firmware.
 *
 * ---------------------------------------------------------------------
 * IMPORTANT - VALUES YOU MUST CONFIRM
 * ---------------------------------------------------------------------
 * FRAME_SOF, FRAME_EOF, MAX_PAYLOAD and every CMD_* value below are NOT
 * present in the .c file you shared - they live in
 * "communication_protocol.h" and "bootloader_config.h", which were not
 * provided. The values below are placeholders that follow the most
 * common convention for this style of framed protocol. Open those two
 * headers in your firmware project and update the #defines in the
 * "PROTOCOL CONSTANTS" section below to match exactly, or the CRC/parsing
 * will not line up with the device and every command will time out.
 * ---------------------------------------------------------------------
 *
 * Build (Linux / macOS):
 *   gcc -O2 -Wall -Wextra -o bootloader_cli bootloader_cli.c
 *
 * Usage:
 *   ./bootloader_cli -p /dev/ttyACM0 ping
 *   ./bootloader_cli -p /dev/ttyACM0 sysinfo
 *   ./bootloader_cli -p /dev/ttyACM0 status
 *   ./bootloader_cli -p /dev/ttyACM0 echo "hello"
 *   ./bootloader_cli -p /dev/ttyACM0 flash firmware.bin
 *   ./bootloader_cli -p /dev/ttyACM0 shell
 *
 * Notes on CMD_SEND_DATA_BATCH / CMD_END_UPDATE:
 *   In the firmware you shared, both handlers are stubbed out (the
 *   actual bootloader_update_batch()/bootloader_stop_update() calls and
 *   their communication_add_responce() calls are commented out), so the
 *   device currently sends NO response frame for those two commands and
 *   also falls through from SEND_DATA_BATCH into END_UPDATE (missing
 *   break). This tool sends the frames correctly and will simply report
 *   a timeout for those two steps until you finish that firmware code;
 *   everything else (ECHO, SYSTEM_INFO, GET_STATUS, START_UPDATE,
 *   ERASE_FLASH) works against the code as shown.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <ctype.h>

/* ===================== PROTOCOL CONSTANTS ================================ */
/* CONFIRMED against the real device via a working Python test script:
 * FRAME_SOF, FRAME_EOF, MAX_PAYLOAD, CMD_ECHO..CMD_ERASE_FLASH below are
 * verified values. CMD_SEND_DATA_BATCH and CMD_END_UPDATE were NOT in that
 * script - they're inferred by continuing the same sequential numbering
 * (0,1,2,3,4 -> 5,6). Confirm these two against communication_protocol.h /
 * bootloader_config.h before relying on 'flash'. */

#define FRAME_SOF          0xE7u
#define FRAME_EOF          0x7Eu
#define MAX_PAYLOAD         64u

#define CMD_ECHO            0x00u
#define CMD_SYSTEM_INFO     0x01u
#define CMD_GET_STATUS      0x02u
#define CMD_START_UPDATE    0x03u
#define CMD_ERASE_FLASH     0x04u
#define CMD_SEND_DATA_BATCH 0x05u  /* TODO: confirm - inferred by pattern, not verified */
#define CMD_END_UPDATE      0x06u  /* TODO: confirm - inferred by pattern, not verified */

/* Bytes of firmware payload sent per CMD_SEND_DATA_BATCH frame while
 * flashing. Must be <= MAX_PAYLOAD. */
#define FLASH_CHUNK_SIZE    (MAX_PAYLOAD)

/* How long (ms) to wait for a reply frame before declaring a timeout. */
/* Default ms to wait for a reply frame before declaring a timeout.
 * Overridable at runtime with -t <ms>; some commands (e.g. START_UPDATE,
 * ERASE_FLASH) may involve real flash operations that take longer than a
 * quick ECHO round-trip. */
#define DEFAULT_RESPONSE_TIMEOUT_MS 3000
#define ERASE_TIMEOUT_MS 2000000/* 20 seconds max wait for device to be ready after erase */

typedef enum
{
  BOOTLOADER_IDLE = 0,
  BOOTLOADER_START_UPDATE,
  BOOTLOADER_ERASE_FLASH,
  BOOTLOADER_UPDATE,
  BOOTLOADER_END_UPDATE,
} boot_state_t;

static int g_response_timeout_ms = DEFAULT_RESPONSE_TIMEOUT_MS;

uint16_t erase_timeout = ERASE_TIMEOUT_MS; 

boot_state_t status = BOOTLOADER_IDLE;

/* ===================== CRC8 (identical to firmware) ====================== */

static uint8_t crc8_update(uint8_t crc, uint8_t byte)
{
    crc ^= byte;
    for (uint8_t bit = 0; bit < 8; bit++)
    {
        if (crc & 0x80u)
        {
            crc = (uint8_t)((crc << 1) ^ 0x07u);
        }
        else
        {
            crc = (uint8_t)(crc << 1);
        }
    }
    return crc;
}

static uint8_t calc_frame_crc(uint8_t id, uint8_t size, const uint8_t *payload)
{
    uint8_t crc = crc8_update(0u, id);
    crc = crc8_update(crc, size);
    for (uint8_t i = 0; i < size; i++)
    {
        crc = crc8_update(crc, payload[i]);
    }
    return crc;
}

/* ===================== Serial port handling ============================= */

static int serial_open(const char *path, int baud)
{
    int fd = open(path, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        fprintf(stderr, "error: cannot open %s: %s\n", path, strerror(errno));
        return -1;
    }

    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    if (tcgetattr(fd, &tty) != 0)
    {
        fprintf(stderr, "error: tcgetattr: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    speed_t speed;
    switch (baud)
    {
        case 9600:    speed = B9600;   break;
        case 19200:   speed = B19200;  break;
        case 38400:   speed = B38400;  break;
        case 57600:   speed = B57600;  break;
        case 115200:  speed = B115200; break;
        case 230400:  speed = B230400; break;
        default:
            fprintf(stderr, "error: unsupported baud rate %d\n", baud);
            close(fd);
            return -1;
    }

    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);

    /* 8N1, raw mode, no flow control */
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag |= (CLOCAL | CREAD);

    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    tty.c_oflag &= ~OPOST;

    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 0; /* we use select() for timeouts ourselves */

    if (tcsetattr(fd, TCSANOW, &tty) != 0)
    {
        fprintf(stderr, "error: tcsetattr: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    tcflush(fd, TCIOFLUSH);

    /* Explicitly assert DTR. This device's USB-CDC (TinyUSB) firmware
     * gates its response path behind tud_cdc_line_state_cb(), which only
     * fires once DTR is raised - confirmed necessary via a working
     * reference script against the real device. Some OS/driver stacks
     * assert DTR automatically on open, but do it explicitly to be sure. */
    int modem_status;
    if (ioctl(fd, TIOCMGET, &modem_status) == 0)
    {
        modem_status |= TIOCM_DTR;
        if (ioctl(fd, TIOCMSET, &modem_status) != 0)
        {
            fprintf(stderr, "warning: could not assert DTR: %s\n", strerror(errno));
        }
    }
    else
    {
        fprintf(stderr, "warning: TIOCMGET failed, could not assert DTR: %s\n",
                strerror(errno));
    }

    /* Give the MCU time to process line_state_cb / emit any startup
     * banner before we start exchanging frames. */
    usleep(300000);

    /* Drain any startup banner or stray bytes so they aren't mistaken for
     * part of the first protocol frame. */
    {
        uint8_t drain_buf[256];
        for (;;)
        {
            fd_set set;
            FD_ZERO(&set);
            FD_SET(fd, &set);
            struct timeval tv = {.tv_sec = 0, .tv_usec = 50000};
            if (select(fd + 1, &set, NULL, NULL, &tv) <= 0)
            {
                break;
            }
            ssize_t n = read(fd, drain_buf, sizeof(drain_buf));
            if (n <= 0)
            {
                break;
            }
            fprintf(stderr, "note: drained %zd startup byte(s) before frame exchange\n", n);
        }
    }

    return fd;
}

static int read_byte_timeout(int fd, uint8_t *out, int timeout_ms)
{
    fd_set set;
    struct timeval tv;
    FD_ZERO(&set);
    FD_SET(fd, &set);
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int rv = select(fd + 1, &set, NULL, NULL, &tv);
    if (rv <= 0)
    {
        return 0; /* timeout or error */
    }
    ssize_t n = read(fd, out, 1);
    return (n == 1) ? 1 : 0;
}

/* Writes the full buffer to fd, looping over short writes, EINTR, and
 * EAGAIN (the port is opened blocking, but short writes can still occur
 * on real serial devices / ptys under load). Returns 0 on success. */
static int write_all(int fd, const uint8_t *buf, size_t len)
{
    size_t sent = 0;
    while (sent < len)
    {
        ssize_t n = write(fd, buf + sent, len - sent);
        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                /* Port is non-blocking or momentarily full; wait for it
                 * to become writable rather than busy-looping. */
                fd_set set;
                FD_ZERO(&set);
                FD_SET(fd, &set);
                struct timeval tv = {.tv_sec = 1, .tv_usec = 0};
                select(fd + 1, NULL, &set, NULL, &tv);
                continue;
            }
            fprintf(stderr, "error: write() failed: %s\n", strerror(errno));
            return -1;
        }
        sent += (size_t)n;
    }
    return 0;
}

/* ===================== Frame send/receive ================================ */

typedef struct
{
    uint8_t id;
    uint8_t size;
    uint8_t payload[MAX_PAYLOAD];
} rx_frame_t;

/* When set, recv_frame() also accepts a reply whose CRC byte equals the
 * CRC of the request we just sent, in addition to the normal CRC computed
 * from the reply's own id/size/payload. This tolerates the known firmware
 * bug in communication_add_responce()/calculate_crc(), which computes the
 * response CRC from the stale request frame instead of the actual response
 * being sent. Off by default; enabled with --firmware-crc-quirk. Does NOT
 * help CMD_SEND_DATA_BATCH, which has a separate, unrelated bug (missing
 * `break;` causes a second bogus id=0 frame and calls
 * bootloader_stop_update() on every chunk) that no host-side workaround
 * can safely paper over. */
static int g_firmware_crc_quirk = 0;
static uint8_t g_last_request_crc = 0;
static int g_verbose = 0;

static void hex_trace(const char *label, const uint8_t *buf, size_t len)
{
    if (!g_verbose)
    {
        return;
    }
    fprintf(stderr, "%s (%zu bytes):", label, len);
    for (size_t i = 0; i < len; i++)
    {
        fprintf(stderr, " %02X", buf[i]);
    }
    fprintf(stderr, "\n");
}

static void send_frame(int fd, uint8_t id, const uint8_t *payload, uint8_t size)
{
    uint8_t buf[3 + MAX_PAYLOAD + 2];
    size_t n = 0;

    buf[n++] = (uint8_t)FRAME_SOF;
    buf[n++] = id;
    buf[n++] = size;
    if (size > 0)
    {
        memcpy(&buf[n], payload, size);
        n += size;
    }
    uint8_t crc = calc_frame_crc(id, size, payload);
    g_last_request_crc = crc;
    buf[n++] = crc;
    buf[n++] = (uint8_t)FRAME_EOF;

    hex_trace("TX", buf, n);
    (void)write_all(fd, buf, n);
}

/* Mirrors the firmware's communication_handle_request() state machine. */
typedef enum { ST_SOF, ST_ID, ST_SIZE, ST_PAYLOAD, ST_CRC, ST_EOF } rx_state_t;

/* Returns 1 on a fully-validated frame, 0 on timeout/failure. */
static int recv_frame(int fd, rx_frame_t *out, int timeout_ms)
{
    rx_state_t state = ST_SOF;
    uint8_t counter = 0;
    uint8_t crc_expected = 0;
    memset(out, 0, sizeof(*out));

    uint8_t raw_seen[3 + MAX_PAYLOAD + 2];
    size_t raw_seen_n = 0;

    struct timeval start, now;
    gettimeofday(&start, NULL);

    for (;;)
    {
        gettimeofday(&now, NULL);
        long elapsed_ms = (now.tv_sec - start.tv_sec) * 1000 +
                           (now.tv_usec - start.tv_usec) / 1000;
        long remaining = timeout_ms - elapsed_ms;
        if (remaining <= 0)
        {
            if (g_verbose)
            {
                if (raw_seen_n > 0)
                {
                    hex_trace("RX (timed out, partial/stray bytes seen)", raw_seen, raw_seen_n);
                }
                else
                {
                    fprintf(stderr, "RX: timed out, 0 bytes received\n");
                }
            }
            return 0;
        }

        uint8_t byte;
        if (!read_byte_timeout(fd, &byte, (int)remaining))
        {
            continue; /* recheck elapsed time / remaining at top of loop */
        }

        if (raw_seen_n < sizeof(raw_seen))
        {
            raw_seen[raw_seen_n++] = byte;
        }

        switch (state)
        {
            case ST_SOF:
                if (byte == (uint8_t)FRAME_SOF)
                {
                    counter = 0;
                    state = ST_ID;
                }
                break;

            case ST_ID:
                out->id = byte;
                state = ST_SIZE;
                break;

            case ST_SIZE:
                if (byte > MAX_PAYLOAD)
                {
                    state = ST_SOF;
                    break;
                }
                out->size = byte;
                state = (byte == 0) ? ST_CRC : ST_PAYLOAD;
                break;

            case ST_PAYLOAD:
                out->payload[counter++] = byte;
                if (counter == out->size)
                {
                    state = ST_CRC;
                }
                break;

            case ST_CRC:
                crc_expected = calc_frame_crc(out->id, out->size, out->payload);
                if (byte == crc_expected)
                {
                    state = ST_EOF;
                }
                else if (g_firmware_crc_quirk && byte == g_last_request_crc)
                {
                    /* Tolerate the known firmware bug: response CRC equals
                     * the CRC of the request we just sent rather than the
                     * CRC of the actual response bytes. */
                    state = ST_EOF;
                }
                else
                {
                    if (g_verbose)
                    {
                        fprintf(stderr, "RX: CRC mismatch for id=0x%02X size=%u - got "
                                         "0x%02X, expected 0x%02X (or 0x%02X w/ -q "
                                         "quirk) - resyncing on next SOF\n",
                                out->id, out->size, byte, crc_expected, g_last_request_crc);
                    }
                    state = ST_SOF;
                }
                break;

            case ST_EOF:
                if (byte == (uint8_t)FRAME_EOF)
                {
                    hex_trace("RX (valid frame)", raw_seen, raw_seen_n);
                    return 1;
                }
                if (g_verbose)
                {
                    fprintf(stderr, "RX: byte after CRC was 0x%02X, expected FRAME_EOF "
                                     "(0x%02X) - discarding and resyncing on next SOF\n",
                            byte, (uint8_t)FRAME_EOF);
                }
                state = ST_SOF;
                break;
        }
    }
}

/* Sends a command and waits for the single reply frame. Returns 1 on
 * success (reply captured in *reply), 0 on timeout. */
static int do_command(int fd, uint8_t cmd, const uint8_t *payload, uint8_t size,
                       rx_frame_t *reply)
{
    send_frame(fd, cmd, payload, size);
    return recv_frame(fd, reply, g_response_timeout_ms);
}

static void hex_dump(const uint8_t *data, uint8_t size)
{
    for (uint8_t i = 0; i < size; i++)
    {
        printf("%02X ", data[i]);
    }
    printf("\n");
}

/* ===================== Command implementations ============================ */

static int cmd_echo(int fd, const char *text)
{
    uint8_t payload[MAX_PAYLOAD];
    size_t len = strlen(text);
    if (len > MAX_PAYLOAD)
    {
        len = MAX_PAYLOAD;
    }
    memcpy(payload, text, len);

    rx_frame_t reply;
    printf("-> ECHO \"%s\"\n", text);
    if (!do_command(fd, CMD_ECHO, payload, (uint8_t)len, &reply))
    {
        fprintf(stderr, "error: timeout waiting for ECHO reply\n");
        return 1;
    }
    printf("<- id=0x%02X size=%u payload=\"%.*s\"\n",
           reply.id, reply.size, reply.size, reply.payload);
    return 0;
}

static int cmd_sysinfo(int fd)
{
    rx_frame_t reply;
    printf("-> SYSTEM_INFO\n");
    if (!do_command(fd, CMD_SYSTEM_INFO, NULL, 0, &reply))
    {
        fprintf(stderr, "error: timeout waiting for SYSTEM_INFO reply\n");
        return 1;
    }
    printf("<- id=0x%02X size=%u payload=", reply.id, reply.size);
    hex_dump(reply.payload, reply.size);
    return 0;
}

static int cmd_status(int fd)
{
    rx_frame_t reply;
    if (!do_command(fd, CMD_GET_STATUS, NULL, 0, &reply))
    {
        fprintf(stderr, "error: timeout waiting for GET_STATUS reply\n");
        return 1;
    }
    return 0;
}

static int cmd_start_update(int fd)
{
    rx_frame_t reply;
    printf("-> START_UPDATE\n");
    if (!do_command(fd, CMD_START_UPDATE, NULL, 0, &reply))
    {
        fprintf(stderr, "error: timeout waiting for START_UPDATE reply\n");
        return 1;
    }
    printf("<- id=0x%02X size=%u payload=", reply.id, reply.size);
    return 0;
}

static int cmd_erase_flash(int fd)
{
    rx_frame_t reply;
    printf("-> ERASE_FLASH\n");
    if (!do_command(fd, CMD_ERASE_FLASH, NULL, 0, &reply))
    {
        fprintf(stderr, "error: timeout waiting for ERASE_FLASH reply\n");
        return 1;
    }
    printf("<- id=0x%02X size=%u payload=", reply.id, reply.size);
    return 0;
}

static int cmd_end_update(int fd)
{
    rx_frame_t reply;
    printf("-> END_UPDATE\n");
    if (!do_command(fd, CMD_END_UPDATE, NULL, 0, &reply))
    {
        fprintf(stderr, "warning: no reply to END_UPDATE (expected - firmware "
                         "handler is currently stubbed out, see file header)\n");
        return 1;
    }
    printf("<- id=0x%02X size=%u payload=", reply.id, reply.size);
    return 0;
}

/* Returns 1 if the file at path starts with the ELF magic (0x7F 'E' 'L' 'F'). */
static int file_is_elf(const char *path)
{
    unsigned char magic[4] = {0};
    FILE *f = fopen(path, "rb");
    if (!f)
    {
        return 0;
    }
    size_t n = fread(magic, 1, 4, f);
    fclose(f);
    return (n == 4 && magic[0] == 0x7F && magic[1] == 'E' &&
            magic[2] == 'L' && magic[3] == 'F');
}

/* Tries a list of objcopy binary names in $PATH, returns the first that
 * exists, or NULL if none do. Uses `command -v` via the shell so this
 * respects the user's PATH the same way a normal invocation would. */
static const char *find_objcopy(void)
{
    static const char *candidates[] = {
        "arm-none-eabi-objcopy",
        "objcopy",
        NULL
    };
    static char found[256];

    for (int i = 0; candidates[i] != NULL; i++)
    {
        char cmd[300];
        snprintf(cmd, sizeof(cmd), "command -v %s >/dev/null 2>&1", candidates[i]);
        if (system(cmd) == 0)
        {
            strncpy(found, candidates[i], sizeof(found) - 1);
            found[sizeof(found) - 1] = '\0';
            return found;
        }
    }
    return NULL;
}

/* Converts an ELF file to a flat raw binary via `objcopy -O binary`,
 * writing the result to out_path (caller-provided buffer of at least
 * out_path_size bytes, filled with a mkstemp()-generated path).
 * Returns 0 on success, non-zero on failure. */
static int convert_elf_to_bin(const char *elf_path, char *out_path, size_t out_path_size)
{
    const char *objcopy = find_objcopy();
    if (!objcopy)
    {
        fprintf(stderr,
            "error: '%s' looks like an ELF file, but no objcopy was found in "
            "PATH (tried arm-none-eabi-objcopy, objcopy).\n"
            "Install your ARM GCC toolchain, or convert manually first:\n"
            "  arm-none-eabi-objcopy -O binary %s firmware.bin\n",
            elf_path, elf_path);
        return 1;
    }

    snprintf(out_path, out_path_size, "/tmp/bootloader_cli_XXXXXX.bin");
    /* mkstemps needs the suffix length; emulate with mkstemp + rename-free
     * approach by building the name manually since mkstemps isn't C99/POSIX
     * base - use mkstemp on a name without suffix instead. */
    char tmpl[64];
    snprintf(tmpl, sizeof(tmpl), "/tmp/bootloader_cli_XXXXXX");
    int tmp_fd = mkstemp(tmpl);
    if (tmp_fd < 0)
    {
        fprintf(stderr, "error: mkstemp failed: %s\n", strerror(errno));
        return 1;
    }
    close(tmp_fd); /* objcopy will overwrite/create the real content */
    snprintf(out_path, out_path_size, "%s", tmpl);

    char cmd[1024];
    int n = snprintf(cmd, sizeof(cmd), "%s -O binary -- '%s' '%s'",
                      objcopy, elf_path, out_path);
    if (n < 0 || (size_t)n >= sizeof(cmd))
    {
        fprintf(stderr, "error: file path too long for objcopy command\n");
        unlink(out_path);
        return 1;
    }

    printf("Detected ELF input, converting with '%s'...\n", objcopy);
    int rc = system(cmd);
    if (rc != 0)
    {
        fprintf(stderr, "error: objcopy conversion failed (exit code %d)\n", rc);
        unlink(out_path);
        return 1;
    }

    printf("Converted to flat binary: %s\n", out_path);
    return 0;
}

static int cmd_flash(int fd, const char *path)
{
    if (g_firmware_crc_quirk)
    {
        fprintf(stderr,
            "warning: -q only works around the response-CRC bug for single-\n"
            "reply commands. CMD_SEND_DATA_BATCH has a separate fallthrough\n"
            "bug (missing 'break;' before CMD_END_UPDATE) that calls\n"
            "bootloader_stop_update() on every chunk and emits a second,\n"
            "bogus id=0 frame - no client-side workaround is safe for that.\n"
            "Fix the firmware before relying on 'flash'.\n");
    }

    char converted_path[64] = {0};
    const char *bin_path = path;
    int using_temp_file = 0;

    if (file_is_elf(path))
    {
        if (convert_elf_to_bin(path, converted_path, sizeof(converted_path)) != 0)
        {
            return 1;
        }
        bin_path = converted_path;
        using_temp_file = 1;
    }

    FILE *f = fopen(bin_path, "rb");
    if (!f)
    {
        fprintf(stderr, "error: cannot open %s: %s\n", bin_path, strerror(errno));
        if (using_temp_file)
        {
            unlink(converted_path);
        }
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize < 0)
    {
        fprintf(stderr, "error: cannot determine file size\n");
        fclose(f);
        if (using_temp_file)
        {
            unlink(converted_path);
        }
        return 1;
    }

    if (using_temp_file)
    {
        printf("Firmware file: %s -> %s (%ld bytes)\n", path, bin_path, fsize);
    }
    else
    {
        printf("Firmware file: %s (%ld bytes)\n", path, fsize);
    }

    if (cmd_start_update(fd) != 0)
    {
        fclose(f);
        if (using_temp_file)
        {
            unlink(converted_path);
        }
        return 1;
    }
    if (cmd_erase_flash(fd) != 0)
    {
        fclose(f);
        if (using_temp_file)
        {
            unlink(converted_path);
        }
        return 1;
    }

    fprintf(stderr, "Waiting for device to be ready after erase...\n");
    do
    {
        status = cmd_status(fd);
        usleep(100);  // Sleep for 100000 microseconds (100 ms) to avoid busy-waiting
        erase_timeout--;
    }
    while ((status != BOOTLOADER_UPDATE) && (erase_timeout > 0));


    if (erase_timeout <= 0 || status != BOOTLOADER_UPDATE)
    {
        fprintf(stderr, "error: device did not become ready after erase within the expected time.\n");
     //   return 1;
    }

    uint8_t chunk[FLASH_CHUNK_SIZE];
    size_t total_sent = 0;
    size_t n;
    int chunk_index = 0;

    while ((n = fread(chunk, 1, FLASH_CHUNK_SIZE, f)) > 0)
    {
        rx_frame_t reply;
        int ok = do_command(fd, CMD_SEND_DATA_BATCH, chunk, (uint8_t)n, &reply);
        total_sent += n;
        chunk_index++;

        printf("\r-> SEND_DATA_BATCH chunk %d (%zu/%ld bytes)%s",
               chunk_index, total_sent, fsize, ok ? " [ack]" : " [no reply]");
        fflush(stdout);

        if (!ok)
        {
            /* Firmware handler for SEND_DATA_BATCH is currently stubbed
             * out and sends no ack (see header comment); keep going
             * rather than aborting, but let the user know once. */
        }
    }
    printf("\n");
    fclose(f);

    cmd_end_update(fd);

    printf("Done: sent %zu / %ld bytes.\n", total_sent, fsize);

    if (using_temp_file)
    {
        unlink(converted_path);
    }
    return 0;
}

static void print_usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s -p <port> [-b <baud>] [-t <ms>] [-q] [-v] <command> [args]\n\n"
        "Options:\n"
        "  -p <port>   Serial device, e.g. /dev/cu.usbmodem1234561\n"
        "  -b <baud>   Baud rate (default 115200; ignored by most USB-CDC\n"
        "              devices but still required by termios)\n"
        "  -t <ms>     Reply timeout in milliseconds (default 2000). Raise\n"
        "              this if a command (e.g. start-update, erase) involves\n"
        "              real flash operations that take longer than a quick\n"
        "              echo round-trip.\n"
        "  -v          Verbose: print every raw byte sent/received in hex,\n"
        "              plus CRC-mismatch and framing-resync diagnostics.\n"
        "  -q          Firmware CRC-quirk compatibility mode. Also accepts a\n"
        "              reply whose CRC equals the CRC of the request just\n"
        "              sent, working around a known firmware bug where\n"
        "              communication_add_responce()/calculate_crc() hash the\n"
        "              stale request frame instead of the actual response.\n"
        "              Does NOT help 'flash'/CMD_SEND_DATA_BATCH - that\n"
        "              command has a separate fallthrough bug (missing\n"
        "              'break;' into CMD_END_UPDATE) with no safe host-side\n"
        "              workaround; fix the firmware for that one.\n\n"
        "Commands:\n"
        "  ping                 Alias for 'echo ping'\n"
        "  echo <text>          Send CMD_ECHO with <text> as payload\n"
        "  sysinfo              Send CMD_SYSTEM_INFO\n"
        "  status                Send CMD_GET_STATUS\n"
        "  start-update          Send CMD_START_UPDATE\n"
        "  erase                Send CMD_ERASE_FLASH\n"
        "  end-update            Send CMD_END_UPDATE\n"
        "  flash <file.bin|.elf>  Full update sequence: start-update, erase,\n"
        "                         chunked CMD_SEND_DATA_BATCH, end-update.\n"
        "                         If given an .elf (or any file starting with\n"
        "                         the ELF magic), it is first converted to a\n"
        "                         flat binary with objcopy -O binary before\n"
        "                         flashing (requires arm-none-eabi-objcopy or\n"
        "                         objcopy in PATH).\n"
        "  shell                Interactive prompt (type 'help' inside)\n",
        prog);
}

static void run_shell(int fd)
{
    char line[256];
    printf("Interactive bootloader shell. Type 'help' for commands, 'quit' to exit.\n");
    for (;;)
    {
        printf("bl> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin))
        {
            break;
        }
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
        {
            line[--len] = '\0';
        }
        if (len == 0)
        {
            continue;
        }

        char *cmd = strtok(line, " ");
        char *arg = strtok(NULL, "");

        if (!cmd) continue;
        if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0)
        {
            break;
        }
        else if (strcmp(cmd, "help") == 0)
        {
            printf("commands: ping, echo <text>, sysinfo, status, start-update, "
                   "erase, end-update, flash <file>, quit\n");
        }
        else if (strcmp(cmd, "ping") == 0)
        {
            cmd_echo(fd, "ping");
        }
        else if (strcmp(cmd, "echo") == 0)
        {
            cmd_echo(fd, arg ? arg : "");
        }
        else if (strcmp(cmd, "sysinfo") == 0)
        {
            cmd_sysinfo(fd);
        }
        else if (strcmp(cmd, "status") == 0)
        {
            cmd_status(fd);
        }
        else if (strcmp(cmd, "start-update") == 0)
        {
            cmd_start_update(fd);
        }
        else if (strcmp(cmd, "erase") == 0)
        {
            cmd_erase_flash(fd);
        }
        else if (strcmp(cmd, "end-update") == 0)
        {
            cmd_end_update(fd);
        }
        else if (strcmp(cmd, "flash") == 0)
        {
            if (!arg)
            {
                printf("usage: flash <file>\n");
            }
            else
            {
                cmd_flash(fd, arg);
            }
        }
        else
        {
            printf("unknown command '%s' (try 'help')\n", cmd);
        }
    }
}

int main(int argc, char **argv)
{
    const char *port = NULL;
    int baud = 115200;
    int opt;

    while ((opt = getopt(argc, argv, "p:b:t:qvh")) != -1)
    {
        switch (opt)
        {
            case 'p': port = optarg; break;
            case 'b': baud = atoi(optarg); break;
            case 't': g_response_timeout_ms = atoi(optarg); break;
            case 'q': g_firmware_crc_quirk = 1; break;
            case 'v': g_verbose = 1; break;
            case 'h':
            default:
                print_usage(argv[0]);
                return (opt == 'h') ? 0 : 1;
        }
    }

    if (!port || optind >= argc)
    {
        print_usage(argv[0]);
        return 1;
    }

    const char *command = argv[optind];

    int fd = serial_open(port, baud);
    if (fd < 0)
    {
        return 1;
    }

    int rc = 0;

    if (strcmp(command, "ping") == 0)
    {
        rc = cmd_echo(fd, "ping");
    }
    else if (strcmp(command, "echo") == 0)
    {
        if (optind + 1 >= argc)
        {
            fprintf(stderr, "error: 'echo' requires a text argument\n");
            rc = 1;
        }
        else
        {
            rc = cmd_echo(fd, argv[optind + 1]);
        }
    }
    else if (strcmp(command, "sysinfo") == 0)
    {
        rc = cmd_sysinfo(fd);
    }
    else if (strcmp(command, "status") == 0)
    {
        rc = cmd_status(fd);
    }
    else if (strcmp(command, "start-update") == 0)
    {
        rc = cmd_start_update(fd);
    }
    else if (strcmp(command, "erase") == 0)
    {
        rc = cmd_erase_flash(fd);
    }
    else if (strcmp(command, "end-update") == 0)
    {
        rc = cmd_end_update(fd);
    }
    else if (strcmp(command, "flash") == 0)
    {
        if (optind + 1 >= argc)
        {
            fprintf(stderr, "error: 'flash' requires a firmware file path\n");
            rc = 1;
        }
        else
        {
            rc = cmd_flash(fd, argv[optind + 1]);
        }
    }
    else if (strcmp(command, "shell") == 0)
    {
        run_shell(fd);
    }
    else
    {
        fprintf(stderr, "error: unknown command '%s'\n", command);
        print_usage(argv[0]);
        rc = 1;
    }

    close(fd);
    return rc;
}