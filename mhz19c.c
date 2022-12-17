#include <stdio.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>

#include "mhz19c.h"

#define BUFFER_SIZE     8
#define RETRY_MAX       10

#define TX_START        0
#define TX_RESERVED     1
#define TX_COMMAND      2
#define TX_DATA(i)      3+i
#define TX_CHECK_SUM    8

#define RX_START        0
#define RX_COMMAND      1
#define RX_DATA(i)      2+i
#define RX_CHECK_SUM    8

#define START_VALUE     0xff
#define RESERVED_VALUE  0x01

#define COM_GET_CO2_PPM     0x86
#define COM_SET_AUTO_CALIB  0x79

#define CALIB_ON        0xa0
#define CALIB_OFF       0x00

static uint8_t mhz19c_get_checksum(const uint8_t *buffer);

// ----------------------------------------------------------------------------
// Logging Utility

#define mhz19c_log_verbose(mhz19c, ...) \
{                                       \
    if (mhz19c->verbose) {              \
        fprintf(stderr, "verbose: ");   \
        fprintf(stderr, __VA_ARGS__);   \
        fprintf(stderr, "\n");          \
    }                                   \
}

#define mhz19c_log_error(...)           \
{                                       \
    fprintf(stderr, "error: ");         \
    fprintf(stderr, __VA_ARGS__);       \
    fprintf(stderr, "\n");              \
}

void mhz19c_set_log_verbose(struct mhz19c_t *mhz19c, bool verbose) {
    mhz19c->verbose = verbose;
    mhz19c_log_verbose(mhz19c, "log level set to verbose = %d.", verbose);
}

// ----------------------------------------------------------------------------
// I/O Utility (UART)

bool mhz19c_open(struct mhz19c_t *mhz19c) {
    mhz19c_log_verbose(mhz19c, "open the device.");

    const int fd = open("/dev/serial0", O_RDWR);
    if (fd < 0) {
        mhz19c_log_error("failed to open the device.");
        return false;
    }

    mhz19c_log_verbose(mhz19c, "set the termios state.");

    // By default stop bit 1 byte and parity bit null.
    struct termios tio = {};
    // Use raw mode. This also sets data bit to 8 bytes.
    cfmakeraw(&tio);
    // Set serial port baud rate be 9600.
    cfsetspeed(&tio, B9600);

    tio.c_cflag |= CREAD;
    tio.c_cflag |= CLOCAL;

    // Set min read bytes to 0.
    tio.c_cc[VMIN] = 0;
    // Set read timeout in 500 ms.
    tio.c_cc[VTIME] = 5;

    // Note:
    // This is equivalent to `ioctl(fd, TCSETS, &tio)`.
    // Use of ioctl makes for non-portable programs.
    const int result = tcsetattr(fd, TCSANOW, &tio);
    if (result < 0) {
        mhz19c_log_error("failed to set the termios state.");
        close(fd);
        return false;
    }

    mhz19c->fd = fd;

    return true;
}

bool mhz19c_close(const struct mhz19c_t *mhz19c) {
    mhz19c_log_verbose(mhz19c, "close the device.");

    const int result = close(mhz19c->fd);
    if (result < 0) {
        mhz19c_log_error("failed to close the device.");
        return false;
    }

    return true;
}

static bool mhz19c_write(const struct mhz19c_t *mhz19c, uint8_t command, const uint8_t *data, size_t data_size) {
    // Write data.

    uint8_t buffer_tx[BUFFER_SIZE] = {};

    buffer_tx[TX_START] = START_VALUE;
    buffer_tx[TX_RESERVED] = RESERVED_VALUE;
    buffer_tx[TX_COMMAND] = command;

    if (data != NULL) {
        for (size_t i = 0; i < data_size; i += 1) {
            buffer_tx[TX_DATA(i)] = data[i];
        }
    }

    if (mhz19c->verbose) {
        char message[64] = "send data:";
        char temp[8];
        for (size_t i = 0; i < BUFFER_SIZE; i += 1) {
            sprintf(temp, " %02x", buffer_tx[i]);
            strcat(message, temp);
        }
        mhz19c_log_verbose(mhz19c, "%s", message);
    }

    ssize_t count = write(mhz19c->fd, buffer_tx, BUFFER_SIZE);
    if (count != BUFFER_SIZE) {
        mhz19c_log_error("failed to send data.");
        return false;
    }

    // Write checksum.

    const uint8_t checksum = mhz19c_get_checksum(buffer_tx);

    mhz19c_log_verbose(mhz19c, "send checksum: %02x", checksum);

    count = write(mhz19c->fd, &checksum, 1);
    if (count != 1) {
        mhz19c_log_error("failed to send checksum.");
        return false;
    }

    return true;
}

