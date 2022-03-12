/*
 * co2SensorSim.h
 *
 * Created on: 2022-01-26
 *     Author: patw
 */

#ifndef CO2SENSORSIM_H
#define CO2SENSORSIM_H

#include "co2Sensor.h"

class Co2SensorSim : public Co2Sensor
{
    public:
        Co2SensorSim();

        ~Co2SensorSim();

        void init();

        void readMeasurements(int& co2ppm, int& temperature, int& relHumidity);
        void readMeasurements(float& co2ppm, float& temperature, float& relHumidity);

    private:

        int i_;

    protected:
};



#endif /* CO2SENSORSIM_H */
