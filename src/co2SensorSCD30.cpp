/*
 * co2SensorSCD30.cpp
 *
 * Created on: 2022-01-26
 *     Author: patw
 */

#include <sstream>
#include <map>
#include <algorithm>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

extern "C" {
#include <i2c/smbus.h>
}

#ifdef DEBUG
#include <iostream>
#include <iomanip>
#endif /* DEBUG */*/

#include "co2SensorSCD30.h"

Co2SensorSCD30::Co2SensorSCD30(uint16_t bus)
{
    std::string i2cDeviceStr = "/dev/i2c-" + std::to_string(bus);
    i2cfd_ = open(i2cDeviceStr.c_str(), O_RDWR);
    if (i2cfd_ < 0) {
        throw scd30Exception("Failed to open I2c device \"" + i2cDeviceStr + "\" (" + strerror(errno) + ")");
    }
    if (ioctl(i2cfd_, I2C_SLAVE, i2cAddr_) < 0) {
        close(i2cfd_);
        i2cfd_ = -1;
        throw scd30Exception("Failed to set address for I2c device \"" + i2cDeviceStr + "\" (" + strerror(errno) + ")");
    }
}

Co2SensorSCD30::~Co2SensorSCD30()
{
    if (i2cfd_ >= 0) {
        StopContinuousMeasurement();
        close(i2cfd_);
        i2cfd_ = -1;
    }
}

// Polynomial: x^8 + x^5 + x^4 + 1 (0x31, MSB)
// Initialization: 0xFF
//
// Algorithm adapted from:
// https://en.wikipedia.org/wiki/Computation_of_cyclic_redundancy_checks
uint8_t Co2SensorSCD30::crc8(const uint8_t bytes[sizeof(uint16_t)])
{
    const uint8_t polynomial = 0x31;
    uint8_t       crc = 0xff;

    for (int j = 0; j < sizeof(uint16_t); j++) {
        crc ^= bytes[j];
        for (int i = 8; i; --i) {
            crc = (crc & 0x80) ? (crc << 1) ^ polynomial : (crc << 1);
        }
    }
    return crc;
}

float Co2SensorSCD30::bytes2float(uint8_t bytes[])
{
    union v {
        float    f;
        uint32_t u32;
    } val;

    val.u32 = (((uint32_t)bytes[0]) << 24) | (((uint32_t)bytes[1]) << 16)
              | (((uint32_t)bytes[2]) << 8)  | ((uint32_t)bytes[3]);
    return val.f;
}

void Co2SensorSCD30::sendCommand(Commands command, VU16& arglist)
{
    uint8_t buf[100];
    int i = 0;
    buf[i++] = (command >> 8) & 0xff;
    buf[i++] = command & 0xff;

for (auto& arg: arglist) {
        buf[i] = (arg >> 8) & 0xff;
        buf[i+1] = arg & 0xff;
        buf[i+2] = crc8(&buf[i]);
        i += 3;
    }
    int nBytes = write(i2cfd_, buf, i);
    if (nBytes != i) {
        throw scd30Exception("Send error: " + std::to_string(nBytes) + "/" + std::to_string(i) + " bytes written");
    }
    // The interface description suggests a >3ms delay between writes and
    // reads for most commands.
    usleep(5000);
}

void Co2SensorSCD30::sendCommand(Commands command, uint16_t arg)
{
    VU16 vArg {arg};
    sendCommand(command, vArg);
}

void Co2SensorSCD30::sendCommand(Commands command)
{
    VU16 dummyArg;
    sendCommand(command, dummyArg);
}

void Co2SensorSCD30::readResponse(int nResponseWords, VU16& responseWords)
{
    // read 3 bytes per response word (2 bytes per word + crc)
    const int nRespBytes = nResponseWords * 3;
    uint8_t buf[100];
    int nBytes = read(i2cfd_, buf, nRespBytes);
    if (nBytes != nRespBytes) {
        throw scd30Exception("Receive error: " + std::to_string(nBytes) + "/" + std::to_string(nRespBytes) + " bytes read");
    }
    for (int i = 0; i < nRespBytes; i += 3) {
        unsigned int exp = buf[i+2] & 0xff;
        unsigned int act = crc8(&buf[i]);
        if (act != exp) {
            throw scd30Exception("CRC error: " + std::to_string(exp) + " should be " + std::to_string(act) + " bytes read");
        }
        uint16_t responseWord = ((uint16_t)buf[i] << 8) | (uint16_t)buf[i+1];
        responseWords.push_back(responseWord);
    }
}

uint16_t Co2SensorSCD30::readResponse()
{
    VU16 response;
    readResponse(1, response);
    return response[0];
}

void Co2SensorSCD30::triggerContinuousMeasurement(uint16_t ambientPressure)
{
    if (ambientPressure && ((ambientPressure < 700) || (ambientPressure > 1400)) ) {
        throw scd30Exception("Ambient pressure must be set to either 0 or in the range [700..1400] mBar");
    }
    VU16 arg {ambientPressure};
    sendCommand(TriggerContMeasCmd, arg);
}

void Co2SensorSCD30::StopContinuousMeasurement(void)
{
    sendCommand(StopContMeasCmd);
}

uint16_t Co2SensorSCD30::setMeasurementInterval(uint16_t interval)
{
    if ( (interval < 2) || (interval > 1800) ) {
        throw scd30Exception("Interval must be in the range [2..1800] (sec)");
    }
    VU16 arg {interval};
    sendCommand(MeasIntervalCmd, arg);
    return readResponse();
}

uint16_t Co2SensorSCD30::measurementInterval(void)
{
    sendCommand(MeasIntervalCmd);
    return readResponse();
}

bool Co2SensorSCD30::dataReadyStatus(void)
{
    sendCommand(DataReadyStatusCmd);
    return readResponse() != 0;
}

