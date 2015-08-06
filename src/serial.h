/*
 * serial.h
 *
 *  Created on: 2015-07-26
 *      Author: patw
 */

#ifndef SERIAL_H_
#define SERIAL_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <sgtty.h>

typedef enum {
	TermParity_None,
	TermParity_Odd,
	TermParity_Even,
	TermParity_Mark,
	TermParity_Space
} TermParity;

void setTermSpeed(int fd, struct termios *tty, int newSpeed);

void setTerm(int fd, int newSpeed, TermParity par, uint8_t nBits, uint8_t stopb,
		     int hwf, int swf);

/*
 * Set cbreak mode.
 * Mode 0 = normal.
 * Mode 1 = cbreak, no echo
 * Mode 2 = raw, no echo.
 * Mode 3 = only return erasechar (for wkeys.c)
 *
 * Returns: the current erase character.
 */
int setcbreak(int mode);

#endif /* SERIAL_H_ */
