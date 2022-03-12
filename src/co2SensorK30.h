/*
 * co2SensorK30.h
 *
 * Created on: 2022-01-26
 *     Author: patw
one line to give the program's name and an idea of what it does.
Copyright (C) yyyy  name of author

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef CO2SENSORK30_H
#define CO2SENSORK30_H

/*
“That great poets imitate and improve, whereas small ones steal and spoil.” W. H. Davenport Adams
http://books.google.com/books?id=5w34DT0fdeUC&q=%22ones+steal%22#v=snippet&
*/
#include "serialPort.h"
#include "co2Sensor.h"

class Co2SensorK30 : public Co2Sensor
{
    public:
        Co2SensorK30(std::string co2Device);

        virtual ~Co2SensorK30();

        virtual void init();

        void readMeasurements(int& co2ppm, int& temperature, int& relHumidity);
        void readMeasurements(float& co2ppm, float& temperature, float& relHumidity);

    private:
        typedef struct {
            int cmdLen;
            int replyLen;
            int replyPos;
            int replySize;
            uint8_t cmd[10];
        } Co2CmdReply;

        typedef enum {
            INITIATE,
            READ_CO2,
            READ_TEMP,
            READ_RH,
            CO2_CMD_MAX
        } Co2CmdType;

        const Co2CmdReply co2CmdReply[CO2_CMD_MAX] = {
            { 8, 4, 0, 0, { 0xfe, 0x41, 0x00, 0x60, 0x01, 0x35, 0xe8, 0x53, 0, 0 } },
            { 7, 7, 3, 2, { 0xfe, 0x44, 0x00, 0x08, 0x02, 0x9f, 0x25, 0, 0, 0 } },
            { 7, 7, 3, 2, { 0xfe, 0x44, 0x00, 0x12, 0x02, 0x94, 0x45, 0, 0, 0 } },
            { 7, 7, 3, 2, { 0xfe, 0x44, 0x00, 0x14, 0x02, 0x97, 0xe5, 0, 0, 0 } }
        };

        int sensorFileDesc_;
        const int timeoutMs_;

        Co2SensorK30();

        void sendCmd(Co2CmdType cmd, uint32_t* pVal);
        int co2CheckIo(uint8_t* buf, int bufsize, int* bytes_read);
        int checkCrc16(uint8_t* byteArray, int size);
        int readTemperature(void);
        int readRelHumidity(void);
        int readCo2ppm(void);

        SerialPort* serialPort;

    protected:
};

#endif /* CO2SENSORK30_H */
