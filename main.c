#include <stdio.h>
#include <string.h>

#include "mhz19c.h"

bool arg_temp;
bool arg_calib;
bool arg_calib_enabled;
bool arg_verbose;

static bool parse(int argc, char *argv[]);
static void usage();

int main(int argc, char *argv[]) {
    if (!parse(argc, argv)) {
        usage();
        return 1;
    }

    struct mhz19c_t mhz19c = {};

    mhz19c_set_log_verbose(&mhz19c, arg_verbose);

    if (!mhz19c_open(&mhz19c)) {
        return 1;
    }

    if (arg_calib) {
        mhz19c_set_auto_calib(&mhz19c, arg_calib_enabled);
    } else {
        int co2_ppm, temp_c;
        if (mhz19c_get_co2_ppm(&mhz19c, &co2_ppm, &temp_c)) {
            if (arg_temp) {
                printf("%d %d\n", co2_ppm, temp_c);
            } else {
                printf("%d \n", co2_ppm);
            }
        }
    }

    mhz19c_close(&mhz19c);

    return 0;
}

static bool parse(int argc, char *argv[]) {
    for (int i = 1; i < argc; i += 1) {
        if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--temp") == 0) {
            arg_temp = true;
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--calib") == 0) {
            arg_calib = true;

            i += 1;
            if (i >= argc) {
                return false;
            }

            if (strcmp(argv[i], "off") == 0) {
                arg_calib_enabled = false;
            } else if (strcmp(argv[i], "on") == 0) {
                arg_calib_enabled = true;
            } else {
                return false;
            }
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            arg_verbose = true;
        } else {
            return false;
        }
    }
    return true;
}

static void usage() {
    fprintf(stderr, "syntax:\n");
    fprintf(stderr, "    mhz19c [-v] [-t]\n");
    fprintf(stderr, "    mhz19c [-v] -c <STATE>\n");
    fprintf(stderr, "options:\n");
    fprintf(stderr, "    -t, --temp           : Also prints the temperature.\n");
    fprintf(stderr, "    -c, --calib [on|off] : Set the state of auto calibration.\n");
    fprintf(stderr, "    -v, --verbose        : Set log level to verbose.\n");
}
