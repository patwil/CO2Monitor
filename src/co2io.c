/*
 * co2io.c
 *
 *  Created on: 2015-07-26
 *      Author: patw
 */

#include <sys/param.h>
#include <stdbool.h>
#include <limits.h>
#include "co2io.h"
#include "serial.h"

#define BUFFSIZE 1024

typedef struct {
	int cmdLen;
	int replyLen;
	int replyPos;
	int replySize;
	uint8_t cmd[10];
} Co2CmdReply;

#if 0
static uint8_t co2Buffer[BUFFSIZE];
static unsigned int co2BuffReadIdx;
static unsigned int co2BuffWriteIdx;
static bool isEmpty;
static bool isFull;
#endif

static Co2CmdReply co2CmdReply[CO2_CMD_MAX] = {
	{ 8, 4, 0, 0, { 0xfe, 0x41, 0x00, 0x60, 0x01, 0x35, 0xe8, 0x53, 0, 0 } },
	{ 7, 7, 3, 2, { 0xfe, 0x44, 0x00, 0x08, 0x02, 0x9f, 0x25, 0, 0, 0 } },
	{ 7, 7, 3, 2, { 0xfe, 0x44, 0x00, 0x12, 0x02, 0x94, 0x45, 0, 0, 0 } },
	{ 7, 7, 3, 2, { 0xfe, 0x44, 0x00, 0x14, 0x02, 0x97, 0xe5, 0, 0, 0 } }
};

static int checkCrc16(uint8_t* byteArray, int size)
{
    int i, j;
    uint16_t crc16 = 0xffff;
    const uint16_t polyVal = 0xa001;
    uint16_t crc16Actual;

    if (size < 2) {
        return -1;
    }

    crc16Actual = ((byteArray[size-1] & 0xff) << 8) | (byteArray[size-2] & 0xff);

    for (i = 0; i < (size - 2); i++) {
        crc16 ^= byteArray[i] & 0xff;
        for (j = 0; j < 8; j++) {
            if (crc16 & 1) {
                crc16 >>= 1;
                crc16 ^= polyVal;
            }
            else {
               crc16 >>= 1;
            }
        }
    }
    if (crc16 != crc16Actual) {
    	fprintf(stderr, "CRC16 act=%#4x  exp=%#4x\n", crc16Actual, crc16);
    	return -1;
    }
    return 0;
}

#if 0
static int co2BuffRead(uint8_t* pTempBuff, unsigned int reqSize)
{
	int availBytes;

	if (isEmpty || !reqSize) {
		return 0;
	}

	if (co2BuffReadIdx > co2BuffWriteIdx) {
		availBytes = co2BuffWriteIdx - co2BuffReadIdx;
	} else {
		availBytes = BUFFSIZE - co2BuffReadIdx + co2BuffWriteIdx;
	}

	int nBytesRead = MIN(availBytes, reqSize);

	if (co2BuffWriteIdx > co2BuffReadIdx) {
		memcpy(pTempBuff, &co2Buffer[co2BuffReadIdx], nBytesRead);
		co2BuffReadIdx += nBytesRead;
	} else {
		if ( (co2BuffReadIdx + nBytesRead) < BUFFSIZE ) {
			memcpy(pTempBuff, &co2Buffer[co2BuffReadIdx], nBytesRead);
			co2BuffReadIdx += nBytesRead;
		} else {
			// co2Buffer has wrapped around
			int firstChunkSize = BUFFSIZE - co2BuffReadIdx;
			memcpy(pTempBuff, &co2Buffer[co2BuffReadIdx], firstChunkSize);
			co2BuffReadIdx = nBytesRead - firstChunkSize;
			memcpy(pTempBuff+firstChunkSize, &co2Buffer[0], co2BuffReadIdx);
		}
	}

	if (co2BuffReadIdx == co2BuffWriteIdx) {
		isEmpty = true;
	}
	isFull = false;

	return nBytesRead;
}

