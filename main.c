#include <stdio.h>
#include <string.h>

#include "mhz19c.h"

int main(int argc, char *argv[]) {
    mhz19c_t mhz19c = {};

    if (!mhz19c_open(&mhz19c)) {
        printf("Cannot open.\n");
        return 1;
    }

    int mode = 0;
    if (argc == 3 && strcmp(argv[1], "calib") == 0) {
        if (strcmp(argv[2], "on") == 0) {
            printf("set on\n");
            mode = 1;
        } else {
            printf("set off\n");
            mode = 2;
        }
    }

    if (mode == 0) {
        int co2 = mhz19c_get_co2_ppm(&mhz19c);
        printf("%d\n", co2);
    } else {
        mhz19c_set_auto_calib(&mhz19c, mode == 1);
    }

    mhz19c_close(&mhz19c);

    return 0;
}