void Co2SensorSCD30::readMeasurements(float& co2ppm, float& temperature, float& relHumidity)
{
    VU16 responseList;
    const int nMeasurements = 3;
    const int expectedResponseWords = nMeasurements * 2;
    sendCommand(ReadMeasurementCmd);
    readResponse(expectedResponseWords, responseList);
    if (responseList.size() != expectedResponseWords) {
        throw scd30Exception("Failed to read measurements - (actual:  " + std::to_string(responseList.size())
                             + ", expected: " + std::to_string(expectedResponseWords) + ")");
    }
    int i = 0;
    float measurements[3];
    for (int i = 0; i < nMeasurements; i++) {
        uint8_t bytes[4];
        int responseListIdx = i * 2;
        bytes[0] = (responseList[responseListIdx] >> 8) & 0xff;
        bytes[1] = responseList[responseListIdx] & 0xff;
        responseListIdx++;
        bytes[2] = (responseList[responseListIdx] >> 8) & 0xff;
        bytes[3] = responseList[responseListIdx] & 0xff;
        measurements[i] = bytes2float(bytes);
    }
    co2ppm = measurements[0];
    temperature = measurements[1];
    relHumidity = measurements[2];
}

void Co2SensorSCD30::activateAutomaticSelfCalibration(bool activate)
{
    VU16 vArg((activate ? 1 : 0));
    sendCommand(AscCmd, vArg);
}

bool Co2SensorSCD30::automaticSelfCalibration(void)
{
    sendCommand(AscCmd);
    return readResponse() != 0;
}

void Co2SensorSCD30::setForcedRecalibration(uint16_t frcOffset)
{
    if ( (frcOffset < 400) || (frcOffset > 2000) ) {
        throw scd30Exception("Forced Recalibration value must be in the range [400..2000] (ppm)");
    }
    sendCommand(FrcCmd, frcOffset);
}

uint16_t Co2SensorSCD30::forcedRecalibration(void)
{
    sendCommand(FrcCmd);
    return readResponse();
}

void Co2SensorSCD30::setTemperatureOffset(uint16_t tempOffset)
{
    sendCommand(TempOffsetCmd, tempOffset);
}

uint16_t Co2SensorSCD30::temperatureOffset(void)
{
    sendCommand(TempOffsetCmd);
    return readResponse();
}

void Co2SensorSCD30::setAltitudeCompensation(uint16_t metres)
{
    sendCommand(AltCompCmd, metres);
}

uint16_t Co2SensorSCD30::altitudeCompensation(void)
{
    sendCommand(AltCompCmd);
    return readResponse();
}

void Co2SensorSCD30::firmwareRevision(int& major, int& minor)
{
    uint16_t word;
    sendCommand(FirmwareRevCmd);
    word = readResponse();
    major = (word >> 8) & 0xff;
    minor = word & 0xff;
}

void Co2SensorSCD30::softReset(void)
{
    sendCommand(SoftResetCmd);
    usleep(50000);
}

static int findI2cDevOnBus(const int i2cBus, const int i2cDevAddr)
{
    std::string i2cDevFilename = "/dev/i2c-" + std::to_string(i2cBus);
    int rc = 0;

    int i2cFd = open(i2cDevFilename.c_str(), O_RDWR);
    if (i2cFd < 0) {
        rc = errno;
        throw scd30Exception("Unable to open I2C device file \"" + i2cDevFilename + "\" (" + strerror(rc) + ")");
    }
    if (ioctl(i2cFd, I2C_SLAVE, i2cDevAddr) < 0) {
        rc = errno;
        close(i2cFd);
        throw scd30Exception("Unable to set peripheral address " + std::to_string(i2cDevAddr) + " for \"" + i2cDevFilename + "\" (" + strerror(rc) + ")");
    }
    rc = i2c_smbus_write_quick(i2cFd, I2C_SMBUS_WRITE);
    close(i2cFd);
    return rc;
}

void Co2SensorSCD30::findI2cBusAll(std::vector<int>& i2cBusList)
{
    // "sysfs is always at /sys" according to www.kernel.org documentation;
    // so no need to do anything fancy like parse /proc/mounts to see
    // where sysfs is mounted.
    //
    const std::string i2cDevDir = "/sys/class/i2c-dev";
    struct dirent *pDirEnt;
    DIR *pDir;
    int rc = 0;

    pDir = opendir(i2cDevDir.c_str());
    if (pDir == NULL) {
        rc = errno;
        throw scd30Exception("Unable to open sysfs I2C device directory \"" + i2cDevDir + "\" (" + strerror(rc) + ")");
    }

    while ( (pDirEnt = readdir(pDir)) != NULL) {
        if (!strncmp(pDirEnt->d_name, ".", 2) || !strncmp(pDirEnt->d_name, "..", 3)) {
            continue;
        }
        int i2cBus = -1;
        if (!sscanf(pDirEnt->d_name, "i2c-%d", &i2cBus)) {
            continue;
        }
        // Only look in I2C busses, rather than SMBUS devices,
        // because of timing issues
        //
        if (i2cBus > 9) {
            continue;
        }
        if (findI2cDevOnBus(i2cBus, i2cAddr_) == 0) {
            i2cBusList.push_back(i2cBus);
        }
    }
    closedir(pDir);
}

int Co2SensorSCD30::findI2cBus(void)
{
    std::vector<int> i2cBusList;
    Co2SensorSCD30::findI2cBusAll(i2cBusList);
    if (i2cBusList.size() == 0) {
        return -1;
    }
    // sort I2C busses into ascending order,
    // so we will return matching one with the lowest number.
    std::sort(i2cBusList.begin(), i2cBusList.end());
    return i2cBusList.front();
}