static int co2BuffWrite(uint8_t* pBytes, unsigned int reqSize)
{
	int availSpace = 0;

	if (isFull || !reqSize) {
		return 0;
	}

	if (co2BuffReadIdx > co2BuffWriteIdx) {
		availSpace = co2BuffReadIdx - co2BuffWriteIdx;
	} else {
		availSpace = BUFFSIZE - co2BuffWriteIdx + co2BuffReadIdx;
	}

	int nBytesWritten = MIN(availSpace, reqSize);

	if (co2BuffReadIdx > co2BuffWriteIdx) {
		memcpy(&co2Buffer[co2BuffWriteIdx], pBytes, nBytesWritten);
		co2BuffWriteIdx += nBytesWritten;
	} else {
		if ( (co2BuffWriteIdx + nBytesWritten) < BUFFSIZE ) {
			memcpy(&co2Buffer[co2BuffWriteIdx], pBytes, nBytesWritten);
			co2BuffWriteIdx += nBytesWritten;
		} else {
			// co2Buffer has wrapped around
			int firstChunkSize = BUFFSIZE - co2BuffWriteIdx;
			memcpy(&co2Buffer[co2BuffWriteIdx], pBytes, firstChunkSize);
			co2BuffWriteIdx = nBytesWritten - firstChunkSize;
			memcpy(&co2Buffer[0], pBytes+firstChunkSize, co2BuffWriteIdx);
		}
	}

	if (co2BuffReadIdx == co2BuffWriteIdx) {
		isFull = true;
	}
	isEmpty = false;

	return nBytesWritten;
}
#endif

/* Check if there is IO pending. */
static int co2CheckIo(int fd, int tmout, uint8_t *buf, int bufsize, int *bytes_read)
{
	int n = 0, i;
	struct timeval tv;
	fd_set fds;

	tv.tv_sec = tmout / 1000;
	tv.tv_usec = (tmout % 1000) * 1000L;

	i = fd;

	FD_ZERO(&fds);
	if (fd >= 0) {
		FD_SET(fd, &fds);
	} else {
		fd = 0;
	}

	if (select(i + 1, &fds, NULL, NULL, &tv) > 0) {
		n = (FD_ISSET(fd, &fds) > 0);
	}

	/* If there is data put it in the co2Buffer. */
	if (buf) {
		i = 0;
		if ((n & 1) == 1) {
			i = read(fd, buf, bufsize);
		}
		buf[i > 0 ? i : 0] = 0;
		if (bytes_read) {
			*bytes_read = i;
		}
	}

	return n;
}

void co2Init(int termFd)
{
	int n;
	uint8_t reply[100];

	// flush anything ingressed on serial port
	(void)co2CheckIo(termFd, 100, reply, sizeof(reply), &n);
#if 0
	memset(co2Buffer, 0, sizeof(co2Buffer));
	co2BuffReadIdx = co2BuffWriteIdx = 0;
	isEmpty = true;
	isFull = false;
#endif
}

int sendCmd(int termFd, Co2CmdType cmd, uint32_t* pVal)
{
	int rc = 0;
	uint8_t reply[100];

	*pVal = 0;

	int n = write(termFd, co2CmdReply[cmd].cmd, co2CmdReply[cmd].cmdLen);
	if (n != co2CmdReply[cmd].cmdLen) {
		fprintf(stderr, "%s:%u - error writing to serial port (%d/%d)\n",
				__FUNCTION__, __LINE__,
				n, co2CmdReply[cmd].cmdLen);
		return -1;
	}

	usleep(100000); // give co2 monitor time to reply

	rc = co2CheckIo(termFd, 100, reply, co2CmdReply[cmd].replyLen, &n);
	if (n != co2CmdReply[cmd].replyLen) {
		fprintf(stderr, "%s:%u - error reading from serial port (%d/%d)\n",
				__FUNCTION__, __LINE__,
				n, co2CmdReply[cmd].replyLen);
		return -2;
	}
	rc = 0;

	if ( (reply[0] != co2CmdReply[cmd].cmd[0]) ||
	     (reply[1] != co2CmdReply[cmd].cmd[1]) ) {
		fprintf(stderr, "%s:%u - error in reply [%#2x %#2x]\n",
				__FUNCTION__, __LINE__,
				reply[0], reply[1]);
		return -3;
	}

	if (checkCrc16(reply, co2CmdReply[cmd].replyLen)) {
		fprintf(stderr, "%s:%u - bad CRC\n",
				__FUNCTION__, __LINE__);
		return -4;
	}

	if (co2CmdReply[cmd].replySize) {
		int i;

		for (i = 0; i < co2CmdReply[cmd].replySize; i++) {
			*pVal = (*pVal << 8) | (reply[co2CmdReply[cmd].replyPos + i] & 0xff);
		}
	}

	return rc;
}

