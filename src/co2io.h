/*
 * co2io.h
 *
 *  Created on: 2015-07-26
 *      Author: patw
 */

#ifndef CO2IO_H_
#define CO2IO_H_

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


typedef enum {
	CO2_CMD_INITIATE,
	CO2_CMD_READ_CO2,
	CO2_CMD_READ_TEMP,
	CO2_CMD_READ_RH,
	CO2_CMD_MAX
} Co2CmdType;

void co2Init(int termFd);
int sendCmd(int termFd, Co2CmdType cmd, uint32_t* pVal);


#endif /* CO2IO_H_ */
