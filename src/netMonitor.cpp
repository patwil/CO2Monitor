/*
 * netMonitor.cpp
 *
 * Created on: 2016-02-06
 *     Author: patw
 */

#include <vector>
#include <thread>
#include <memory>
#include <functional>
#include <unordered_map>
#include <cstdlib>
#include <cstring>
#include <syslog.h>
#include <dirent.h>

#include <zmq.hpp>

#include "co2Message.pb.h"
#include <google/protobuf/text_format.h>

#include "netMonitor.h"
#include "ping.h"
#include "netLink.h"
#include "utils.h"

#ifdef SYSTEMD_WDOG
#include "sysdWatchdog.h"
#endif

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
    zmq::context_t &ctx_;
    zmq::socket_t worker_;
};


NetMonitor::NetMonitor(zmq::context_t &ctx, int sockType) :
    ctx_(ctx),
    mainSocket_(ctx, sockType),
    shouldTerminate_(false),
    currentState_(Start),
    prevState_(Start)
{
    try {
        ConfigMap* pCfg = globals->getCfg();
        int tempInt;

        if (pCfg->find("NetDevice") != pCfg->end()) {
            //const char* p = pCfg->find("NetDevice")->second->getStr();
            //netDevice_ = string(p);
            netDevice_ = string(pCfg->find("NetDevice")->second->getStr());
        }
        if (pCfg->find("NetworkCheckPeriod") != pCfg->end()) {
            tempInt = pCfg->find("NetworkCheckPeriod")->second->getInt();
            networkCheckPeriod_ = (time_t)tempInt;
        }
        if (pCfg->find("WatchdogKickPeriod") != pCfg->end()) {
            tempInt = pCfg->find("WatchdogKickPeriod")->second->getInt();
            wdogKickPeriod_ = (time_t)tempInt;
        }
        if (pCfg->find("NetDeviceDownRebootMinTime") != pCfg->end()) {
            tempInt = pCfg->find("NetDeviceDownRebootMinTime")->second->getInt();
            netDeviceDownRebootMinTime_ = (time_t)tempInt;
        }
    } catch (std::exception& e) {
        throw e;
    } catch (...) {
        throw;
    }
    syslog(LOG_DEBUG, "NetMonitor init: NetDevice=\"%s\"  NetworkCheckPeriod=%lus  NetDeviceDownRebootMinTime=%lus",
                      netDevice_.c_str(), networkCheckPeriod_, netDeviceDownRebootMinTime_);
}

NetMonitor::~NetMonitor()
{
    // Delete all dynamic memory.
}

State NetMonitor::checkNetInterfacesPresent()
{
    DIR* pd;
    struct dirent* p;
    State netState = NoNetDevices;

    pd = opendir("/sys/class/net");
    if (pd) {
        while (p = readdir(pd)) {
            if (!strncmp(p->d_name, netDevice_.c_str(), netDevice_.length())) {
                netState = Unknown;
                break;
            }
            netState = Missing;
        }
        closedir(pd);
    }
    return netState;
}
class client_task {
public:
    client_task()
        : ctx_(1),
          client_socket_(ctx_, ZMQ_DEALER)
    {}

    void start() {
        // generate random identity
        char identity[10] = {};
        sprintf(identity, "%04X-%04X", within(0x10000), within(0x10000));
        printf("%s\n", identity);
        client_socket_.setsockopt(ZMQ_IDENTITY, identity, strlen(identity));
        client_socket_.connect("tcp://localhost:5570");

        zmq::pollitem_t items[] = {{static_cast<void*>(client_socket_), 0, ZMQ_POLLIN, 0}};
        int request_nbr = 0;
        try {
            while (true) {
                for (int i = 0; i < 100; ++i) {
                    // 10 milliseconds
                    zmq::poll(items, 1, 10);
                    if (items[0].revents & ZMQ_POLLIN) {
                        printf("\n%s ", identity);
                        s_dump(client_socket_);
                    }
                }
                char request_string[16] = {};
                sprintf(request_string, "request #%d", ++request_nbr);
                client_socket_.send(request_string, strlen(request_string));
            }
        }
        catch (std::exception &e) {}
    }

private:
    zmq::context_t ctx_;
    zmq::socket_t client_socket_;
};


