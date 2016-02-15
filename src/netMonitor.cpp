/*
 * netMonitor.cpp
 *
 * Created on: 2016-02-06
 *     Author: patw
 */

#include <vector>
#include <unordered_map>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <sys/time.h>

#include "netMonitor.h"
#include "ping.h"
#include "utils.h"

#ifdef SYSTEMD_WDOG
#include "sysdWatchdog.h"
#endif

using namespace std;

NetMonitor::NetMonitor() : _shouldTerminate(false)
{
    try {
        ConfigMap* pCfg = globals->getCfg();

        if (pCfg->find("NetDevice") != pCfg->end()) {
            _netDevice = string(pCfg->find("NetDevice")->second->getStr());
        }
    } catch (...) {
    }

}

NetMonitor::~NetMonitor ()
{
    // Delete all dynamic memory.
}

void NetMonitor::loop()
{
    time_t timeNow;
    time_t timeLastPing = 0;
    time_t timeLastNetCheck = 0;
    const int nConsecFailsAllowed = 2; // third time is unlucky
    int nPingFails = 0;

    Ping* singlePing = new Ping();



    while (!_shouldTerminate) {
        timeNow = time(0);

#ifdef SYSTEMD_WDOG
        sdWatchdog->kick();
#endif

        if ( (timeNow - timeLastPing) > _networkCheckPeriod ) {
            try {
                singlePing->pingGateway();
                nPingFails = 0; // all is forgiven, start over
            } catch (pingException& pe) {
                if (++nPingFails > nConsecFailsAllowed) {
                    throw pe;
                }
                syslog(LOG_ERR, "Ping error (%1d): %s", nPingFails, pe.what());
            } catch (exceptionLevel& el) {
                if ( el.isFatal() || (++nPingFails > nConsecFailsAllowed) ) {
                    throw el;
                }
                syslog(LOG_ERR, "Ping (non-fatal) exception (%1d): %s", nPingFails, el.what());
            } catch (exception& e) {
                throw e;
            }

            timeNow = time(0);
            // ping may take a while so we cannot assume that
            // less than a second has elapsed.
            timeLastPing = timeNow;
        }
        timeLastPing = time(0);

        if ( (timeNow - timeLastNetCheck) > _networkCheckPeriod ) {
            singlePing->pingGateway();
            timeLastNetCheck = time(0);
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

