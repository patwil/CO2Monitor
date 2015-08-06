/*
 * co2monitor.c
 *
 *  Created on: 2015-07-26
 *      Author: patw
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <stdbool.h>

#include "serial.h"
#include "co2io.h"

char* progName = "";

void usage(char* s) {
	fprintf(stderr, "%s: %s\nusage: %s <serial_port>\n", progName, s,
			progName);
}

int main(int argc, char* argv[]) {
	int termFd = -1;
	int rc = 0;

	progName = argv[0];
	if (argc < 2) {
		usage("too few args");
		exit(-1);
	}

	termFd = open(argv[1], O_RDWR | O_NDELAY | O_NOCTTY);
	if (termFd >= 0) {
		/* Cancel the O_NDELAY flag. */
		int n = fcntl(termFd, F_GETFL, 0);
		fcntl(termFd, F_SETFL, n & ~O_NDELAY);
	} else {
		usage("Cannot open serial port");
		exit(-2);
	}
	if (isatty(termFd)) {
		setTerm(termFd, 9600, TermParity_None, 8/*bits*/, 1/*stop*/, 0, 0);
	}

	do {
		uint32_t val;
		uint32_t co2ppm;
		uint32_t temperature;
		uint32_t relHumidity;

		rc = sendCmd(termFd, CO2_CMD_INITIATE, &val);
		if (rc) {
			break;
		}

		rc = sendCmd(termFd, CO2_CMD_READ_CO2, &co2ppm);
		if (rc) {
			break;
		}
		co2ppm &= 0xffff;

		rc = sendCmd(termFd, CO2_CMD_READ_TEMP, &temperature);
		if (rc) {
			break;
		}
		float fTemperature = ( (temperature & 0xffff) * 1.0) / 100.0;

		rc = sendCmd(termFd, CO2_CMD_READ_RH, &relHumidity);
		if (rc) {
			break;
		}
		float fRH = ( (relHumidity & 0xffff) * 1.0) / 100.0;

		printf("CO2 = %u ppm   Temp = %4.2fC   RH = %4.2f%%\n", co2ppm, fTemperature, fRH);
	} while (false);

	close(termFd);

	return 0;
}
