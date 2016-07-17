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
#include <unistd.h>
//#include <linux/reboot.h>
#include <sys/reboot.h>

#include "co2Message.pb.h"
#include <google/protobuf/text_format.h>

#include <zmq.hpp>

#include "netMonitor.h"
#include "config.h"
#include "parseConfigFile.h"
#include "co2Defaults.h"
#include "utils.h"

#ifdef SYSTEMD_WDOG
#include "sysdWatchdog.h"
#endif

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

void getCo2Cfg(ConfigMap& cfg, std::string& cfgStr)
{
    bool configIsOk = true;
    co2Message::Co2Message co2Msg;

    co2Message::Co2Config* co2Cfg = co2Msg.mutable_co2config();

    co2Msg.set_messagetype(co2Message::Co2Message_Co2MessageType_CO2_CFG);

    if (cfg.find("CO2Port") != cfg.end()) {
        co2Cfg->set_co2port(cfg.find("CO2Port")->second->getStr());
    } else {
        configIsOk = false;
        syslog(LOG_ERR, "Missing CO2Port config");
    }

    if (configIsOk) {
        co2Msg.SerializeToString(cfgStr);
    } else {
        throw exceptionLevel("Missing Co2Config", true);
    }
}

void getNetCfg(ConfigMap& cfg, std::string& cfgStr)
{
    bool configIsOk = true;
    co2Message::Co2Message co2Msg;

    co2Message::NetConfig* netCfg = co2Msg.mutable_netconfig();

    co2Msg.set_messagetype(co2Message::Co2Message_Co2MessageType_NET_CFG);

    if (cfg.find("NetDevice") != cfg.end()) {
        netCfg->set_netdevice(cfg.find("NetDevice")->second->getStr());
    } else {
        configIsOk = false;
        syslog(LOG_ERR, "Missing NetDevice config");
    }

    if (cfg.find("NetworkCheckPeriod") != cfg.>end()) {
        netCfg->set_networkcheckperiod(cfg.find("NetworkCheckPeriod")->second->getInt());
    } else {
        configIsOk = false;
        syslog(LOG_ERR, "Missing NetworkCheckPeriod config");
    }

    if (cfg.find("NetDeviceDownRebootMinTime") != cfg.end()) {
        netCfg->set_netdevicedownrebootmintime(cfg.find("NetDeviceDownRebootMinTime")->second->getInt());
    } else {
        configIsOk = false;
        syslog(LOG_ERR, "Missing NetDeviceDownRebootMinTime config");
    }

    if (configIsOk) {
        co2Msg.SerializeToString(cfgStr);
    } else {
        throw exceptionLevel("Missing NetConfig", true);
    }
}

void getUICfg(ConfigMap& cfg, std::string& cfgStr)
{
    co2Message::Co2Message co2Msg;

    co2Message::UIConfig* uiCfg = co2Msg.mutable_co2config();

    co2Msg.set_messagetype(co2Message::Co2Message_Co2MessageType_CO2_CFG);
    uiCfg->set_(cfg.find("NetDevice")->second->getStr());

    co2Msg.SerializeToString(cfgStr);
}

void getFanCfg(ConfigMap& cfg, std::string& cfgStr)
{
    co2Message::Co2Message co2Msg;

    co2Message::Co2Config* co2Cfg = co2Msg.mutable_co2config();

    co2Msg.set_messagetype(co2Message::Co2Message_Co2MessageType_CO2_CFG);
    co2Cfg->set_co2port(cfg.find("CO2Port")->second->getStr());

    co2Msg.SerializeToString(cfgStr);
}

void publishAllConfig(ConfigMap& cfg, zmq::socket_t& mainPubSkt)
{
    std::string cfgStr;

    // Co2Config
    getCo2Cfg(cfg, cfgStr);
    zmq::message_t configMsg(cfgStr.size());

    memcpy (configMsg.data(), msg_str.c_str(), msg_str.size());
    mainPubSkt.send(configMsg);

    // NetConfig
    getNetCfg(cfg, cfgStr);
    zmq::message_t configMsg(cfgStr.size());

    memcpy (configMsg.data(), msg_str.c_str(), msg_str.size());
    mainPubSkt.send(configMsg);

    // UIConfig
    getUICfg(cfg, cfgStr);
    zmq::message_t configMsg(cfgStr.size());

    memcpy (configMsg.data(), msg_str.c_str(), msg_str.size());
    mainPubSkt.send(configMsg);

    // FanConfig
    getFanCfg(cfg, cfgStr);
    zmq::message_t configMsg(cfgStr.size());

    memcpy (configMsg.data(), msg_str.c_str(), msg_str.size());
    mainPubSkt.send(configMsg);
}

void terminateAllThreads(zmq::socket_t& mainPubSkt)
{
    std::string pubMsgStr(kTerminateStr);
    zmq::message_t pubMsg (pubMsgStr.size());

    memcpy(pubMsg.data(), pubMsgStr.c_str(), pubMsgStr.size());
    mainPubSkt.send(pubMsg);

    // give threads some time to tidy up and terminate
    std::this_thread::sleep_for(std::chrono::seconds(5));
}

