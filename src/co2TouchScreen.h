/*
 * co2TouchScreen.h
 *
 * Created on: 2016-11-13
 *     Author: patw
 */

#ifndef CO2TOUCHSCREEN_H
#define CO2TOUCHSCREEN_H

//#include <thread>         // std::thread
//#include <mutex>          // std::mutex
//#include <vector>
#include <atomic>
//#include <cstdlib>
//#include <cstring>
//#include <syslog.h>
//#include <unistd.h>
//#include <signal.h>
//#include "SDL.h"
//#include "SDL_thread.h"
//#include <SDL_ttf.h>
#include <sys/types.h>
//#include <time.h>

#ifdef HAS_WIRINGPI
//#include <wiringPi.h>
#endif

#ifndef EV_SYN
#define EV_SYN 0
#endif
#ifndef SYN_MAX
#define SYN_MAX 3
#define SYN_CNT (SYN_MAX + 1)
#endif
#ifndef SYN_MT_REPORT
#define SYN_MT_REPORT 2
#endif
#ifndef SYN_DROPPED
#define SYN_DROPPED 3
#endif

class Co2TouchScreen
{
    public:
        Co2TouchScreen();

        void init(std::string mouseDevice);
        void buttonInit();
        void run();
        void stop();

        ~Co2TouchScreen();

        typedef enum {
            Button1,
            Button2,
            Button3,
            Button4,
            ButtonMax
        } Buttons;

        typedef enum {
            TouchDown = SDL_USEREVENT,
            TouchUp,
            ButtonPush,
            Timer,
            Signal
        } EventType;

        static void button1Action();
        static void button2Action();
        static void button3Action();
        static void button4Action();
        static void buttonAction(int button);

        typedef void (*ButtonAction)(void);
        typedef struct {
            int          gpioPin;
            ButtonAction action;
        } ButtonInfo;

        static uint32_t sendTimerEvent(uint32_t interval, void* arg);

    private:

        std::atomic<bool> shouldTerminate_;
        int mouseDeviceFd_;

        static const uint32_t kDebounceMilliSecs_;

    protected:
};


#endif /* CO2TOUCHSCREEN_H */
