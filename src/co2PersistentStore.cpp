/*
 * co2PersistentStore.cpp
 *
 * Created on: 2016-08-31
 *     Author: patw
 */
#include <iostream>
#include <fstream>
#include <string>
#include <fstream>
#include <cstdlib>
#include <syslog.h>
#include <sys/time.h>
#include <fmt/core.h>
#include <filesystem>
#include "utils.h"

#include "co2PersistentStore.h"
#include <google/protobuf/text_format.h>

namespace fs = std::filesystem;

Co2PersistentStore::Co2PersistentStore() :
    restartReason_(co2Message::Co2PersistentStore_RestartReason_UNKNOWN),
    restartReasonWasSet_(false),
    lastRestartTime_(0),
    numberOfRebootsAfterFail_(0),
    numberOfRebootsAfterFailWasSet_(false),
    temperature_(0),
    temperatureWasSet_(false),
    co2_(0),
    co2WasSet_(false),
    relHumidity_(0),
    relHumidityWasSet_(false)
{
}

Co2PersistentStore::~Co2PersistentStore()
{
    // Delete all dynamic memory.
}

void Co2PersistentStore::setFileName(const char *fileName)
{
    if (fileName && *fileName) {
        pathName_ = std::string(fileName);

        // Create parent directory if necessary
        fs::path filePath(fileName);
        if (!fs::exists(filePath.parent_path())) {
            if (!fs::create_directories(filePath.parent_path())) {
                throw CO2::exceptionLevel(fmt::format("{}: cannot create parent directory {} for persistent store", __FUNCTION__, filePath.parent_path().c_str()), true);
            }
        } else if (!fs::is_directory(filePath.parent_path())) {
            throw CO2::exceptionLevel(fmt::format("{}: cannot create persistent store because parent {} is not a directory", __FUNCTION__, filePath.parent_path().c_str()), true);
        }
    } else {
        throw CO2::exceptionLevel("Missing Persistent Store file name", true);
    }
}

void Co2PersistentStore::read()
{
    if (pathName_.empty()) {
        syslog(LOG_ERR, "Persistent Store filename not set. Read failed.");
        return;
    }

    syslog(LOG_DEBUG, "Reading persistent store \"%s\"", pathName_.c_str());

    co2Message::Co2PersistentStore co2Store;

    std::fstream input(pathName_, std::ios::in | std::ios::binary);
    if (!input) {
        syslog(LOG_INFO, "%s: File not found.  Creating \"%s\"", __FUNCTION__, pathName_.c_str());
        this->write();
        return;
    } else if (!co2Store.ParseFromIstream(&input)) {
        syslog(LOG_ERR, "%s: unable to parse persistent store file \"%s\"", __FUNCTION__, pathName_.c_str());
        input.close();
        this->write();
        return;
    }

    char syslogBuf[300];
    int syslogBufLen = sizeof(syslogBuf);
    int nChars = 0;
    syslogBuf[0] = 0;

    if (co2Store.has_restartreason()) {
        const char* restartReasonStr = "";

        restartReason_ = co2Store.restartreason();

        switch (restartReason_) {
        case co2Message::Co2PersistentStore_RestartReason_STOP:
            restartReasonStr = "STOP";
            break;
        case co2Message::Co2PersistentStore_RestartReason_RESTART:
            restartReasonStr = "RESTART";
            break;
        case co2Message::Co2PersistentStore_RestartReason_REBOOT_USER_REQ:
            restartReasonStr = "REBOOT_USER_REQ";
            break;
        case co2Message::Co2PersistentStore_RestartReason_REBOOT:
            restartReasonStr = "REBOOT";
            break;
        case co2Message::Co2PersistentStore_RestartReason_SHUTDOWN_USER_REQ:
            restartReasonStr = "SHUTDOWN_USER_REQ";
            break;
        case co2Message::Co2PersistentStore_RestartReason_UNKNOWN:
            restartReasonStr = "CRASH/UNKNOWN";
            break;
        // proto3 has extra enums, so default case is necessary
        default:
            restartReasonStr = "CRASH/UNKNOWN";
            break;
        }

        nChars = snprintf(syslogBuf, syslogBufLen, "Restart Reason: %s, ", restartReasonStr);
        syslogBufLen -= nChars;
    }

    if (co2Store.has_timestampseconds()) {
        struct tm timeDate;
        struct tm* pTimeDate;
        time_t theTime = co2Store.timestampseconds();
        pTimeDate = localtime_r(&theTime, &timeDate);
        if (pTimeDate) {
            nChars = snprintf(syslogBuf, syslogBufLen, "Time of last restart: %2d/%02d/%04d %2d:%02d:%02d, ",
                              timeDate.tm_mday, timeDate.tm_mon+1, timeDate.tm_year+1900,
                              timeDate.tm_hour, timeDate.tm_min, timeDate.tm_sec);
            syslogBufLen -= nChars;
        } else {
            syslog(LOG_ERR, "%s: error getting time", __FUNCTION__);
        }
    }

    if (co2Store.has_numberofrebootsafterfail()) {
        numberOfRebootsAfterFail_ = co2Store.numberofrebootsafterfail();
        nChars = snprintf(syslogBuf,
                          syslogBufLen,
                          "# reboots after fail = %d, ",
                          numberOfRebootsAfterFail_);
    }

    if (co2Store.has_temperature()) {
        temperature_ = co2Store.temperature();
        nChars = snprintf(syslogBuf, syslogBufLen, "temperature = %d, ", temperature_);
        syslogBufLen -= nChars;
    }

    if (co2Store.has_co2()) {
        co2_ = co2Store.co2();
        nChars = snprintf(syslogBuf, syslogBufLen, "CO2 = %d, ", co2_);
        syslogBufLen -= nChars;
    }

    if (co2Store.has_relhumidity()) {
        relHumidity_ = co2Store.relhumidity();
        nChars = snprintf(syslogBuf, syslogBufLen, "Rel Humidity = %d", relHumidity_);
        syslogBufLen -= nChars;
    }

    if (syslogBuf[0]) {
        syslog(LOG_INFO, "%s", syslogBuf);
    }

    input.close();

    // now overwrite persistent store so there is
    // something more up to date if we crash in this run.
    //
    this->write();
}

