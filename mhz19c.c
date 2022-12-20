#include <stdio.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>

#include "mhz19c.h"

#define BUFFER_SIZE     (9)
#define RETRY_MAX       (10)

#define TX_START        (0)
#define TX_RESERVED     (1)
#define TX_COMMAND      (2)
#define TX_DATA(i)      (3+i)
#define TX_CHECKSUM     (8)

#define RX_START        (0)
#define RX_COMMAND      (1)
#define RX_DATA(i)      (2+i)
#define RX_CHECKSUM     (8)

#define START_VALUE     (0xff)
#define RESERVED_VALUE  (0x01)

#define COM_SET_AUTO_CALIB  (0x79)
#define COM_GET_AUTO_CALIB  (0x7d)
#define COM_GET_TEMPRATURE  (0x85)
#define COM_GET_CO2_PPM     (0x86)
#define COM_GET_VERSION     (0xa0)

#define SET_AUTO_CALIB_OFF  (0x00)
#define SET_AUTO_CALIB_ON   (0xa0)

static bool mhz19c_get_version(struct mhz19c_t *mhz19c);
static uint8_t mhz19c_get_checksum(const uint8_t *buffer);

// ----------------------------------------------------------------------------
// Logging Utility

#define mhz19c_log_verbose(mhz19c, ...) \
do {                                    \
    if (mhz19c->verbose) {              \
        fprintf(stderr, "verbose: ");   \
        fprintf(stderr, __VA_ARGS__);   \
        fprintf(stderr, "\n");          \
    }                                   \
} while(false)

#define mhz19c_log_error(...)           \
do {                                    \
    fprintf(stderr, "error: ");         \
    fprintf(stderr, __VA_ARGS__);       \
    fprintf(stderr, "\n");              \
} while(false)

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

    struct termios tio = {};

    if (tcgetattr(fd, &tio) < 0) {
        mhz19c_log_error("failed to get the termios state.");
        close(fd);
        return false;
    }

    // Set serial port baud rate be 9600.
    cfsetspeed(&tio, B9600);

    // Use raw mode. This also sets data bit to 8 bytes and parity bit null.
    cfmakeraw(&tio);

    // Set stop bit to 1 byte.
    tio.c_cflag &= ~((tcflag_t)CSTOPB);

    tio.c_cflag |= CREAD;
    tio.c_cflag |= CLOCAL;

    // Set min read bytes to 0.
    tio.c_cc[VMIN] = 0;
    // Set read timeout in 500 ms.
    tio.c_cc[VTIME] = 5;

    // Note:
    // This is equivalent to `ioctl(fd, TCSETS, &tio)`.
    // Use of ioctl makes for non-portable programs.
    if (tcsetattr(fd, TCSANOW, &tio) < 0) {
        mhz19c_log_error("failed to set the termios state.");
        close(fd);
        return false;
    }

    mhz19c->fd = fd;

    // Get firmware version.
    for (int i = 0; i < RETRY_MAX; i += 1) {
        if (mhz19c_get_version(mhz19c)) {
            break;
        }
    }

    return true;
}

