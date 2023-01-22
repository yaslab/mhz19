#include <stdio.h>
#include <string.h>

#include "mhz19c.h"

struct arguments {
    bool get_co2;
    bool get_temperature;
    bool set_abc;
    bool set_abc_is_on;
    bool get_abc;
    bool zero_calibration;
    bool get_version;
    bool verbose;
};

static bool parse(int argc, char *argv[], struct arguments *args);
static void usage();

int main(int argc, char *argv[]) {
    struct arguments args = {};
    if (!parse(argc, argv, &args)) {
        usage();
        return 1;
    }

    struct mhz19c_t mhz19c = {};

    mhz19c_set_log_verbose(&mhz19c, args.verbose);

    if (!mhz19c_open(&mhz19c)) {
        return 1;
    }

    int status = 0;

    if (args.get_version) {
        printf("%s\n", mhz19c.version);
    } else if (args.set_abc) {
        if (!mhz19c_set_abc(&mhz19c, args.set_abc_is_on)) {
            status = 1;
            goto CLEAN_UP;
        }
    } else if (args.get_abc) {
        bool is_on;
        if (!mhz19c_get_abc(&mhz19c, &is_on)) {
            status = 1;
            goto CLEAN_UP;
        }
        printf("%s\n", is_on ? "on" : "off");
    } else if (args.zero_calibration) {
        if (!mhz19c_zero_calibration(&mhz19c)) {
            status = 1;
            goto CLEAN_UP;
        }
    } else {
        int co2_ppm;
        float temp;
        char text[64] = {};
        char work[64];
        if (args.get_co2) {
            if (!mhz19c_get_co2_ppm(&mhz19c, &co2_ppm, NULL)) {
                status = 1;
                goto CLEAN_UP;
            }
            sprintf(work, "%d", co2_ppm);
            strcat(text, work);
        }
        if (args.get_temperature) {
            if (!mhz19c_get_temperature(&mhz19c, &temp)) {
                status = 1;
                goto CLEAN_UP;
            }
            if (args.get_co2) {
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

static bool parse(int argc, char *argv[], struct arguments *args) {
    if (argc == 1) {
        return false;
    }
    for (int i = 1; i < argc; i += 1) {
        if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--co2") == 0) {
            args->get_co2 = true;
        } else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--temperature") == 0) {
            args->get_temperature = true;
        } else if (strcmp(argv[i], "--set-abc") == 0) {
            args->set_abc = true;

            i += 1;
            if (i >= argc) {
                return false;
            }

            if (strcmp(argv[i], "off") == 0) {
                args->set_abc_is_on = false;
            } else if (strcmp(argv[i], "on") == 0) {
                args->set_abc_is_on = true;
            } else {
                return false;
            }
        } else if (strcmp(argv[i], "--get-abc") == 0) {
            args->get_abc = true;
        } else if (strcmp(argv[i], "--zero-calibration") == 0) {
            args->zero_calibration = true;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            args->get_version = true;
        } else if (strcmp(argv[i], "--verbose") == 0) {
            args->verbose = true;
        } else {
            return false;
        }
    }
    if (args->set_abc && args->get_abc) {
        return false;
    }
    if ((args->set_abc || args->get_abc) && (args->get_co2 || args->get_temperature)) {
        return false;
    }
    if (args->get_version && argc != 2) {
        return false;
    }
    return true;
}

static void usage() {
    fprintf(stderr, "syntax:\n");
    fprintf(stderr, "  mhz19c [-c] [-t]\n");
    fprintf(stderr, "  mhz19c --get-abc\n");
    fprintf(stderr, "  mhz19c --set-abc <STATE>\n");
    fprintf(stderr, "  mhz19c --zero-calibration\n");
    fprintf(stderr, "  mhz19c -v\n");
    fprintf(stderr, "options:\n");
    fprintf(stderr, "  -c, --co2          : Prints the CO2 concentration.\n");
    fprintf(stderr, "  -t, --temperature  : Prints the temperature.\n");
    fprintf(stderr, "  --get-abc          : Get the state of ABC logic.\n");
    fprintf(stderr, "  --set-abc <STATE>  : Set the state of ABC logic. STATE=[on|off]\n");
    fprintf(stderr, "  --zero-calibration : Request zero caribration.\n");
    fprintf(stderr, "  -v, --version      : Prints the firmware version.\n");
    fprintf(stderr, "  --verbose          : Prints verbose log.\n");
}
