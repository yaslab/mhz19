#ifndef MHZ19C_H
#define MHZ19C_H

#include <stdint.h>
#include <stdbool.h>

struct mhz19c_t {
    int fd;
};

typedef struct mhz19c_t mhz19c_t;

bool mhz19c_open(mhz19c_t *mhz19c);
bool mhz19c_close(const mhz19c_t *mhz19c);

int mhz19c_get_co2_ppm(const mhz19c_t *mhz19c);
void mhz19c_set_auto_calib(const mhz19c_t *mhz19c, bool enabled);

#endif /* MHZ19C_H */
