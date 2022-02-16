/*
 * co2SensorK30.cpp
 *
 * Created on: 2022-01-26
 *     Author: patw
 */

#include "co2SensorK30.h"

Co2SensorK30::Co2SensorK30(std::string co2Device) : timeoutMs_(100)
{
    sensorFileDesc_ = open(co2Device.c_str(), O_RDWR | O_NDELAY | O_NOCTTY);
    if (sensorFileDesc_ >= 0) {
        /* Cancel the O_NDELAY flag. */
        int flag = fcntl(sensorFileDesc_, F_GETFL, 0);
        fcntl(sensorFileDesc_, F_SETFL, flag & ~O_NDELAY);
    } else {
        syslog(LOG_ERR, "Cannot open CO2 port: \"%s\"", co2Device.c_str());
        throw CO2::exceptionLevel("Error opening CO2 port", true);
    }

    serialPort = new SerialPort(sensorFileDesc_);
    if (isatty(sensorFileDesc_)) {
        serialPort->setTerm(9600/*baud*/, SerialPort::None, 8/*bits*/, 1/*stop*/, 0, 0);
    }
}

Co2SensorK30::~Co2SensorK30()
{
    delete serialPort;
    if (close(sensorFileDesc_) < 0) {
        if (errno == EINTR) {
            // we were interrupted by a signal, so try again
            close(sensorFileDesc_);
        } else {
            syslog(LOG_ERR, "Error (%d) when closing CO2 port", errno);
        }
    }
}

int Co2SensorK30::checkCrc16(uint8_t* byteArray, int size)
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
            } else {
                crc16 >>= 1;
            }
        }
    }
    if (crc16 != crc16Actual) {
        syslog(LOG_ERR, "CRC16 act=%#4x  exp=%#4x\n", crc16Actual, crc16);
        return -1;
    }
    return 0;
}

/* Check if there is IO pending. */
int Co2SensorK30::co2CheckIo(uint8_t *buf, int bufsize, int *bytes_read)
{
    int n = 0, i;
    struct timeval tv;
    fd_set fds;

    tv.tv_sec = timeoutMs_ / 1000;
    tv.tv_usec = (timeoutMs_ % 1000) * 1000L;

    i = sensorFileDesc_;

    FD_ZERO(&fds);
    if (sensorFileDesc_ >= 0) {
        FD_SET(sensorFileDesc_, &fds);
    } else {
        sensorFileDesc_ = 0;
    }

    if (select(i + 1, &fds, NULL, NULL, &tv) > 0) {
        n = (FD_ISSET(sensorFileDesc_, &fds) > 0);
    }

    /* If there is data put it in the co2Buffer. */
    if (buf) {
        i = 0;
        if ((n & 1) == 1) {
            i = read(sensorFileDesc_, buf, bufsize);
        }
        buf[i > 0 ? i : 0] = 0;
        if (bytes_read) {
            *bytes_read = i;
        }
    }

    return n;
}

void Co2SensorK30::init()
{
    int n;
    uint8_t reply[100];
    uint32_t val;

    try {
        // flush anything ingressed on serial port
        (void)co2CheckIo(reply, sizeof(reply), &n);
        sendCmd(INITIATE, &val);
    } catch (CO2::exceptionLevel& el) {
        throw CO2::exceptionLevel(fmt::format("{}:{} - Error sending INITIATE to CO2 sensor ({})", __FUNCTION__, __LINE__, el.what()), el.isFatal());
    } catch (...) {
        throw;
    }

    // allow sensor time to ready itself
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

void Co2SensorK30::sendCmd(Co2SensorK30::Co2CmdType cmd, uint32_t* pVal)
{
    uint8_t reply[100];

    *pVal = 0;

    int n = write(sensorFileDesc_, co2CmdReply[cmd].cmd, co2CmdReply[cmd].cmdLen);
    if (n != co2CmdReply[cmd].cmdLen) {
        throw CO2::exceptionLevel(fmt::format("error writing to serial port ({}/{})",
                                              n, co2CmdReply[cmd].cmdLen), false);
    }

    // give co2 sensor time to reply
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    co2CheckIo(reply, co2CmdReply[cmd].replyLen, &n);
    if (n != co2CmdReply[cmd].replyLen) {
        throw CO2::exceptionLevel(fmt::format("error reading from serial port ({}/{})",
                                              n, co2CmdReply[cmd].cmdLen), false);
    }

    if ( (reply[0] != co2CmdReply[cmd].cmd[0]) ||
            (reply[1] != co2CmdReply[cmd].cmd[1]) ) {
        throw CO2::exceptionLevel(fmt::format("error in reply [{:#04x} {:#04x}]",
                                              reply[0], reply[1]), false);
    }

    if (checkCrc16(reply, co2CmdReply[cmd].replyLen)) {
        throw CO2::exceptionLevel("bad CRC", false);
    }

    if (co2CmdReply[cmd].replySize) {
        int i;

        for (i = 0; i < co2CmdReply[cmd].replySize; i++) {
            *pVal = (*pVal << 8) | (reply[co2CmdReply[cmd].replyPos + i] & 0xff);
        }
    }
}

int Co2SensorK30::readTemperature()
{
    uint32_t t;
    try {
        sendCmd(READ_TEMP, &t);
    } catch (CO2::exceptionLevel& el) {
        throw CO2::exceptionLevel(fmt::format("{}:{} - {}", __FUNCTION__, __LINE__, el.what()), el.isFatal());
    } catch (...) {
        throw;
    }
    return static_cast<int>(t & 0xffff);
}

int Co2SensorK30::readRelHumidity()
{
    uint32_t rh;
    try {
        sendCmd(READ_RH, &rh);
    } catch (CO2::exceptionLevel& el) {
        throw CO2::exceptionLevel(fmt::format("{}:{} - {}", __FUNCTION__, __LINE__, el.what()), el.isFatal());
    } catch (...) {
        throw;
    }
    return static_cast<int>(rh & 0xffff);
}

int Co2SensorK30::readCo2ppm()
{
    uint32_t co2ppm;
    try {
        sendCmd(READ_CO2, &co2ppm);
    } catch (CO2::exceptionLevel& el) {
        throw CO2::exceptionLevel(fmt::format("{}:{} - {}", __FUNCTION__, __LINE__, el.what()), el.isFatal());
    } catch (...) {
        throw;
    }
    if ((co2ppm & 0x7fff) == 0x7fff) {
        // need to reset sensor
        throw CO2::exceptionLevel(fmt::format("{}:{} - bad CO2 sensor reading", __FUNCTION__, __LINE__), false);
    }
    return static_cast<int>(co2ppm & 0xffff);
}

void Co2SensorK30::readMeasurements(int& co2ppm, int& temperature, int& relHumidity)
{
    co2ppm = this->readCo2ppm();
    temperature = this->readTemperature();
    relHumidity = this->readRelHumidity();
}

void Co2SensorK30::readMeasurements(float& co2ppm, float& temperature, float& relHumidity)
{
    co2ppm = this->readCo2ppm() * 1.0;
    temperature = (this->readTemperature() * 1.0) / 100.0;
    relHumidity = (this->readRelHumidity() * 1.0) / 100.0;
}

