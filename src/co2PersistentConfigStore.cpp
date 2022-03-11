/*
 * co2PersistentConfigStore.cpp
 *
 * Created on: 2022-02-23
 *     Author: patw
 */
//#include <iostream>
//#include <fstream>
//#include <iomanip>
//#include <cstdlib>
#include <syslog.h>
#include <fmt/core.h>
#include <filesystem>
#include <libconfig.h++>

#include "co2PersistentConfigStore.h"

namespace CFG = libconfig;
namespace fs = std::filesystem;

static co2Message::FanConfig_FanOverride strToFanOverride(const std::string& str)
{
    std::string lcaseStr = str;
    std::transform(lcaseStr.begin(), lcaseStr.end(), lcaseStr.begin(),
                   [](unsigned char c) -> unsigned char { return std::tolower(c); });
    if (lcaseStr == std::string("auto"))       return co2Message::FanConfig_FanOverride_AUTO;
    if (lcaseStr == std::string("manual_off")) return co2Message::FanConfig_FanOverride_MANUAL_OFF;
    if (lcaseStr == std::string("manual_on"))  return co2Message::FanConfig_FanOverride_MANUAL_ON;

    throw CO2::exceptionLevel(fmt::format("Unknown/Invalid Fan Override string: \"{}\"", str), false);
}

static const char* fanOverrideToStr(const co2Message::FanConfig_FanOverride fanoverride)
{
    switch (fanoverride) {
    case co2Message::FanConfig_FanOverride_AUTO:       return "Auto";
    case co2Message::FanConfig_FanOverride_MANUAL_OFF: return "Manual_Off";
    case co2Message::FanConfig_FanOverride_MANUAL_ON:  return "Manual_On";
    default: return nullptr;
    }
}

Co2PersistentConfigStore::Co2PersistentConfigStore() :
        relHumFanOnThreshold_(-1),
        relHumSyncNeeded_(false),
        co2FanOnThreshold_(-1),
        co2SyncNeeded_(false),
        fanOverride_(co2Message::FanConfig_FanOverride_AUTO),
        fanOverrideSyncNeeded_(false)
{
}

Co2PersistentConfigStore::~Co2PersistentConfigStore()
{
    // Delete all dynamic memory.
}

const std::string Co2PersistentConfigStore::titleSetting_       = "Title";
const std::string Co2PersistentConfigStore::titleValue_         = "Config for fan thresholds and fan state";
const std::string Co2PersistentConfigStore::fanConfigSetting_   = "FanConfig";
const std::string Co2PersistentConfigStore::relHumSetting_      = "RelHumFanOnThreshold";
const std::string Co2PersistentConfigStore::co2Setting_         = "CO2FanOnThreshold";
const std::string Co2PersistentConfigStore::fanOverrideSetting_ = "FanOverride";

void Co2PersistentConfigStore::init(const char *fileName)
{
    if (fileName && *fileName) {
        pathName_ = fileName;

        // Create parent directory if necessary
        fs::path filePath(fileName);
        if (!fs::exists(filePath.parent_path())) {
            if (!fs::create_directories(filePath.parent_path())) {
                throw CO2::exceptionLevel(fmt::format("{}: cannot create parent directory {} for persistent config", __FUNCTION__, filePath.parent_path().c_str()), true);
            }
        } else if (!fs::is_directory(filePath.parent_path())) {
            throw CO2::exceptionLevel(fmt::format("{}: cannot create persistent config because parent {} is not a directory", __FUNCTION__, filePath.parent_path().c_str()), true);
        }
    } else {
        throw CO2::exceptionLevel("Missing Persistent Store config file name", true);
    }
}

bool Co2PersistentConfigStore::hasConfig() const
{
    if (pathName_.empty()) {
        throw CO2::exceptionLevel("Persistent Store filename not set.", false);
    }

    fs::path filePath(pathName_);
    return  fs::exists(filePath) && fs::is_regular_file(filePath);
}

