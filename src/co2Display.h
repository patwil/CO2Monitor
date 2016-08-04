/*
 * co2Display.h
 *
 * Created on: 2016-06-18
 *     Author: patw
 */

#ifndef CO2DISPLAY_H
#define CO2DISPLAY_H

#include <iostream>

class Co2Display
{
    public:
        Co2Display(zmq::context_t& ctx, int sockType);

        virtual ~Co2Display();

        void Co2Display::run()

    private:
        Co2Display();
        Co2Display(const Co2Display& rhs);
        Co2Display& operator=(const Co2Display& rhs);
        Co2Display* operator&();
        const Co2Display* operator&() const;

    protected:
};

#endif /* CO2DISPLAY_H */
