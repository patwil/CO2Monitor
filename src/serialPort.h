/*
 * serialPort.h
 *
 * Created on: 2017-01-03
 *     Author: patw
 */

#ifndef SERIALPORT_H
#define SERIALPORT_H

#include <stdint.h>
#include <termios.h>

class SerialPort
{
    public:
        SerialPort(int fileDesc);

        virtual ~SerialPort();

        typedef enum {
            None,
            Odd,
            Even,
            Mark,
            Space
        } TermParity;

        void setTerm(int newSpeed, TermParity par, uint8_t nBits, uint8_t stopb,
                     int hwf, int swf);

    private:
        SerialPort();

        int fileDesc_;
        struct termios savetty;

        void setHwFlowCtl(int on);
        void setRts();
        void setTermSpeed(struct termios *tty, int newSpeed);
        /*
         * Set cbreak mode.
         * Mode 0 = normal.
         * Mode 1 = cbreak, no echo
         * Mode 2 = raw, no echo.
         * Mode 3 = only return erasechar (for wkeys.c)
         *
         * Returns: the current erase character.
         */
        int setcbreak(int mode);
    protected:
};




#endif /* SERIALPORT_H */