bool Co2PersistentConfigStore::read()
{
    if (!hasConfig()) {
        return false;
    }

    syslog(LOG_DEBUG, "Reading persistent config \"%s\"", pathName_.c_str());

    CFG::Config cfg;
    cfg.setOptions(CFG::Config::OptionFsync | CFG::Config::OptionColonAssignmentForGroups);

    try {
        cfg.readFile(pathName_);
    } catch(const CFG::FileIOException &fioex) {
        syslog(LOG_ERR, "%s: I/O error while reading config file \"%s\".", __FUNCTION__, pathName_.c_str());
        return false;
    } catch(const CFG::ParseException &pex) {
        syslog(LOG_ERR, "%s: Config file parse error at \"%s:%u\".", __FUNCTION__, pex.getFile(), pex.getLine());
        return false;
    }

    CFG::Setting& root = cfg.getRoot();

    if (!root.exists(fanConfigSetting_)) {
        return false;
    }

    CFG::Setting& fanConfig = root.lookup(fanConfigSetting_);

    int rh;
    int co2;
    std::string fan;

    // only read settings which have not changed since last save/write
    if (!relHumSyncNeeded_  && fanConfig.lookupValue(relHumSetting_, rh)) {
        relHumFanOnThreshold_ = rh;
    }
    if (!co2SyncNeeded_ && fanConfig.lookupValue(co2Setting_, co2)) {
        co2FanOnThreshold_ = co2;
    }
    if (!fanOverrideSyncNeeded_ && fanConfig.lookupValue(fanOverrideSetting_, fan)) {
        try {
            fanOverride_ = strToFanOverride(fan);
        } catch (CO2::exceptionLevel& el) {
            syslog(LOG_ERR, "%s: %s", __FUNCTION__, el.what());
        } catch (...) {
            syslog(LOG_ERR, "%s: unknown exception", __FUNCTION__);
            throw;
        }
    }
    return true;
}

void Co2PersistentConfigStore::write()
{
    // No need to save anything if nothing has changed.
    if (!(relHumSyncNeeded_ || co2SyncNeeded_ || fanOverrideSyncNeeded_)) {
        return;
    }

    CFG::Config cfg;
    cfg.setOptions(CFG::Config::OptionFsync | CFG::Config::OptionColonAssignmentForGroups);
	CFG::Setting& root = cfg.getRoot();

    root.add(titleSetting_, CFG::Setting::TypeString) = titleValue_;

    CFG::Setting& fanConfig = root.add(fanConfigSetting_, CFG::Setting::TypeGroup);

    fanConfig.add(relHumSetting_, CFG::Setting::TypeInt) = relHumFanOnThreshold_;
    fanConfig.add(co2Setting_, CFG::Setting::TypeInt) = co2FanOnThreshold_;
    fanConfig.add(fanOverrideSetting_, CFG::Setting::TypeString) = fanOverrideToStr(fanOverride_);

    // Write out the updated configuration.
    try
    {
        cfg.writeFile(pathName_);
        syslog(LOG_DEBUG, "%s: Config file written to \"%s\".", __FUNCTION__, pathName_.c_str());
        relHumSyncNeeded_ = false;
        co2SyncNeeded_ = false;
        fanOverrideSyncNeeded_ = false;
    }
    catch(const CFG::FileIOException &fioex)
    {
        syslog(LOG_ERR, "%s: I/O error while writing config file \"%s\".", __FUNCTION__, pathName_.c_str());
    }
}

void Co2PersistentConfigStore::setRelHumFanOnThreshold(int relHumFanOnThreshold)
{
    if ( (relHumFanOnThreshold > 0) && (relHumFanOnThreshold != relHumFanOnThreshold_) ) {
        relHumFanOnThreshold_ = relHumFanOnThreshold;
        relHumSyncNeeded_ = true;
    }
}

void Co2PersistentConfigStore::setCo2FanOnThreshold(int co2FanOnThreshold)
{
    if ( (co2FanOnThreshold > 0) && (co2FanOnThreshold != co2FanOnThreshold_) ) {
        co2FanOnThreshold_ = co2FanOnThreshold;
        co2SyncNeeded_ = true;
    }
}

void Co2PersistentConfigStore::setFanOverride(std::string& fanOverrideStr)
{
    setFanOverride(strToFanOverride(fanOverrideStr));
}

void Co2PersistentConfigStore::setFanOverride(co2Message::FanConfig_FanOverride fanOverride)
{
    if (fanOverride != fanOverride_) {
        fanOverride_ = fanOverride;
        fanOverrideSyncNeeded_ = true;
    }
}

