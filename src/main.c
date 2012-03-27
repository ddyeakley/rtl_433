/*
 * rtl-sdr, a poor man's SDR using a Realtek RTL2832 based DVB-stick
 * Copyright (C) 2012 by Steve Markgraf <steve@steve-m.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 *(at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>

#include "rtl-sdr.h"

#define READLEN		(16 * 16384)

static int do_exit = 0;

void usage(void)
{
	printf("rtl-sdr, an I/Q recorder for RTL2832 based USB-sticks\n\n"
		"Usage:\t -f frequency to tune to [Hz]\n"
		"\t[-s samplerate (default: 2048000 Hz)]\n"
		"\toutput filename\n");
	exit(1);
}

static void sighandler(int signum)
{
	do_exit = 1;
}

int main(int argc, char **argv)
{
	struct sigaction sigact;
	int r, opt;
	char *filename;
	uint32_t frequency = 0, samp_rate = 2048000;
	uint8_t buffer[READLEN];
    uint32_t n_read;
	FILE *file;
	rtlsdr_dev_t *dev = NULL;

	while ((opt = getopt(argc, argv, "f:s:")) != -1) {
		switch (opt) {
		case 'f':
			frequency = atoi(optarg);
			break;
		case 's':
			samp_rate = atoi(optarg);
			break;
		default:
			usage();
			break;
		}
	}

	if (argc <= optind) {
		usage();
	} else {
		filename = argv[optind];
	}

    rtlsdr_init();

    int device_count = rtlsdr_get_device_count();
    if (!device_count) {
        fprintf(stderr, "No supported devices found.\n");
        exit(1);
    }

    printf("Found %d device(s).\n", device_count);

    dev = rtlsdr_open(0); /* open the first device */
	if (NULL == dev) {
        fprintf(stderr, "Failed to open rtlsdr device.\n");
		exit(1);
	}

	sigact.sa_handler = sighandler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
	sigaction(SIGQUIT, &sigact, NULL);

	/* Set the sample rate */
    r = rtlsdr_set_sample_rate(dev, samp_rate);
	if (r < 0) {
        fprintf(stderr, "WARNING: Failed to set sample rate.\n");
	}

	/* Set the frequency */
	r = rtlsdr_set_center_freq(dev, frequency);
    if (r < 0) {
        fprintf(stderr, "WARNING: Failed to set center freq.\n");
    }

	file = fopen(filename, "wb");

	if (!file) {
		printf("Failed to open %s\n", filename);
		goto out;
	}

	/* Reset endpoint before we start reading from it */
	r = rtlsdr_reset_buffer(dev);
    if (r < 0) {
        fprintf(stderr, "WARNING: Failed to reset buffers.\n");
    }

	printf("Reading samples...\n");
	while (!do_exit) {
		r = rtlsdr_read_sync(dev, buffer, READLEN, &n_read);
        if (r < 0) {
                fprintf(stderr, "WARNING: sync read failed.\n");
        }

		fwrite(buffer, n_read, 1, file);

		if (n_read < READLEN) {
			printf("Short read, samples lost, exiting!\n");
			break;
		}
	}

	fclose(file);

    rtlsdr_exit();
out:
	return r >= 0 ? r : -r;
}
