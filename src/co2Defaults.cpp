/*
 * co2Defaults.cpp
 *
 * Created on: 2016-07-09
 *     Author: patw
 */


#include <syslog.h>
#include "co2Defaults.h"
#include "utils.h"

Co2Defaults::Co2Defaults() :
#ifdef DEBUG
    kLogLevelDefault(LOG_DEBUG)
#else
    kLogLevelDefault(LOG_ERR)
#endif
{
}

void Co2Defaults::setConfigDefaults(ConfigMap& cfg)
{
    // Log level is one of DEBUG (verbose), INFO, NOTICE, WARNING, ERR, CRIT, ALERT (highest)
    cfg["LogLevel"] = new Config(CO2::getLogLevelStr(kLogLevelDefault));

    cfg["NetDevice"] = new Config("wlan0");
    cfg["NetDeviceDownRebootMinTime"] = new Config(5);
    cfg["NetDownRebootMinTime"] = new Config(600);

    cfg["SensorType"] = new Config("sim");
    cfg["SensorPort"] = new Config("dummy");

    cfg["PersistentStoreFileName"] = new Config("/var/tmp/co2mon/state.info");
    cfg["PersistentStoreConfigFile"] = new Config("/var/tmp/co2mon/state.cfg");

    cfg["Co2LogBaseDir"] = new Config("/var/log/co2mon");

    cfg["NetworkCheckPeriod"] = new Config(60);
    cfg["WatchdogKickPeriod"] = new Config(60);

    // Use environment vars (if set) for SDL defaults
    const char* pEnvVar;
    pEnvVar = getenv("SDL_FBDEV");
    cfg["SDL_FBDEV"] = new Config((pEnvVar) ? pEnvVar : "/dev/fb1");

    pEnvVar = getenv("SDL_MOUSEDEV");
    cfg["SDL_MOUSEDEV"] = new Config((pEnvVar) ? pEnvVar : "/dev/input/ts");

    pEnvVar = getenv("SDL_MOUSEDRV");
    cfg["SDL_MOUSEDRV"] = new Config((pEnvVar) ? pEnvVar : "TSLIB");

    pEnvVar = getenv("SDL_MOUSE_RELATIVE");
    cfg["SDL_MOUSE_RELATIVE"] = new Config((pEnvVar) ? pEnvVar : "0");

    cfg["SDL_TTF_DIR"] = new Config(".");
    cfg["SDL_BMP_DIR"] = new Config(".");

    cfg["ScreenRefreshRate"] = new Config(20, 1, 60);
    cfg["ScreenTimeout"] = new Config(60, 10, 7200);

    cfg["FanOnOverrideTime"] = new Config(30, 1, 180);
    cfg["RelHumFanOnThreshold"] = new Config(70, 10, 95);
    cfg["CO2FanOnThreshold"] = new Config(999, 200, 2000);
}

void Co2Defaults::clearConfigDefaults(ConfigMap& cfg)
{
    for (auto iter = cfg.begin(); iter != cfg.end(); iter++) {
        delete iter->second;
    }
}

Co2Defaults::~Co2Defaults()
{

}

std::shared_ptr<Co2Defaults> CO2::co2Defaults = Co2Defaults::getInstance();
std::mutex Co2Defaults::mutex_;