void NetMonitor::run()
{
    time_t timeNow = time(0);
    time_t timeOfNextNetCheck = timeNow;
    time_t timeOfNextWdogKick = timeNow;
    time_t netLinkTimeout = 0;

    // Number of times ping is allowed to fail consecutively
    // before we terminate or reboot, i.e. third time is unlucky
    const int nConsecFailsAllowed = 2;

    int nPingFails = 0;
    Ping *singlePing = 0;
    NetLink* devNetLink = 0;
    State currentState = Up;
    State prevState = Up;
    time_t stateChangeTime = timeNow;

    try {
        singlePing = new Ping();
        devNetLink = new NetLink(netDevice_.c_str());
        devNetLink->open();
    } catch (...) {
        throw;
    }

    // A simple monitor loop which endeavours to make sure that:
    //  - watchdog is kicked every wdogKickPeriod_ seconds
    //  - gateway is pinged every networkCheckPeriod_ seconds
    //  - network interface is checked for netlink status change in the
    //    intervening periods.
    //
    while (!shouldTerminate_) {
        timeNow = time(0);

        if (timeNow >= timeOfNextWdogKick) {
#ifdef SYSTEMD_WDOG
            sdWatchdog->kick();
#endif
            timeOfNextWdogKick += wdogKickPeriod_;
        }

        if (currentState == Up) {
            if (timeNow >= timeOfNextNetCheck) {
                try {
                    singlePing->pingGateway();
                    nPingFails = 0; // all is forgiven, start over
                } catch (pingException& pe) {
                    if (++nPingFails > nConsecFailsAllowed) {
                        shouldTerminate_ = true;
                        throw pe;
                    }
                    syslog(LOG_ERR, "Ping error (%1d): %s", nPingFails, pe.what());
                } catch (exceptionLevel& el) {
                    if ( el.isFatal() || (++nPingFails > nConsecFailsAllowed) ) {
                        shouldTerminate_ = true;
                        throw el;
                    }
                    syslog(LOG_ERR, "Ping (non-fatal) exception (%1d): %s", nPingFails, el.what());
                } catch (std::exception& e) {
                    shouldTerminate_ = true;
                    throw e;
                } catch (...) {
                    shouldTerminate_ = true;
                    throw;
                }

                timeNow = time(0);
                // ping may take a while so we cannot assume that
                // less than a second has elapsed.
                timeOfNextNetCheck = timeNow + networkCheckPeriod_;
            }

        } else {
            // We haven't pinged because the network is down
            timeOfNextNetCheck = timeNow + networkCheckPeriod_;

            switch (currentState) {
            case Down:
                break;
            case DownReboot:
                break;
            case DownTerminate:
                break;
            default:
                break;
            }
        }

        netLinkTimeout =  (timeOfNextNetCheck < timeOfNextWdogKick) ? timeOfNextNetCheck : timeOfNextWdogKick;
        netLinkTimeout -= timeNow;

        syslog(LOG_DEBUG, "netLinkTimeout:%lu\n", netLinkTimeout);

        bool netLinkEvent = false;
        try {
            netLinkEvent = devNetLink->readEvent(netLinkTimeout);
        } catch (exceptionLevel& el) {
            if (el.isFatal()) {
                shouldTerminate_ = true;
                throw el;
            }
            netLinkEvent = true;
            syslog(LOG_ERR, "NetLink (non-fatal) exception (%1d): %s", nPingFails, el.what());
        } catch (...) {
            shouldTerminate_ = true;
            throw;
        }

        if (netLinkEvent) {
            currentState = devNetLink->linkState();
            if (currentState != prevState) {
                stateChangeTime = timeNow;
            }
        }

        Start,
        Up,
        NoConnection,
        Down,
        Restart,
        StillDown,
        Missing,
        NoNetDevices,
        Invalid
        switch (currentState) {
        case Up:
            switch (prevState) {
            case Up:
                break;
            case Down:
                prevState = currentState;
                break;
            case DownReboot:

            }
            break;
        case Down:
            break;
        case DownReboot:
            break;
        case DownPowerOff:
            break;
        case DownTerminate:
            break;
        default:
            break;
        }
        if (netLinkEvent && (devNetLink->linkState() == NetLink::DOWN)) {
        }
    }
}

int NetMonitor::getGCD(int a, int b)
{
    if ( (a == 0) || (b == 0) ) {
        return 1;
    }

    a = abs(a);
    b = abs(b);

    if (a == b) {
        return a;
    }

    int x;
    int y;

    if (a > b) {
        x = a;
        y = b;
    } else {
        x = b;
        y = a;
    }

    while (true) {
        x = x % y;
        if (x == 0) {
            return y;
        }

        y = y % x;
        if (y == 0) {
            return x;
        }
    }
}

