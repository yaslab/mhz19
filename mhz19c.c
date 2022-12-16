#include <stdio.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include "mhz19c.h"

#define BUFFER_SIZE 8

#define TX_START        0
#define TX_RESERVED     1
#define TX_COMMAND      2
#define TX_DATA(i)      3+i
#define TX_CHECK_SUM    8

#define RX_START        0
#define RX_COMMAND      1
#define RX_DATA(i)      2+i
#define RX_CHECK_SUM    8

#define START_VALUE 0xff

#define RESERVED_VALUE 0x01

#define COM_READ_CO2 0x86
#define COM_SET_SELF_CALIBRATION 0x79

#define CALIB_ON 0xa0
#define CALIB_OFF 0x00





bool mhz19c_open(mhz19c_t *mhz19c) {
    int fd = open("/dev/serial0", O_RDWR);
    if (fd < 0) {
        return false;
    }

    // By default stop bit 1 byte and parity bit null.
    struct termios tio = {};
    // Use raw mode. This also sets data bit to 8 bytes.
    cfmakeraw(&tio);
    // Set serial port baud rate be 9600.
    cfsetspeed(&tio, B9600);

    tio.c_cflag |= CREAD;
    tio.c_cflag |= CLOCAL;

    // Note:
    // This is equivalent to `ioctl(fd, TCSETS, &tio)`.
    // Use of ioctl makes for non-portable programs.
    int result = tcsetattr(fd, TCSANOW, &tio);
    if (result < 0) {
        close(fd);
        return false;
    }

    mhz19c->fd = fd;

    return true;
}

bool mhz19c_close(const mhz19c_t *mhz19c) {
    return close(mhz19c->fd) == 0;
}

static uint8_t mhz19c_get_checksum(const uint8_t *buffer) {
    uint8_t checksum = 0x00;
    for (int i = 1; i <= 7; i += 1) {
        checksum += buffer[i];
    }
    return 0xff - checksum + 0x01;
}

static bool mhz19c_write(const mhz19c_t *mhz19c, uint8_t command, const uint8_t *data, size_t data_size) {
    // Write data.

    uint8_t buffer_tx[BUFFER_SIZE] = {};

    buffer_tx[TX_START] = START_VALUE;
    buffer_tx[TX_RESERVED] = RESERVED_VALUE;
    buffer_tx[TX_COMMAND] = command;

    for (size_t i = 0; i < data_size; i += 1) {
        buffer_tx[TX_DATA(i)] = data[i];
    }

    ssize_t count = write(mhz19c->fd, buffer_tx, BUFFER_SIZE);
    if (count != BUFFER_SIZE) {
        return false;
    }

    // Write checksum.

    const uint8_t checksum = mhz19c_get_checksum(buffer_tx);

    count = write(mhz19c->fd, &checksum, 1);
    if (count != 1) {
        return false;
    }

    return true;
}

static bool mhz19c_read(const mhz19c_t *mhz19c, uint8_t *command, uint8_t *data, size_t data_size) {
    // Read data.

    uint8_t buffer_rx[BUFFER_SIZE] = {};
    size_t pos = 0;

    for (int i = 0; i < 10; i += 1) {
        const size_t rem = BUFFER_SIZE - pos;
        if (rem <= 0) {
            break;
        }

        ssize_t count = read(mhz19c->fd, buffer_rx + pos, rem);
        if (count < 0) {
            // error
            break;
        }
        pos += (size_t)count;

        usleep(10);
    }

    // Read checksum.

    uint8_t checksum = 0;

    for (int i = 0; i < 10; i += 1) {
        const ssize_t count = read(mhz19c->fd, &checksum, 1);
        if (count < 0) {
            // error
            break;
        }
        if (count == 1) {
            break;
        }

        usleep(10);
    }

    // Validate data with checksum.

    const uint8_t actual_checksum = mhz19c_get_checksum(buffer_rx);
    if (actual_checksum != checksum) {
        // error
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

int mhz19c_get_co2_ppm(const mhz19c_t *mhz19c) {
    // Send command.

    if (!mhz19c_write(mhz19c, COM_READ_CO2, NULL, 0)) {
        printf("werror\n");
        return -1;
    }

    // Read the return value.

    const size_t data_size = 2;
    uint8_t data[data_size];

    if (!mhz19c_read(mhz19c, NULL, data, data_size)) {
        printf("rerror\n");
        return -1;
    }

    // Return CO2 concentration (ppm).

    return data[0] << 8 | data[1];
}

void mhz19c_set_auto_calib(const mhz19c_t *mhz19c, bool enabled) {
    const uint8_t data = enabled ? CALIB_ON : CALIB_OFF;
    mhz19c_write(mhz19c, COM_SET_SELF_CALIBRATION, &data, 1);
}
