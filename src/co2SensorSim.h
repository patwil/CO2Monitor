/*
 * co2SensorSim.h
 *
 * Created on: 2022-01-26
 *     Author: patw
 */

#ifndef CO2SENSORSIM_H
#define CO2SENSORSIM_H

#include <iostream>

class Co2SensorSim
{
public:
    Co2SensorSim(std::string co2Device);

    virtual ~Co2SensorSim();

    virtual void init();

    virtual int readTemperature();
    virtual int readRelHumidity();
    virtual int readCo2ppm();
    virtual void readMeasurements(int& co2ppm, int& temperature, int& relHumidity);
    virtual void readMeasurements(float& co2ppm, float& temperature, float& relHumidity);

private:
    Co2SensorSim();

protected:
protected:
};



#endif /* CO2SENSORSIM_H */
