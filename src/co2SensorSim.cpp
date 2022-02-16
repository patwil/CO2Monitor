/*
 * co2SensorSim.cpp
 *
 * Created on: 2022-01-26
 *     Author: patw
 */

#include "co2SensorSim.h"

Co2SensorSim::Co2SensorSim() : i_(0)
{
}

Co2SensorSim::~Co2SensorSim()
{
}

void Co2SensorSim::init()
{
    i_ = 0;
}

void Co2SensorSim::readMeasurements(int& co2ppm, int& temperature, int& relHumidity)
{
    temperature = 1234 + (100 * (i_ % 18)) + (i_ % 23);
    relHumidity = 3456 + (100 * (i_ % 27)) + (i_ % 19);
    co2ppm = 250 + (i_ % 450);
    i_++;
    if (i_ > 1000000) {
        i_ = 0;
    }
}

void Co2SensorSim::readMeasurements(float& co2ppm, float& temperature, float& relHumidity)
{
    int co2ppmInt;
    int temperatureInt;
    int relHumidityInt;
    this->readMeasurements(co2ppmInt, temperatureInt, relHumidityInt);
    co2ppm = co2ppmInt * 1.0;
    temperature = (temperatureInt * 1.0) / 100.0;
    relHumidity = (relHumidityInt * 1.0) / 100.0;
}
