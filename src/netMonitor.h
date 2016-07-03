/*
 * netMonitor.h
 *
 * Created on: 2016-02-06
 *     Author: patw
 */

#ifndef NETMONITOR_H
#define NETMONITOR_H

#include <iostream>

#include <ctime>
#include <sys/time.h>
class server_worker {
public:
    server_worker(zmq::context_t &ctx, int sock_type)
        : ctx_(ctx),
          worker_(ctx_, sock_type)
    {}

    void work() {
            worker_.connect("inproc://backend");

        try {
            while (true) {
                zmq::message_t identity;
                zmq::message_t msg;
                zmq::message_t copied_id;
                zmq::message_t copied_msg;
                worker_.recv(&identity);
                worker_.recv(&msg);

                int replies = within(5);
                for (int reply = 0; reply < replies; ++reply) {
                    s_sleep(within(1000) + 1);
                    copied_id.copy(&identity);
                    copied_msg.copy(&msg);
                    worker_.send(copied_id, ZMQ_SNDMORE);
                    worker_.send(copied_msg);
                }
            }
        }
        catch (std::exception &e) {}
    }



private:
};

class NetMonitor
{
    typedef enum {
        Start,
        Up,
        NoConnection,
        NetDeviceDown,
        NetDeviceMissing,
        NoNetDevices,
        Invalid
    } State;

    NetMonitor();
    NetMonitor(const NetMonitor& rhs);
    NetMonitor& operator= (const NetMonitor& rhs);
    NetMonitor* operator& ();
    const NetMonitor* operator& () const;

    int getGCD(int a, int b);

    time_t networkCheckPeriod_;
    time_t wdogKickPeriod_;

    std::string netDevice_;
    time_t netDeviceDownRebootMinTime_;

    bool shouldTerminate_;
    State currentState_;
    State prevState_;
    zmq::context_t &ctx_;
    zmq::socket_t mainSocket_;

public:
    NetMonitor(zmq::context_t &ctx, int sockType);

    ~NetMonitor();

    State precheckNetInterfaces();
    void run();
    void terminate();
};

#endif /* NETMONITOR_H */
