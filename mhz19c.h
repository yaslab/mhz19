#ifndef MHZ19C_H
#define MHZ19C_H

#include <stdint.h>
#include <stdbool.h>

struct mhz19c_t {
    bool verbose;
    int fd;
};

void mhz19c_set_log_verbose(struct mhz19c_t *mhz19c, bool verbose);

bool mhz19c_open(struct mhz19c_t *mhz19c);
bool mhz19c_close(const struct mhz19c_t *mhz19c);

bool mhz19c_get_co2_ppm(const struct mhz19c_t *mhz19c, int *co2_ppm, int *temp_c);
bool mhz19c_set_auto_calib(const struct mhz19c_t *mhz19c, bool enabled);

#endif /* MHZ19C_H */
