/*
 * co2SensorSCD30.cpp
 *
 * Created on: 2022-01-26
 *     Author: patw
 */

#include <thread>          // std::thread
#include <fmt/core.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <unistd.h>

#ifdef HAS_I2C
extern "C" {
#include <i2c/smbus.h>
}
#endif /* HAS_I2C */

#include "co2SensorSCD30.h"

Co2SensorSCD30::Co2SensorSCD30(std::string i2cDevice) : 
    measurementInterval_(2),
    lastMeasurementTime_(0)
{
    i2cfd_ = open(i2cDevice.c_str(), O_RDWR);
    if (i2cfd_ < 0) {
        throw CO2::exceptionLevel(fmt::format("Failed to open I2c device \"{}\" ({})", i2cDevice, strerror(errno)), true);
    }
    if (ioctl(i2cfd_, I2C_SLAVE, i2cAddr_) < 0) {
        close(i2cfd_);
        i2cfd_ = -1;
        throw CO2::exceptionLevel(fmt::format("Failed to set address for I2c device \"{}\" ({})", i2cDevice, strerror(errno)), true);
    }
}

Co2SensorSCD30::Co2SensorSCD30(uint16_t bus) : Co2SensorSCD30("/dev/i2c-" + std::to_string(bus))
{
}

Co2SensorSCD30::~Co2SensorSCD30()
{
    if (i2cfd_ >= 0) {
        stopContinuousMeasurement();
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

    for (int j = 0; j < int(sizeof(uint16_t)); j++) {
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
        throw CO2::exceptionLevel(fmt::format("Send error: {}:{} bytes written", nBytes, i), false);
    }
    // The interface description suggests a >3ms delay between writes and
    // reads for most commands.
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
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
        throw CO2::exceptionLevel(fmt::format("Receive error: {}:{} bytes read", nBytes, nRespBytes), false);
    }
    for (int i = 0; i < nRespBytes; i += 3) {
        unsigned int exp = buf[i+2] & 0xff;
        unsigned int act = crc8(&buf[i]);
        if (act != exp) {
            throw CO2::exceptionLevel(fmt::format("CRC error - expected: {:#04x} - actual: {:#04x}", exp, act), false);
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
        throw CO2::exceptionLevel(fmt::format("Ambient pressure ({}) must be set to either 0 or in the range [700..1400] mBar",
                                  ambientPressure), false);
    }
    VU16 arg {ambientPressure};
    sendCommand(TriggerContMeasCmd, arg);
}

void Co2SensorSCD30::stopContinuousMeasurement(void)
{
    sendCommand(StopContMeasCmd);
}

uint16_t Co2SensorSCD30::setMeasurementInterval(uint16_t interval)
{
    if ( (interval < 2) || (interval > 1800) ) {
        throw CO2::exceptionLevel(fmt::format("Interval ({}) must be in the range [2..1800] (sec)", interval), false);
    }
    VU16 arg {interval};
    sendCommand(MeasIntervalCmd, arg);

    uint16_t newInterval = readResponse();
    if (newInterval != interval) {
        throw CO2::exceptionLevel(fmt::format("Measurement interval set error - actual: {}  -  expected: {}",newInterval, interval), false);
    }
    measurementInterval_ = newInterval;
    return measurementInterval_;
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

void Co2SensorSCD30::waitForDataReady(void)
{
    // We might have to wait if this has been called
    // before the measurement interval has elapsed.
    auto endTime = std::chrono::nanoseconds(lastMeasurementTime_) + std::chrono::seconds(measurementInterval_);
    auto waitTime = endTime - std::chrono::steady_clock::now().time_since_epoch();

    auto sleepTime = std::chrono::duration_cast<std::chrono::milliseconds>(waitTime);

    if (sleepTime.count() > 0) {
        // wait for the remainder of the current measurement interval to elapse
        std::this_thread::sleep_for(sleepTime);
    }

    // Although the sensor should be ready to read measurements
    // we'll try every 50ms. If it's still not ready after another
    // second, we'll cry "Uncle".
    auto timeStart = std::chrono::system_clock::now();
    while (!dataReadyStatus()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        auto timeNow = std::chrono::system_clock::now();
        if ( ((timeNow - timeStart) / std::chrono::seconds(1)) > 1 ) {
            throw CO2::exceptionLevel("Timed out waiting to read measurements", false);
        }
    }
}

void Co2SensorSCD30::readMeasurements(float& co2ppm, float& temperature, float& relHumidity)
{
    waitForDataReady();

    VU16 responseList;
    const int nMeasurements = 3;
    const int expectedResponseWords = nMeasurements * 2;
    sendCommand(ReadMeasurementCmd);
    readResponse(expectedResponseWords, responseList);
    if (responseList.size() != expectedResponseWords) {
        throw CO2::exceptionLevel(fmt::format("Failed to read measurements - (actual: {}, expected: {})",
                                              responseList.size(), expectedResponseWords), false);
    }

    lastMeasurementTime_ = std::chrono::steady_clock::now().time_since_epoch().count();

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

void Co2SensorSCD30::readMeasurements(int& co2ppm, int& temperature, int& relHumidity)
{
    float fCo2ppm;
    float fTemperature;
    float fRelHumidity;
    this->readMeasurements(fCo2ppm, fTemperature, fRelHumidity);
    co2ppm = int(fCo2ppm);
    temperature = int (fTemperature * 100.0);
    relHumidity = int(fRelHumidity * 100.0);
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
        throw CO2::exceptionLevel(fmt::format("Forced Recalibration value ({}) must be in the range [400..2000] (ppm)", frcOffset), false);
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
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

void Co2SensorSCD30::init(void)
{
    this->stopContinuousMeasurement();
    this->softReset();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    this->setMeasurementInterval(measurementInterval_);
    this->triggerContinuousMeasurement();
    lastMeasurementTime_ = std::chrono::steady_clock::now().time_since_epoch().count();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

static int findI2cDevOnBus(const int i2cBus, const int i2cDevAddr)
{
    std::string i2cDevFilename = "/dev/i2c-" + std::to_string(i2cBus);
    int rc = 0;

    int i2cFd = open(i2cDevFilename.c_str(), O_RDWR);
    if (i2cFd < 0) {
        rc = errno;
        throw CO2::exceptionLevel(fmt::format("Unable to open I2C device file \"{}\" ({})", i2cDevFilename, rc), false);
    }
    if (ioctl(i2cFd, I2C_SLAVE, i2cDevAddr) < 0) {
        rc = errno;
        close(i2cFd);
        throw CO2::exceptionLevel(fmt::format("Unable to set peripheral address {} for \"{}\" ({})", i2cDevAddr, i2cDevFilename, rc), false);
    }
#ifdef HAS_I2C
    rc = i2c_smbus_write_quick(i2cFd, I2C_SMBUS_WRITE);
#endif /* HAS_I2C */
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
        throw CO2::exceptionLevel(fmt::format("Unable to open sysfs I2C device directory \"{}\" ({})", i2cDevDir, rc), false);
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

