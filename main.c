#include <stdio.h>
#include <string.h>

#include "mhz19c.h"

bool arg_get_co2;
bool arg_get_temp;
bool arg_set_calib;
bool arg_set_calib_is_on;
bool arg_get_calib;
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

    int status = 0;

    if (arg_set_calib) {
        if (!mhz19c_set_auto_calib(&mhz19c, arg_set_calib_is_on)) {
            status = 1;
            goto CLEAN_UP;
        }
    } else if (arg_get_calib) {
        bool is_on;
        if (!mhz19c_get_auto_calib(&mhz19c, &is_on)) {
            status = 1;
            goto CLEAN_UP;
        }
        printf("%s\n", is_on ? "on" : "off");
    } else {
        int co2_ppm;
        float temp;
        char text[64] = {};
        char work[64];
        if (arg_get_co2) {
            if (!mhz19c_get_co2_ppm(&mhz19c, &co2_ppm, NULL)) {
                status = 1;
                goto CLEAN_UP;
            }
            sprintf(work, "%d", co2_ppm);
            strcat(text, work);
        }
        if (arg_get_temp) {
            if (!mhz19c_get_temperature(&mhz19c, &temp)) {
                status = 1;
                goto CLEAN_UP;
            }
            if (arg_get_co2) {
                strcat(text, " ");
            }
            sprintf(work, "%.2f", temp);
            strcat(text, work);
        }
        printf("%s\n", text);
    }

CLEAN_UP:

    mhz19c_close(&mhz19c);

    return status;
}

static bool parse(int argc, char *argv[]) {
    if (argc == 1) {
        return false;
    }
    for (int i = 1; i < argc; i += 1) {
        if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--co2") == 0) {
            arg_get_co2 = true;
        } else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--temperature") == 0) {
            arg_get_temp = true;
        } else if (strcmp(argv[i], "--set-calib") == 0) {
            arg_set_calib = true;

            i += 1;
            if (i >= argc) {
                return false;
            }

            if (strcmp(argv[i], "off") == 0) {
                arg_set_calib_is_on = false;
            } else if (strcmp(argv[i], "on") == 0) {
                arg_set_calib_is_on = true;
            } else {
                return false;
            }
        } else if (strcmp(argv[i], "--get-calib") == 0) {
            arg_get_calib = true;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            arg_verbose = true;
        } else {
            return false;
        }
    }
    if (arg_set_calib && arg_get_calib) {
        return false;
    }
    if ((arg_set_calib || arg_get_calib) && (arg_get_co2 || arg_get_temp)) {
        return false;
    }
    return true;
}

static void usage() {
    fprintf(stderr, "syntax:\n");
    fprintf(stderr, "  mhz19c -c [-t] [-v]\n");
    fprintf(stderr, "  mhz19c -t [-c] [-v]\n");
    fprintf(stderr, "  mhz19c --set-calib <STATE> [-v]\n");
    fprintf(stderr, "  mhz19c --get-calib [-v]\n");
    fprintf(stderr, "options:\n");
    fprintf(stderr, "  -c, --co2            : Prints the co2 concentration.\n");
    fprintf(stderr, "  -t, --temperature    : Prints the temperature.\n");
    fprintf(stderr, "  --set-calib <STATE>  : Set the state of auto calibration. STATE=[on|off]\n");
    fprintf(stderr, "  --get-calib          : Get the state of auto calibration.\n");
    fprintf(stderr, "  -v, --verbose        : Set log level to verbose.\n");
}
