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

class Co2Main
{
    private:
        ConfigMap& cfg_;

        void getCo2Cfg(std::string& cfgStr);
        void getNetCfg(std::string& cfgStr);
        void getUICfg(std::string& cfgStr);
        void getFanCfg(std::string& cfgStr);
        void publishAllConfig(zmq::socket_t& mainPubSkt);
        void terminateAllThreads(zmq::socket_t& mainPubSkt);

    public:
        Co2Main(ConfigMap& cfg) : cfg_(cfg) {}
        int readConfigFile(const char* pFilename);
        void runloop();
};

int Co2Main::readConfigFile(const char* pFilename)
{
    int rc = 0;
    std::string cfgLine;
    std::string key;
    std::string val;
    std::ifstream inFile (pFilename);
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
            ConfigMapCI entry = cfg_.find(key);

            if (entry != cfg_.end()) {
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

void Co2Main::getCo2Cfg(std::string& cfgStr)
{
    bool configIsOk = true;
    co2Message::Co2Message co2Msg;

    co2Message::Co2Config* co2Cfg = co2Msg.mutable_co2config();

    co2Msg.set_messagetype(co2Message::Co2Message_Co2MessageType_CO2_CFG);

    if (cfg_.find("CO2Port") != cfg_.end()) {
        co2Cfg->set_co2port(cfg_.find("CO2Port")->second->getStr());
    } else {
        configIsOk = false;
        syslog(LOG_ERR, "Missing CO2Port config");
    }

    if (configIsOk) {
        co2Msg.SerializeToString(&cfgStr);
    } else {
        throw exceptionLevel("Missing Co2Config", true);
    }
}

void Co2Main::getNetCfg(std::string& cfgStr)
{
    bool configIsOk = true;
    co2Message::Co2Message co2Msg;

    co2Message::NetConfig* netCfg = co2Msg.mutable_netconfig();

    co2Msg.set_messagetype(co2Message::Co2Message_Co2MessageType_NET_CFG);

    if (cfg_.find("NetDevice") != cfg_.end()) {
        netCfg->set_netdevice(cfg_.find("NetDevice")->second->getStr());
    } else {
        configIsOk = false;
        syslog(LOG_ERR, "Missing NetDevice config");
    }

    if (cfg_.find("NetworkCheckPeriod") != cfg_.end()) {
        netCfg->set_networkcheckperiod(cfg_.find("NetworkCheckPeriod")->second->getInt());
    } else {
        configIsOk = false;
        syslog(LOG_ERR, "Missing NetworkCheckPeriod config");
    }

    if (cfg_.find("NetDeviceDownRebootMinTime") != cfg_.end()) {
        netCfg->set_netdevicedownrebootmintime(cfg_.find("NetDeviceDownRebootMinTime")->second->getInt());
    } else {
        configIsOk = false;
        syslog(LOG_ERR, "Missing NetDeviceDownRebootMinTime config");
    }

    if (configIsOk) {
        co2Msg.SerializeToString(&cfgStr);
    } else {
        throw exceptionLevel("Missing NetConfig", true);
    }
}

void Co2Main::getUICfg(std::string& cfgStr)
{
    bool configIsOk = true;
    co2Message::Co2Message co2Msg;

    co2Message::UIConfig* uiCfg = co2Msg.mutable_uiconfig();

    co2Msg.set_messagetype(co2Message::Co2Message_Co2MessageType_UI_CFG);

    if (cfg_.find("SDL_FBDEV") != cfg_.end()) {
        uiCfg->set_fbdev(cfg_.find("SDL_FBDEV")->second->getStr());
    } else {
        configIsOk = false;
        syslog(LOG_ERR, "Missing SDL_FBDEV config");
    }

    if (cfg_.find("SDL_MOUSEDEV") != cfg_.end()) {
        uiCfg->set_mousedev(cfg_.find("SDL_MOUSEDEV")->second->getStr());
    } else {
        configIsOk = false;
        syslog(LOG_ERR, "Missing SDL_MOUSEDEV config");
    }

    if (cfg_.find("SDL_MOUSEDRV") != cfg_.end()) {
        uiCfg->set_mousedrv(cfg_.find("SDL_MOUSEDRV")->second->getStr());
    } else {
        configIsOk = false;
        syslog(LOG_ERR, "Missing SDL_MOUSEDRV config");
    }

    if (cfg_.find("SDL_MOUSE_RELATIVE") != cfg_.end()) {
        uiCfg->set_mouserelative(cfg_.find("SDL_MOUSE_RELATIVE")->second->getStr());
    } else {
        configIsOk = false;
        syslog(LOG_ERR, "Missing SDL_MOUSE_RELATIVE config");
    }

    if (cfg_.find("SDL_VIDEODRIVER") != cfg_.end()) {
        uiCfg->set_videodriver(cfg_.find("SDL_VIDEODRIVER")->second->getStr());
    } else {
        configIsOk = false;
        syslog(LOG_ERR, "Missing SDL_VIDEODRIVER config");
    }

    if (cfg_.find("SDL_TTF_DIR") != cfg_.end()) {
        uiCfg->set_ttfdir(cfg_.find("SDL_TTF_DIR")->second->getStr());
    } else {
        configIsOk = false;
        syslog(LOG_ERR, "Missing SDL_TTF_DIR config");
    }

    if (cfg_.find("SDL_BMP_DIR") != cfg_.end()) {
        uiCfg->set_bitmapdir(cfg_.find("SDL_BMP_DIR")->second->getStr());
    } else {
        configIsOk = false;
        syslog(LOG_ERR, "Missing SDL_BMP_DIR config");
    }

    if (cfg_.find("ScreenRefreshRate") != cfg_.end()) {
        uiCfg->set_screenrefreshrate(cfg_.find("ScreenRefreshRate")->second->getInt());
    } else {
        configIsOk = false;
        syslog(LOG_ERR, "Missing ScreenRefreshRate config");
    }

    if (cfg_.find("ScreenTimeout") != cfg_.end()) {
        uiCfg->set_screentimeout(cfg_.find("ScreenTimeout")->second->getInt());
    } else {
        configIsOk = false;
        syslog(LOG_ERR, "Missing ScreenTimeout config");
    }

    if (configIsOk) {
        co2Msg.SerializeToString(&cfgStr);
    } else {
        throw exceptionLevel("Missing UIConfig", true);
    }
}

void Co2Main::getFanCfg(std::string& cfgStr)
{
    bool configIsOk = true;
    co2Message::Co2Message co2Msg;

    co2Message::FanConfig* fanCfg = co2Msg.mutable_fanconfig();

    co2Msg.set_messagetype(co2Message::Co2Message_Co2MessageType_FAN_CFG);

    if (cfg_.find("FanOnOverrideTime") != cfg_.end()) {
        fanCfg->set_fanonoverridetime(cfg_.find("FanOnOverrideTime")->second->getInt());
    } else {
        configIsOk = false;
        syslog(LOG_ERR, "Missing FanOnOverrideTime config");
    }

    if (cfg_.find("RelHumFanOnThreshold") != cfg_.end()) {
        fanCfg->set_relhumfanonthreshold(cfg_.find("RelHumFanOnThreshold")->second->getInt());
    } else {
        configIsOk = false;
        syslog(LOG_ERR, "Missing RelHumFanOnThreshold config");
    }

    if (cfg_.find("CO2FanOnThreshold") != cfg_.end()) {
        fanCfg->set_co2fanonthreshold(cfg_.find("CO2FanOnThreshold")->second->getInt());
    } else {
        configIsOk = false;
        syslog(LOG_ERR, "Missing CO2FanOnThreshold config");
    }

    co2Msg.SerializeToString(&cfgStr);
}

void Co2Main::publishAllConfig(zmq::socket_t& mainPubSkt)
{
    std::string cfgStr;

    // Co2Config
    getCo2Cfg(cfgStr);
    zmq::message_t configMsg(cfgStr.size());

    memcpy (configMsg.data(), cfgStr.c_str(), cfgStr.size());
    mainPubSkt.send(configMsg);

    // NetConfig
    getNetCfg(cfgStr);
    configMsg.rebuild(cfgStr.size());

    memcpy (configMsg.data(), cfgStr.c_str(), cfgStr.size());
    mainPubSkt.send(configMsg);

    // UIConfig
    getUICfg(cfgStr);
    configMsg.rebuild(cfgStr.size());

    memcpy (configMsg.data(), cfgStr.c_str(), cfgStr.size());
    mainPubSkt.send(configMsg);

    // FanConfig
    getFanCfg(cfgStr);
    configMsg.rebuild(cfgStr.size());

    memcpy (configMsg.data(), cfgStr.c_str(), cfgStr.size());
    mainPubSkt.send(configMsg);
}

void Co2Main::terminateAllThreads(zmq::socket_t& mainPubSkt)
{
    std::string pubMsgStr(kTerminateStr);
    zmq::message_t pubMsg (pubMsgStr.size());

    memcpy(pubMsg.data(), pubMsgStr.c_str(), pubMsgStr.size());
    mainPubSkt.send(pubMsg);

    // give threads some time to tidy up and terminate
    std::this_thread::sleep_for(std::chrono::seconds(5));
}

void Co2Main::runloop()
{
    NetMonitor* netMon = nullptr;
    std::thread* netMonThread;



    //std::thread* co2MonThread;
    //std::thread* displayThread;

    zmq::context_t context(1);
    int zSockType = ZMQ_PAIR;

    zmq::socket_t mainPubSkt(context, ZMQ_PUB);
    mainPubSkt.bind(co2MainPubEndpoint);

    zmq::socket_t netMonSkt(context, zSockType);
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
        netMon = new NetMonitor(context, zSockType);

        if (netMon) {
            netMonThread = new std::thread(std::bind(&NetMonitor::runloop, netMon));
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
                    s.append(" failed to start");
                    throw exceptionLevel(s, true);
                }

                // if we get here it means that all threads are ready
                break;
            }

            if (rxItems[0].revents & ZMQ_POLLIN) {
                zmq::message_t readyMsg;
                netMonSkt.recv(&readyMsg);

                if (!strncmp(static_cast<char*>(readyMsg.data()), kReadyStr, readyMsg.size())) {
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
        publishAllConfig(mainPubSkt);
    } catch (...) {
    }

    time_t timeNow = time(0);
    time_t wdogKickPeriod;
    time_t timeOfNextWdogKick = timeNow;

    if (cfg_.find("WatchdogKickPeriod") != cfg_.end()) {
        int tempInt = cfg_.find("WatchdogKickPeriod")->second->getInt();
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
    Co2Main co2Main(cfg);

    // These configuration may be set in config file
    int logLevel;

    globals->setProgName(argv[0]);

    co2Defaults->setConfigDefaults(cfg);
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

    rc = co2Main.readConfigFile(argv[2]);

    if (rc < 0) {
        return rc;
    }

    logLevel =  getLogLevelFromStr(cfg.find("LogLevel")->second->getStr());

    if ( (logLevel >= 0) && ((logLevel & LOG_PRIMASK) == logLevel) ) {
        setlogmask(LOG_UPTO(logLevel));
    } else {
        // It doesn't matter too much if log level is bad. We can just use default.
        syslog (LOG_ERR, "invalid log level \"%s\" in config file", cfg.find("LogLevel")->second->getStr());
        logLevel = co2Defaults->kLogLevelDefault;
    }

    syslog(LOG_INFO, "logLevel=%s\n", getLogLevelStr(logLevel));

#ifdef SYSTEMD_WDOG
    sdWatchdog->kick();
#endif

    try {
        co2Main.runloop();
    } catch (...) {
    }

    return 0;
}

