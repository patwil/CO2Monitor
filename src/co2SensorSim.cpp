/*
 * co2SensorSim.cpp
 *
 * Created on: 2022-01-26
 *     Author: patw
 */

#include "co2SensorSim.h"

Co2SensorSim::Co2SensorSim()
{
}

Co2SensorSim::Co2SensorSim(const Co2SensorSim& rhs)
{
    // Assign new dynamic memory and and copy over data.

}


Co2SensorSim::~Co2SensorSim()
{
    // Delete all dynamic memory.
}

int Co2SensorSim::readTemperature()
{
    uint32_t t;
    int rc = sendCmd(READ_TEMP, &t);

    if (rc < 0) {
        return rc;
    }
    return static_cast<int>(t & 0xffff);
}

int Co2SensorSim::readRelHumidity()
{
    uint32_t rh;
    int rc = sendCmd(READ_RH, &rh);

    if (rc < 0) {
        return rc;
    }
    return static_cast<int>(rh & 0xffff);
}

int Co2SensorSim::readCo2ppm()
{
    uint32_t co2ppm;
    int rc = sendCmd(READ_CO2, &co2ppm);

    if (rc < 0) {
        return rc;
    }

    return static_cast<int>(co2ppm & 0xffff);
}