bool mhz19c_close(const struct mhz19c_t *mhz19c) {
    mhz19c_log_verbose(mhz19c, "close the device.");

    if (close(mhz19c->fd) < 0) {
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

    buffer_tx[TX_CHECKSUM] = mhz19c_get_checksum(buffer_tx);

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

    if (tcdrain(mhz19c->fd) < 0) {
        mhz19c_log_error("failed to drain data.");
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
            mhz19c_log_error("failed to read data. (%zd)", count);
            return false;
        }

        mhz19c_log_verbose(mhz19c, "received %zd bytes.", count);

        total_count += (size_t)count;

        if (total_count == BUFFER_SIZE) {
            break;
        }
    }

    if (total_count != BUFFER_SIZE) {
        mhz19c_log_error("failed to read data. (%zu)", total_count);
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

    // Validate data with checksum.

    const uint8_t actual_checksum = mhz19c_get_checksum(buffer_rx);
    if (buffer_rx[RX_CHECKSUM] != actual_checksum) {
        mhz19c_log_error("failed to varify checksum. the expected value is %02x, but the actual value is %02x.", buffer_rx[RX_CHECKSUM], actual_checksum);
        return false;
    }

    mhz19c_log_verbose(mhz19c, "the checksum value (%02x) is correct.", actual_checksum);

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

bool mhz19c_get_temperature(const struct mhz19c_t *mhz19c, float *temp) {
    mhz19c_log_verbose(mhz19c, "mhz19c_get_temperature()");

    if (tcflush(mhz19c->fd, TCIOFLUSH) < 0) {
        mhz19c_log_error("failed to flush data.");
        return false;
    }

    // Send command.

    if (!mhz19c_write(mhz19c, COM_GET_TEMPRATURE, NULL, 0)) {
        return false;
    }

    // Read the return value.

    const size_t data_size = 4;
    uint8_t data[data_size];

    if (!mhz19c_read(mhz19c, NULL, data, data_size)) {
        return false;
    }

    // Return temperature (°C).

    float _temp = (float)(data[2] << 8 | data[3]) / 100.f;

    mhz19c_log_verbose(mhz19c, "temp = %f", _temp);

    if (temp != NULL) {
        *temp = _temp;
    }

    return true;
}

bool mhz19c_get_co2_ppm(const struct mhz19c_t *mhz19c, int *co2_ppm, int *temp) {
    mhz19c_log_verbose(mhz19c, "mhz19c_get_co2_ppm()");

    if (tcflush(mhz19c->fd, TCIOFLUSH) < 0) {
        mhz19c_log_error("failed to flush data.");
        return false;
    }

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
    int _temp = data[2] - 40;

    mhz19c_log_verbose(mhz19c, "co2_ppm = %d", _co2_ppm);
    mhz19c_log_verbose(mhz19c, "temp = %d", _temp);

    if (co2_ppm != NULL) {
        *co2_ppm = _co2_ppm;
    }
    if (temp != NULL) {
        *temp = _temp;
    }

    return true;
}

bool mhz19c_set_auto_calib(const struct mhz19c_t *mhz19c, bool is_on) {
    mhz19c_log_verbose(mhz19c, "mhz19c_set_auto_calib(is_on = %d)", is_on);

    if (tcflush(mhz19c->fd, TCIOFLUSH) < 0) {
        mhz19c_log_error("failed to flush data.");
        return false;
    }

    // Send command.

    const uint8_t data = is_on ? SET_AUTO_CALIB_ON : SET_AUTO_CALIB_OFF;
    if (!mhz19c_write(mhz19c, COM_SET_AUTO_CALIB, &data, 1)) {
        return false;
    }

    return true;
}

bool mhz19c_get_auto_calib(const struct mhz19c_t *mhz19c, bool *is_on) {
    mhz19c_log_verbose(mhz19c, "mhz19c_get_auto_calib()");

    if (tcflush(mhz19c->fd, TCIOFLUSH) < 0) {
        mhz19c_log_error("failed to flush data.");
        return false;
    }

    // Send command.

    if (!mhz19c_write(mhz19c, COM_GET_AUTO_CALIB, NULL, 0)) {
        return false;
    }

    // Read the return value.

    const size_t data_size = 6;
    uint8_t data[data_size];

    if (!mhz19c_read(mhz19c, NULL, data, data_size)) {
        return false;
    }

    // Return state of auto calibration.

    bool _is_on = data[5];

    mhz19c_log_verbose(mhz19c, "is_on = %d", _is_on);

    if (is_on != NULL) {
        *is_on = _is_on;
    }

    return true;
}

static bool mhz19c_get_version(struct mhz19c_t *mhz19c) {
    mhz19c_log_verbose(mhz19c, "mhz19c_get_version()");

    if (tcflush(mhz19c->fd, TCIOFLUSH) < 0) {
        mhz19c_log_error("failed to flush data.");
        return false;
    }

    // Send command.

    if (!mhz19c_write(mhz19c, COM_GET_VERSION, NULL, 0)) {
        return false;
    }

    // Read the return value.

    if (!mhz19c_read(mhz19c, NULL, mhz19c->version, 4)) {
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
