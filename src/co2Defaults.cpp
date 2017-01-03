/*
 * co2Defaults.cpp
 *
 * Created on: 2016-07-09
 *     Author: patw
 */

#include "co2Defaults.h"
#include "utils.h"

#if 0
const int Co2Defaults::kNetworkCheckPeriodDefault = 60;
const int Co2Defaults::kWatchdogKickPeriod = 60;
const int Co2Defaults::kLogLevelDefault = LOG_ERR;
const char* Co2Defaults::kNetDevice = "wlan0";
const int Co2Defaults::kNetDeviceDownRebootMinTime = 5;
const int Co2Defaults::kNetDeviceDownPowerOffMinTime = 10;
const int Co2Defaults::kNetDeviceDownPowerOffMaxTime = 20;
const char* Co2Defaults::kCO2Port = "/dev/ttyUSB0";
const char* Co2Defaults::kPersistentStoreFileName = "/var/tmp/co2monitor";
const char* Co2Defaults::kSdlFbDev = "/dev/fb0";
const char* Co2Defaults::kSdlMouseDev = "/dev/input/ts";
const char* Co2Defaults::kSdlMouseDrv = "TSLIB";
const char* Co2Defaults::kSdlMouseRel = "0";
const char* Co2Defaults::kSdlVideoDriver = "fbcon";
const char* Co2Defaults::kSdlTtfDir = ".";
const char* Co2Defaults::kSdlBmpDir = ".";
const int Co2Defaults::kScreenRefreshRate = 20;
const int Co2Defaults::kScreenTimeout = 60;
const int Co2Defaults::kFanOnOverrideTime = 30;
const int Co2Defaults::kRelHumFanOnThreshold = 70;
const int Co2Defaults::kCO2FanOnThreshold = 500;
#endif

Co2Defaults::Co2Defaults() :
    kNetworkCheckPeriodDefault(60),
    kWatchdogKickPeriod(60),
#ifdef DEBUG
    kLogLevelDefault(LOG_DEBUG),
#else
    kLogLevelDefault(LOG_ERR),
#endif
    kNetDevice("wlan0"),
    kNetDeviceDownRebootMinTime(5),
    kNetDownRebootMinTime(600),
    kCO2Port("/dev/ttyUSB0"),
    kPersistentStoreFileName("/var/tmp/co2monitor"),
    kSdlFbDev("/dev/fb0"),
    kSdlMouseDev("/dev/input/ts"),
    kSdlMouseDrv("TSLIB"),
    kSdlMouseRel("0"),
    kSdlVideoDriver("fbcon"),
    kSdlTtfDir("."),
    kSdlBmpDir("."),
    kScreenRefreshRate(20),
    kScreenTimeout(60),
    kFanOnOverrideTime(30),
    kRelHumFanOnThreshold(70),
    kCO2FanOnThreshold(500)
{
}

void Co2Defaults::setConfigDefaults(ConfigMap& cfg)
{
    // Log level is one of DEBUG (verbose), INFO, NOTICE, WARNING, ERR, CRIT, ALERT (highest)
    cfg["LogLevel"] = new Config(CO2::getLogLevelStr(kLogLevelDefault));

    cfg["NetDevice"] = new Config(kNetDevice);
    cfg["NetDeviceDownRebootMinTime"] = new Config(kNetDeviceDownRebootMinTime);
    cfg["NetDownRebootMinTime"] = new Config(kNetDownRebootMinTime);

    cfg["CO2Port"] = new Config(kCO2Port);

    cfg["PersistentStoreFileName"] = new Config(kPersistentStoreFileName);

    cfg["NetworkCheckPeriod"] = new Config(kNetworkCheckPeriodDefault);
    cfg["WatchdogKickPeriod"] = new Config(kWatchdogKickPeriod);

    // Use environment vars (if set) for SDL defaults
    const char* pEnvVar;
    pEnvVar = getenv("SDL_FBDEV");
    cfg["SDL_FBDEV"] = new Config((pEnvVar) ? pEnvVar : kSdlFbDev);

    pEnvVar = getenv("SDL_MOUSEDEV");
    cfg["SDL_MOUSEDEV"] = new Config((pEnvVar) ? pEnvVar : kSdlMouseDev);

    pEnvVar = getenv("SDL_MOUSEDRV");
    cfg["SDL_MOUSEDRV"] = new Config((pEnvVar) ? pEnvVar : kSdlMouseDrv);

    pEnvVar = getenv("SDL_VIDEODRIVER");
    cfg["SDL_VIDEODRIVER"] = new Config((pEnvVar) ? pEnvVar : kSdlVideoDriver);

    pEnvVar = getenv("SDL_MOUSE_RELATIVE");
    cfg["SDL_MOUSE_RELATIVE"] = new Config((pEnvVar) ? pEnvVar : kSdlMouseRel);

    cfg["SDL_TTF_DIR"] = new Config(kSdlTtfDir);
    cfg["SDL_BMP_DIR"] = new Config(kSdlBmpDir);

    cfg["ScreenRefreshRate"] = new Config(kScreenRefreshRate);
    cfg["ScreenTimeout"] = new Config(kScreenTimeout);

    cfg["FanOnOverrideTime"] = new Config(kFanOnOverrideTime);
    cfg["RelHumFanOnThreshold"] = new Config(kRelHumFanOnThreshold);
    cfg["CO2FanOnThreshold"] = new Config(kCO2FanOnThreshold);
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

