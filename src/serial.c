/*
 * serial.c
 *
 *  Created on: 2015-07-26
 *      Author: patw
 */

#include "serial.h"

static struct termios savetty;

/* Set hardware flow control. */
void setHwFlowCtl(int fd, int on) {
	struct termios tty;

	tcgetattr(fd, &tty);
	if (on)
		tty.c_cflag |= CRTSCTS;
	else
		tty.c_cflag &= ~CRTSCTS;
	tcsetattr(fd, TCSANOW, &tty);
}

/* Set RTS line. Sometimes dropped. Linux specific? */
static void setRts(int fd) {
#if defined(TIOCM_RTS) && defined(TIOCMODG)
	{
		int mcs=0;

		ioctl(fd, TIOCMODG, &mcs);
		mcs |= TIOCM_RTS;
		ioctl(fd, TIOCMODS, &mcs);
	}
#endif
}

void setTermSpeed(int fd, struct termios *tty, int newSpeed) {
	int speed = -1;

	switch (newSpeed) {
	case 0:
#ifdef B0
		speed = B0;
#else
		speed = 0;
#endif
		break;
	case 3:
		speed = B300;
		break;
	case 6:
		speed = B600;
		break;
	case 12:
		speed = B1200;
		break;
	case 24:
		speed = B2400;
		break;
	case 48:
		speed = B4800;
		break;
	case 96:
		speed = B9600;
		break;
#ifdef B19200
	case 192:
		speed = B19200;
		break;
#else /* B19200 */
#  ifdef EXTA
		case 192: speed = EXTA; break;
#   else /* EXTA */
		case 192: speed = B9600; break;
#   endif /* EXTA */
#endif	 /* B19200 */
#ifdef B38400
	case 384:
		speed = B38400;
		break;
#else /* B38400 */
#  ifdef EXTB
		case 384: speed = EXTB; break;
#   else /* EXTB */
		case 384: speed = B9600; break;
#   endif /* EXTB */
#endif	 /* B38400 */
#ifdef B57600
	case 576:
		speed = B57600;
		break;
#endif
#ifdef B115200
	case 1152:
		speed = B115200;
		break;
#endif
#ifdef B230400
	case 2304:
		speed = B230400;
		break;
#endif
#ifdef B460800
	case 4608:
		speed = B460800;
		break;
#endif
#ifdef B500000
	case 5000:
		speed = B500000;
		break;
#endif
#ifdef B576000
	case 5760:
		speed = B576000;
		break;
#endif
#ifdef B921600
	case 9216:
		speed = B921600;
		break;
#endif
#ifdef B1000000
	case 10000:
		speed = B1000000;
		break;
#endif
#ifdef B1152000
	case 11520:
		speed = B1152000;
		break;
#endif
#ifdef B1500000
	case 15000:
		speed = B1500000;
		break;
#endif
#ifdef B2000000
	case 20000:
		speed = B2000000;
		break;
#endif
#ifdef B2500000
	case 25000:
		speed = B2500000;
		break;
#endif
#ifdef B3000000
	case 30000:
		speed = B3000000;
		break;
#endif
#ifdef B3500000
	case 35000:
		speed = B3500000;
		break;
#endif
#ifdef B4000000
	case 40000:
		speed = B4000000;
		break;
#endif
	default:
		break;
	}

	if (speed != -1) {
		cfsetospeed(tty, (speed_t) speed);
		cfsetispeed(tty, (speed_t) speed);
	}
}
/*
 * Set baudrate, parity and number of bits.
 */
void setTerm(int fd, int newSpeed, TermParity par, uint8_t nBits, uint8_t stopb,
		int hwf, int swf) {
	struct termios tty;

	tcgetattr(fd, &tty);

	/* We generate mark and space parity ourselves. */
	if (nBits == 7 && (par == TermParity_Mark || par == TermParity_Space)) {
		nBits = 8;
	}

	setTermSpeed(fd, &tty, newSpeed);

	switch (nBits) {
	case 5:
		tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS5;
		break;
	case 6:
		tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS6;
		break;
	case 7:
		tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS7;
		break;
	case 8:
	default:
		tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
		break;
	}
	/* Set into raw, no echo mode */
	tty.c_iflag = IGNBRK;
	tty.c_lflag = 0;
	tty.c_oflag = 0;
	tty.c_cflag |= CLOCAL | CREAD;
#ifdef _DCDFLOW
	tty.c_cflag &= ~CRTSCTS;
#endif
	tty.c_cc[VMIN] = 1;
	tty.c_cc[VTIME] = 5;

	if (swf) {
		tty.c_iflag |= IXON | IXOFF;
	} else {
		tty.c_iflag &= ~(IXON | IXOFF | IXANY);
	}

	tty.c_cflag &= ~(PARENB | PARODD);
	if (par == TermParity_Even) {
		tty.c_cflag |= PARENB;
	} else if (par == TermParity_Odd) {
		tty.c_cflag |= (PARENB | PARODD);
	}

	if (stopb == 2) {
		tty.c_cflag |= CSTOPB;
	} else {
		tty.c_cflag &= ~CSTOPB;
	}

	tcsetattr(fd, TCSANOW, &tty);

	setRts(fd);

#ifndef _DCDFLOW
	setHwFlowCtl(fd, hwf);
#endif
}

/*
 * Set cbreak mode.
 * Mode 0 = normal.
 * Mode 1 = cbreak, no echo
 * Mode 2 = raw, no echo.
 * Mode 3 = only return erasechar (for wkeys.c)
 *
 * Returns: the current erase character.
 */
int setcbreak(int mode) {
	struct termios tty;
	static int init = 0;
	static int erasechar;

#ifndef XCASE
#  ifdef _XCASE
#    define XCASE _XCASE
#  else
#    define XCASE 0
#  endif
#endif

	if (init == 0) {
		tcgetattr(0, &savetty);
		erasechar = savetty.c_cc[VERASE];
		init++;
	}

	if (mode == 3)
		return erasechar;

	/* Always return to default settings first */
	tcsetattr(0, TCSADRAIN, &savetty);

	if (mode == 0) {
		return erasechar;
	}

	tcgetattr(0, &tty);
	if (mode == 1) {
		tty.c_oflag &= ~OPOST;
		tty.c_lflag &= ~(XCASE | ECHONL | NOFLSH);
		tty.c_lflag &= ~(ICANON | ISIG | ECHO);
		tty.c_iflag &= ~(ICRNL | INLCR);
		tty.c_cflag |= CREAD;
		tty.c_cc[VTIME] = 5;
		tty.c_cc[VMIN] = 1;
	}
	if (mode == 2) { /* raw */
		tty.c_iflag &= ~(IGNBRK | IGNCR | INLCR | ICRNL | IUCLC | IXANY | IXON
				| IXOFF | INPCK | ISTRIP);
		tty.c_iflag |= (BRKINT | IGNPAR);
		tty.c_oflag &= ~OPOST;
		tty.c_lflag &= ~(XCASE | ECHONL | NOFLSH);
		tty.c_lflag &= ~(ICANON | ISIG | ECHO);
		tty.c_cflag |= CREAD;
		tty.c_cc[VTIME] = 5;
		tty.c_cc[VMIN] = 1;
	}
	tcsetattr(0, TCSADRAIN, &tty);
	return erasechar;
}