void Co2PersistentStore::write()
{
    if (pathName_.empty()) {
        syslog(LOG_ERR, "Persistent Store filename not set. Write failed.");
        return;
    }

    co2Message::Co2PersistentStore co2Store;

    // if restart reason hasn't been set we will know that this program
    // must have crashed, next time it starts up.
    //
    co2Store.set_restartreason((restartReasonWasSet_) ?
                               restartReason_ :
                               co2Message::Co2PersistentStore_RestartReason_CRASH);

    struct timeval tv;
    gettimeofday(&tv, NULL);
    co2Store.set_timestampseconds(tv.tv_sec);

    // if number of reboots hasn't been set we will know that the
    // program has crashed, so we should increment the number.
    //
    co2Store.set_numberofrebootsafterfail((numberOfRebootsAfterFailWasSet_) ?
                                          numberOfRebootsAfterFail_ :
                                          numberOfRebootsAfterFail_ + 1);

    // the following values are only written if they have been set
    //
    if (temperatureWasSet_) {
        co2Store.set_temperature(temperature_);
    }

    if (co2WasSet_) {
        co2Store.set_co2(co2_);
    }

    if (relHumidityWasSet_) {
        co2Store.set_relhumidity(relHumidity_);
    }

    if (!pathName_.empty()) {
        syslog(LOG_DEBUG, "Writing to: \"%s\"", pathName_.c_str());
        std::fstream output(pathName_.c_str(), std::ios::out | std::ios::trunc | std::ios::binary);
        if (!co2Store.SerializeToOstream(&output)) {
            syslog(LOG_ERR, "Failed to write persistent store \"%s\"", pathName_.c_str());
          return;
        }
    } else {
        syslog(LOG_ERR, "persistent store file name undefined");
        return;
    }
}

void Co2PersistentStore::setRestartReason(co2Message::Co2PersistentStore_RestartReason restartReason)
{
    restartReason_ = restartReason;
    restartReasonWasSet_ = true;
}

uint64_t Co2PersistentStore::secondsSinceLastRestart()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec - lastRestartTime_;
}

void Co2PersistentStore::setNumberOfRebootsAfterFail(uint32_t numberOfRebootsAfterFail)
{
    numberOfRebootsAfterFail_ = numberOfRebootsAfterFail;
    numberOfRebootsAfterFailWasSet_ = true;
}

void Co2PersistentStore::setTemperature(uint32_t temperature)
{
    temperature_ = temperature;
    temperatureWasSet_ = true;
}

void Co2PersistentStore::setCo2(uint32_t co2)
{
    co2_ = co2;
    co2WasSet_ = true;
}

void Co2PersistentStore::setRelHumidity(uint32_t relHumidity)
{
    relHumidity_ = relHumidity;
    relHumidityWasSet_ = true;
}


