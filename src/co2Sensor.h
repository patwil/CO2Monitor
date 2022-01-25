/*
 * co2Sensor.h
 *
 * Created on: 2017-01-03
 *     Author: patw
 */

#ifndef CO2SENSOR_H
#define CO2SENSOR_H


class Co2Sensor
{
    public:
        Co2Sensor(std::string co2Port);
        Co2Sensor(std::string co2Port);

        virtual ~Co2Sensor();

        void init();

        int readTemperature();
        int readRelHumidity();
        int readCo2ppm();
        virtual void readMeasurements(int& co2ppm, int& temperature, int& relHumidity);
        virtual void readMeasurements(float& co2ppm, float& temperature, float& relHumidity);

    private:
        Co2Sensor();

    protected:
};


#endif /* CO2SENSOR_H */
