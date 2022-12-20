#ifndef MHZ19C_H
#define MHZ19C_H

#include <stdint.h>
#include <stdbool.h>

struct mhz19c_t {
    bool verbose;
    int fd;
    uint8_t version[4];
};

void mhz19c_set_log_verbose(struct mhz19c_t *mhz19c, bool verbose);

bool mhz19c_open(struct mhz19c_t *mhz19c);
bool mhz19c_close(const struct mhz19c_t *mhz19c);

bool mhz19c_get_temperature(const struct mhz19c_t *mhz19c, float *temp);
bool mhz19c_get_co2_ppm(const struct mhz19c_t *mhz19c, int *co2_ppm, int *temp);
bool mhz19c_set_auto_calib(const struct mhz19c_t *mhz19c, bool is_on);
bool mhz19c_get_auto_calib(const struct mhz19c_t *mhz19c, bool *is_on);

#endif /* MHZ19C_H */
