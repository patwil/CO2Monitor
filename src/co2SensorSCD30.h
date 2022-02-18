/*
 * co2SensorSCD30.h
 *
 * Created on: 2022-01-26
 *     Author: patw
 */

#ifndef CO2SENSORSCD30_H
#define CO2SENSORSCD30_H

#include <vector>
#include <string>
#include <unistd.h>
#include <stdint.h>
#include <chrono>
#include "co2Sensor.h"

using VU16 = std::vector<uint16_t>;

class Co2SensorSCD30 : public Co2Sensor
{
public:
    Co2SensorSCD30(std::string i2cDevice);
    Co2SensorSCD30(uint16_t bus);
    ~Co2SensorSCD30();
    void triggerContinuousMeasurement(uint16_t ambientPressure = 0);
    void stopContinuousMeasurement(void);
    uint16_t setMeasurementInterval(uint16_t interval);
    uint16_t measurementInterval(void);
    bool dataReadyStatus(void);
    void activateAutomaticSelfCalibration(bool activate);
    void waitForDataReady(void);
    bool automaticSelfCalibration(void);
    void setForcedRecalibration(uint16_t co2ppm);
    uint16_t forcedRecalibration(void);
    void setTemperatureOffset(uint16_t tempOffset);
    uint16_t temperatureOffset(void);
    void setAltitudeCompensation(uint16_t metres);
    uint16_t altitudeCompensation(void);
    void firmwareRevision(int& major, int& minor);
    void softReset(void);

    virtual void init();

    int readTemperature();
    int readRelHumidity();
    int readCo2ppm();
    void readMeasurements(int& co2ppm, int& temperature, int& relHumidity);
    void readMeasurements(float& co2ppm, float& temperature, float& relHumidity);

    static void findI2cBusAll(std::vector<int>& i2cBusList);
    static int findI2cBus(void);
    static void findI2cDeviceAll(std::vector<std::string>& i2cDeviceList);
    static void findI2cDevice(std::string& i2cDevice);

private:
    typedef enum {
        TriggerContMeasCmd = 0x0010,
        StopContMeasCmd    = 0x0104,
        MeasIntervalCmd    = 0x4600,
        DataReadyStatusCmd = 0x0202,
        ReadMeasurementCmd = 0x0300,
        AscCmd             = 0x5306,
        FrcCmd             = 0x5204,
        TempOffsetCmd      = 0x5403,
        AltCompCmd         = 0x5102,
        FirmwareRevCmd     = 0xd100,
        SoftResetCmd       = 0xd304
    } Commands;

    int i2cfd_;
    static const int i2cAddr_ = 0x61;

    uint16_t measurementInterval_;
    int64_t lastMeasurementTime_;

    Co2SensorSCD30();
    void sendCommand(Commands command, VU16& arglist);
    void sendCommand(Commands command, uint16_t arg);
    void sendCommand(Commands command);
    void readResponse(int nResponseWords, VU16& responseWords);
    uint16_t readResponse(void);
    uint8_t crc8(const uint8_t bytes[sizeof(uint16_t)]);
    float bytes2float(uint8_t bytes[]);

protected:
};

#endif /* CO2SENSORSCD30_H */
