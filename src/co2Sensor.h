/*
 * co2Sensor.h
 *
 * Created on: 2017-01-03
 *     Author: patw
 */

#ifndef CO2SENSOR_H
#define CO2SENSOR_H

#include "utils.h"

class Co2Sensor
{
public:
    Co2Sensor() {};
    Co2Sensor(std::string co2Device);

    virtual ~Co2Sensor();

    virtual void init() {};

    virtual void readMeasurements(int& co2ppm, int& temperature, int& relHumidity) {};
    virtual void readMeasurements(float& co2ppm, float& temperature, float& relHumidity) {};

private:

protected:
};


#endif /* CO2SENSOR_H */
