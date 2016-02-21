/*
 * networkMonitor.cpp
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
#include <unistd.h>
//#include <linux/reboot.h>
#include <sys/reboot.h>

#include "netMonitor.h"
#include "config.h"
#include "parseConfigFile.h"
#include "utils.h"

#ifdef SYSTEMD_WDOG
#include "sysdWatchdog.h"
#endif


using namespace std;

void doShutDown(bool bReboot)
{
    //int cmd = (bReboot) ? LINUX_REBOOT_CMD_RESTART2 : LINUX_REBOOT_CMD_POWER_OFF;
    int cmd = (bReboot) ? RB_AUTOBOOT : RB_POWER_OFF;

    sync();
    sync(); // to be sure
    sync(); // to be sure to be sure

    //reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, cmd, reason);
    reboot(cmd);

    // a successful call to reboot() should not return, so
    // there's something amiss if we're here
    syslog(LOG_ERR, "reboot/shutdown failed");
    exit(errno);
}

void setConfigDefaults(ConfigMap& cfg)
{
    const int kNetworkCheckPeriodDefault = 60;
    const int kWatchdogKickPeriod = 60;
    const int kLogLevelDefault = LOG_ERR;
    const char* kNetDevice = "wlan0";
    const int kNetDeviceDownRebootMinTime = 5;
    const int kNetDeviceDownPowerOffMinTime = 10;
    const int kNetDeviceDownPowerOffMaxTime = 20;

    cfg["NetworkCheckPeriod"] = new Config(kNetworkCheckPeriodDefault);
    cfg["WatchdogKickPeriod"] = new Config(kWatchdogKickPeriod);
    // Log level is one of DEBUG (verbose), INFO, NOTICE, WARNING, ERR, CRIT, ALERT (highest)
    cfg["LogLevel"] = new Config(getLogLevelStr(kLogLevelDefault));

    cfg["NetDevice"] = new Config(kNetDevice);
    cfg["NetDeviceDownRebootMinTime"] = new Config(kNetDeviceDownRebootMinTime);
    cfg["NetDeviceDownPowerOffMinTime"] = new Config(kNetDeviceDownPowerOffMinTime);
    cfg["NetDeviceDownPowerOffMaxTime"] = new Config(kNetDeviceDownPowerOffMaxTime);
}

int readConfigFile(ConfigMap& cfg, const char* pFilename)
{
    int rc = 0;
    string cfgLine;
    string key;
    string val;
    ifstream inFile (pFilename);
    int lineNumber = 0;
    int errorCount = 0;

    if (inFile.fail()) {
        syslog (LOG_ERR, "unable to open config file: \"%s\"", pFilename);
        return -2;
    }

    while (inFile) {
        getline(inFile, cfgLine, '\n');
        ++lineNumber;

        rc = parseStringForKeyAndValue(cfgLine, key, val);

        if (rc > 0) {
            ConfigMapCI entry = cfg.find(key);
            if (entry != cfg.end()) {
                Config* pCfg = entry->second;

                // Set config value based on perceived type (double, integer or char string)
                if (isdigit(val[0])) {
                    if (strchr(val.c_str(), '.')) {
                        pCfg->set(stod(val, 0));
                    } else {
                        pCfg->set(stoi(val, 0, 0));
                    }
                } else {
                    pCfg->set(val);
                }
             }
        } else if (rc < 0) {
            syslog (LOG_ERR, "Error in config file \"%s\":%u", pFilename, lineNumber);
            if (++errorCount > 3) {
                syslog (LOG_ERR, "Too many errors in config file - bailing...");
                rc = -1;
                break;
            }
        }
    }

    inFile.close();

    if (rc < 0) {
        return rc;
    }

    return lineNumber;
}

int main(int argc, char* argv[])
{
    int rc = 0;
    ConfigMap cfg;

    int defaultLogLevel = LOG_ERR;
    int logLevel;

    globals->setProgName(argv[0]);
    globals->setCfg(&cfg);
    setConfigDefaults(cfg);

    // use default log level for now
    if (cfg.find("LogLevel") != cfg.end()) {
        defaultLogLevel =  getLogLevelFromStr(cfg.find("LogLevel")->second->getStr());
    }
    setlogmask(LOG_UPTO(defaultLogLevel));

    openlog(globals->getProgName(), LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);

    // no need for anything fancy like getopts() because
    // we only ever take a single argument pair: the config filename
    //
    if ( (argc < 3) || strncmp(argv[1], "-c", 3) ) {
        syslog (LOG_ERR, "too few arguments - usage: %s -c <config_file>", globals->getProgName());
        return -1;
    }

    rc = readConfigFile(cfg, argv[2]);
    if (rc < 0) {
        return rc;
    }

    if (cfg.find("LogLevel") != cfg.end()) {
        logLevel =  getLogLevelFromStr(cfg.find("LogLevel")->second->getStr());
    }
    if ( (logLevel >= 0) && ((logLevel & LOG_PRIMASK)== logLevel) ) {
        setlogmask(LOG_UPTO(logLevel));
    } else {
        // It doesn't matter too much if log level is bad. We can just use default.
        syslog (LOG_ERR, "invalid log level \"%s\" in config file", cfg.find("LogLevel")->second->getStr());
        logLevel = defaultLogLevel;
    }

    try {
        NetMonitor *netMon = new NetMonitor();

        if (netMon) {
            netMon->loop();
        }
    } catch (...) {
    }

    return 0;
}