static bool mhz19c_read(const struct mhz19c_t *mhz19c, uint8_t *command, uint8_t *data, size_t data_size) {
    // Read data.

    uint8_t buffer_rx[BUFFER_SIZE] = {};
    size_t total_count = 0;

    for (int i = 0; i < RETRY_MAX; i += 1) {
        const size_t rem = BUFFER_SIZE - total_count;
        ssize_t count = read(mhz19c->fd, buffer_rx + total_count, rem);
        if (count < 0) {
            mhz19c_log_error("failed to read data. (%d)", count);
            return false;
        }

        total_count += (size_t)count;

        if (total_count == BUFFER_SIZE) {
            break;
        }
    }

    if (total_count != BUFFER_SIZE) {
        mhz19c_log_error("failed to read data. (%d)", total_count);
        return false;
    }

    if (mhz19c->verbose) {
        char message[64] = "read data:";
        char temp[8];
        for (size_t i = 0; i < BUFFER_SIZE; i += 1) {
            sprintf(temp, " %02x", buffer_rx[i]);
            strcat(message, temp);
        }
        mhz19c_log_verbose(mhz19c, "%s", message);
    }

    // Read checksum.

    uint8_t checksum = 0;
    total_count = 0;

    for (int i = 0; i < RETRY_MAX; i += 1) {
        const ssize_t count = read(mhz19c->fd, &checksum, 1);
        if (count < 0) {
            mhz19c_log_error("failed to read checksum. (%d)", count);
            return false;
        }

        total_count += (size_t)count;

        if (total_count == 1) {
            break;
        }
    }

    if (total_count != 1) {
        mhz19c_log_error("failed to read checksum. (%d)", total_count);
        return false;
    }

    mhz19c_log_verbose(mhz19c, "read checksum: %02x", checksum);

    // Validate data with checksum.

    const uint8_t actual_checksum = mhz19c_get_checksum(buffer_rx);
    if (actual_checksum != checksum) {
        mhz19c_log_error("failed to varify checksum. the expected value is %02x, but the actual value is %02x.", checksum, actual_checksum);
        return false;
    }

    // Return the received command.

    if (command != NULL) {
        *command = buffer_rx[RX_COMMAND];
    }

    // Return the received data.

    if (data != NULL) {
        for (size_t i = 0; i < data_size; i += 1) {
            data[i] = buffer_rx[RX_DATA(i)];
        }
    }

    return true;
}

// ----------------------------------------------------------------------------
// MH-Z19C

bool mhz19c_get_co2_ppm(const struct mhz19c_t *mhz19c, int *co2_ppm, int *temp_c) {
    mhz19c_log_verbose(mhz19c, "mhz19c_get_co2_ppm()");

    tcflush(mhz19c->fd, TCIOFLUSH);

    // Send command.

    if (!mhz19c_write(mhz19c, COM_GET_CO2_PPM, NULL, 0)) {
        return false;
    }

    // Read the return value.

    const size_t data_size = 3;
    uint8_t data[data_size];

    if (!mhz19c_read(mhz19c, NULL, data, data_size)) {
        return false;
    }

    // Return CO2 concentration (ppm) and temperature (°C).

    int _co2_ppm = data[0] << 8 | data[1];
    int _temp_c = data[2] - 40;

    mhz19c_log_verbose(mhz19c, "co2_ppm = %d", _co2_ppm);
    mhz19c_log_verbose(mhz19c, "temp_c = %d", _temp_c);

    if (co2_ppm != NULL) {
        *co2_ppm = _co2_ppm;
    }
    if (temp_c != NULL) {
        *temp_c = _temp_c;
    }

    return true;
}

bool mhz19c_set_auto_calib(const struct mhz19c_t *mhz19c, bool enabled) {
    mhz19c_log_verbose(mhz19c, "mhz19c_set_auto_calib(enabled = %d)", enabled);

    tcflush(mhz19c->fd, TCIOFLUSH);

    // Send command.

    const uint8_t data = enabled ? CALIB_ON : CALIB_OFF;
    if (!mhz19c_write(mhz19c, COM_SET_AUTO_CALIB, &data, 1)) {
        return false;
    }

    return true;
}

static uint8_t mhz19c_get_checksum(const uint8_t *buffer) {
    uint8_t checksum = 0x00;
    for (int i = 1; i <= 7; i += 1) {
        checksum += buffer[i];
    }
    return 0xff - checksum + 0x01;
}
