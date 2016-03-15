/*
 * networkMonitor.cpp
 *
 *  Created on: 2015-11-22
 *      Author: patw
 */

#include <iostream>
#include <vector>
#include <unordered_map>
#include <cstdlib>
#include <cstring>
#include <syslog.h>

#include "netMonitor.h"
#include "config.h"
#include "parseConfigFile.h"
#include "utils.h"

#ifdef SYSTEMD_WDOG
#include "sysdWatchdog.h"
#endif


using namespace std;

static const int kLogLevelDefault = LOG_ERR;

void setConfigDefaults(ConfigMap& cfg)
{
    // Log level is one of DEBUG (verbose), INFO, NOTICE, WARNING, ERR, CRIT, ALERT (highest)
    cfg["LogLevel"] = new Config("ERR");
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

    // These configuration may be set in config file
    int logLevel;

    globals->setProgName(argv[0]);
    setlogmask(LOG_UPTO(kLogLevelDefault));

    openlog(globals->getProgName(), LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
    setConfigDefaults(cfg);

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

    logLevel =  getLogLevelFromStr(cfg.find("LogLevel")->second->getStr());
    if ( (logLevel >= 0) && ((logLevel & LOG_PRIMASK)== logLevel) ) {
        setlogmask(LOG_UPTO(logLevel));
    } else {
        // It doesn't matter too much if log level is bad. We can just use default.
        syslog (LOG_ERR, "invalid log level \"%s\" in config file", cfg.find("LogLevel")->second->getStr());
        logLevel = kLogLevelDefault;
    }

    syslog(LOG_INFO, "logLevel=%d\n", logLevel);
/*
   CO2Monitor* co2Mon = new CO2Monitor(deviceCheckRetryPeriod, networkCheckPeriod);

    if (co2Mon) {
        co2Mon->loop();
    }
*/
    return 0;
}

