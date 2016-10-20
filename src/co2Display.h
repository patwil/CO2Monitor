/*
 * co2Display.h
 *
 * Created on: 2016-06-18
 *     Author: patw
 */

#ifndef CO2DISPLAY_H
#define CO2DISPLAY_H

#include <iostream>
#include <zmq.hpp>

class Co2Display
{
    public:
        Co2Display(zmq::context_t& ctx, int sockType);

        virtual ~Co2Display();

        void run();

    private:
        Co2Display();

    protected:
};

#endif /* CO2DISPLAY_H */
