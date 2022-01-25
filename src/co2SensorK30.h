
/*
 * co2SensorK30.h
 *
 * Created on: 2022-01-18
 *     Author: patw
 */

#ifndef CO2SENSORK30_H
#define CO2SENSOR30_H

#include <cstdlib>
#include <string>
#include <cstring>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include "serialPort.h"

class Co2SensorK30
{
    public:
        typedef enum {
            INITIATE,
            READ_CO2,
            READ_TEMP,
            READ_RH,
            CO2_CMD_MAX
        } Co2CmdType;

        Co2Sensor(std::string co2Port);

        virtual ~Co2Sensor();

        void init();

        int readTemperature();
        int readRelHumidity();
        int readCo2ppm();
        virtual void readMeasurements(int& co2ppm, int& temperature, int& relHumidity);
        virtual void readMeasurements(float& co2ppm, float& temperature, float& relHumidity);

    private:
        typedef struct {
            int cmdLen;
            int replyLen;
            int replyPos;
            int replySize;
            uint8_t cmd[10];
        } Co2CmdReply;

        const Co2CmdReply co2CmdReply[CO2_CMD_MAX] = {
            { 8, 4, 0, 0, { 0xfe, 0x41, 0x00, 0x60, 0x01, 0x35, 0xe8, 0x53, 0, 0 } },
            { 7, 7, 3, 2, { 0xfe, 0x44, 0x00, 0x08, 0x02, 0x9f, 0x25, 0, 0, 0 } },
            { 7, 7, 3, 2, { 0xfe, 0x44, 0x00, 0x12, 0x02, 0x94, 0x45, 0, 0, 0 } },
            { 7, 7, 3, 2, { 0xfe, 0x44, 0x00, 0x14, 0x02, 0x97, 0xe5, 0, 0, 0 } }
        };

        int sensorFileDesc_;
        const int timeoutMs_;

        Co2Sensor();

        int sendCmd(Co2CmdType cmd, uint32_t* pVal);
        int co2CheckIo(uint8_t *buf, int bufsize, int *bytes_read);
        int checkCrc16(uint8_t* byteArray, int size);

        SerialPort* serialPort;

    protected:
};


#endif /* CO2SENSORK30_H */
