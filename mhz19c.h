#ifndef MHZ19C_H
#define MHZ19C_H

#include <stdint.h>
#include <stdbool.h>

struct mhz19c_t {
    bool verbose;
    int fd;
    char version[5];
};

void mhz19c_set_log_verbose(struct mhz19c_t *mhz19c, bool verbose);

bool mhz19c_open(struct mhz19c_t *mhz19c);
bool mhz19c_close(const struct mhz19c_t *mhz19c);

// 0x85: Read temperature as double (undocumented)
bool mhz19c_get_temperature(const struct mhz19c_t *mhz19c, float *temp);
// 0x86: Read CO2 concentration (Also read undocumented temperature as integer)
bool mhz19c_get_co2_ppm(const struct mhz19c_t *mhz19c, int *co2_ppm, int *temp);
// 0x87: Request zero calibration
bool mhz19c_zero_calibration(const struct mhz19c_t *mhz19c);
// 0x79: Set ABC logic on/off
bool mhz19c_set_abc(const struct mhz19c_t *mhz19c, bool is_on);
// 0x7d: Get ABC logic on/off
bool mhz19c_get_abc(const struct mhz19c_t *mhz19c, bool *is_on);

#endif /* MHZ19C_H */
