/*
 * co2Defaults.cpp
 *
 * Created on: 2016-07-09
 *     Author: patw
 */

#include "co2Defaults.h"
#include "utils.h"

#if 0
const int Co2Defaults::kLogLevelDefault = LOG_INFO;
const int Co2Defaults::kNetworkCheckPeriodDefault = 60;
const int Co2Defaults::kWatchdogKickPeriod = 60;
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
#ifdef DEBUG
    kLogLevelDefault(LOG_DEBUG)
#else
    kLogLevelDefault(LOG_ERR)
#endif
#if 0
    kNetworkCheckPeriodDefault(60),
    kWatchdogKickPeriod(60),
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
    kCO2FanOnThreshold(999)
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

    cfg["CO2Port"] = new Config("/dev/ttyUSB0");

    cfg["PersistentStoreFileName"] = new Config("/var/tmp/co2monitor");

    cfg["NetworkCheckPeriod"] = new Config(60);
    cfg["WatchdogKickPeriod"] = new Config(60);

    // Use environment vars (if set) for SDL defaults
    const char* pEnvVar;
    pEnvVar = getenv("SDL_FBDEV");
    cfg["SDL_FBDEV"] = new Config((pEnvVar) ? pEnvVar : "/dev/fb0");

    pEnvVar = getenv("SDL_MOUSEDEV");
    cfg["SDL_MOUSEDEV"] = new Config((pEnvVar) ? pEnvVar : "/dev/input/ts");

    pEnvVar = getenv("SDL_MOUSEDRV");
    cfg["SDL_MOUSEDRV"] = new Config((pEnvVar) ? pEnvVar : "TSLIB");

    pEnvVar = getenv("SDL_VIDEODRIVER");
    cfg["SDL_VIDEODRIVER"] = new Config((pEnvVar) ? pEnvVar : "fbcon");

    pEnvVar = getenv("SDL_MOUSE_RELATIVE");
    cfg["SDL_MOUSE_RELATIVE"] = new Config((pEnvVar) ? pEnvVar : "0");

    cfg["SDL_TTF_DIR"] = new Config(".");
    cfg["SDL_BMP_DIR"] = new Config(".");

    cfg["ScreenRefreshRate"] = new Config(20, 1, 60);
    cfg["ScreenTimeout"] = new Config(60, 10, 7200);

    cfg["FanOnOverrideTime"] = new Config(30, 1, 180);
    cfg["RelHumFanOnThreshold"] = new Config(70, 10, 90);
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

