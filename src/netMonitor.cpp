/*
 * netMonitor.cpp
 *
 *  Created on: 2015-11-22
 *      Author: patw
 */

#include <iostream>
#include <vector>
#include <unordered_map>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "netMonitor.h"
#include "utils.h"
#ifdef SYSTEMD_WDOG
#include "sysdWatchdog.h"
#endif


using namespace std;

NetMonitor::NetMonitor()
{
}

NetMonitor::~NetMonitor()
{
}