void doMainLoop(ConfigMap& cfg)
{
    NetMonitor *netMon = nullptr;
    std::thread* netMonThread;

    //std::thread* co2MonThread;
    //std::thread* displayThread;

    zmq::context_t context(1);

    zmq::socket_t mainPubSkt(context, ZMQ_PUB);
    mainPubSkt.bind(co2MainPubEndpoint);

    zmq::socket_t netMonSkt(context, ZMQ_PAIR);
    netMonSkt.bind(netMonEndpoint);

    zmq::pollitem_t rxItems [] = {
        { static_cast<void*>(netMonSkt), 0, ZMQ_POLLIN, 0 }
        //{ static_cast<void*>(rxBar), 0, ZMQ_POLLIN, 0 }
    };
    int numRxItems = sizeof(rxItems) / sizeof(rxItems[0]);

    // Allow this amount of time for all the threads to start and report that
    // they are ready.
    //
    long rxTimeoutMsec = 200;

    // start threads
    try {
        netMon = new NetMonitor();

        if (netMon) {
            netMonThread = new std::thread(std::bind(NetMonitor::run, netMon));
        } else {
            throw;
        }
    } catch (...) {
        // cleanup and exit
        terminateAllThreads(mainPubSkt);
        throw;
    }

    // Now that we have started all threads we need to wait for them to
    // report that they are ready.
    // All must be ready within the given time otherwise we have to terminate
    // everything.

    bool netMonReady = false;

    try {
        while (!(netMonReady)) {
            int nItems = zmq::poll(rxItems, numRxItems, rxTimeoutMsec);

            if (nItems == 0) {
                // timed out
                std::string s;
                if (!(netMonReady)) {
                    if (!s.empty()) {
                        s.append(", ");
                    }
                    s.append("NetMonitor");
                }

                if (!s.empty()) {
                    // at least one thread didn't start
                    s.append(" failed to start")
                    throw exceptionLevel(s, true);
                }

                // if we get here it means that all threads are ready
                break;
            }

            if (items[0].revents & ZMQ_POLLIN) {
                zmq::message_t readyMsg;
                netMonSkt.recv(&readyMsg);
                if (!strncmp(static_cast<char*>(readyMsg.data()), kReadyStr, readyMsg.size()) {
                    netMonReady = true;
                }
            }
        }
    } catch (exceptionLevel& el) {
        if (el.isFatal()) {
            terminateAllThreads(mainPubSkt);
            throw el;
        }
    } catch (...) {
        throw;
    }

    // If we get here it means that everything started up OK

    // Publish config data
    try {
        publishAllConfig(cfg, mainPubSkt);
    } catch () {
    }

    time_t wdogKickPeriod;
    if (cfg.find("WatchdogKickPeriod") != cfg.end()) {
        int tempInt = cfg.find("WatchdogKickPeriod")->second->getInt();
        wdogKickPeriod = (time_t)tempInt;
    } else {
        wdogKickPeriod = 60; // seconds
        syslog(LOG_ERR, "Missing WatchdogKickPeriod. Using a period of %u seconds.", uint(wdogKickPeriod));
    }

#ifdef SYSTEMD_WDOG
    sdWatchdog->kick();
#endif
    timeOfNextWdogKick += wdogKickPeriod;

}

int main(int argc, char* argv[])
{
    int rc = 0;
    ConfigMap cfg;

    // These configuration may be set in config file
    int logLevel;

    globals->setProgName(argv[0]);

    co2Defaults->setConfigDefaults(&cfg);
    globals->setCfg(&cfg);

    setlogmask(LOG_UPTO(co2Defaults->kLogLevelDefault));

    openlog(globals->getProgName(), LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);

    // no need for anything fancy like getopts() because
    // we only ever take a single argument pair: the config filename
    //
    if ( (argc < 3) || strncmp(argv[1], "-c", 3) ) {
        syslog (LOG_ERR, "too few arguments - usage: %s -c <config_file>", globals->getProgName());
        return -1;
    }

    // as we need root permissions for devices we need to run as root
    if (getuid() != 0) {
        syslog (LOG_ERR, "Need to run \"%s\" as root as it needs root priveleges for devices", globals->getProgName());
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
        logLevel = co2Defaults->kLogLevelDefault;
    }

    syslog(LOG_INFO, "logLevel=%d\n", logLevel);
/*
   CO2Monitor* co2Mon = new CO2Monitor(deviceCheckRetryPeriod, networkCheckPeriod);

    if (co2Mon) {
        co2Mon->loop();
    }
*/
#ifdef SYSTEMD_WDOG
    sdWatchdog->kick();
#endif

    try {
        doMainLoop(cfg);
    } catch (...) {
    }

    return 0;
}

