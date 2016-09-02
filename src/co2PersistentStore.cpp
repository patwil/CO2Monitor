/*
 * co2PersistentStore.cpp
 *
 * Created on: 2016-08-31
 *     Author: patw
 */
#include <iostream>
#include <fstream>
#include <string>
#include "utils.h"

#include "co2PersistentStore.h"

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
    relHumidityWasSet_(false),
    pathName_(nullptr),
    dirName_("/var/tmp/")
{
}

Co2PersistentStore::~Co2PersistentStore()
{
    // Delete all dynamic memory.
}

void Co2PersistentStore::read()
{
    read(globals->progName());
}

void Co2PersistentStore::read(const char* progName)
{
    if (!persistentStorePathName_) {
        int pathnameLen = strlen(dirName_) + strlen(progName) + 1;
        pathName_ = new char(pathnameLen);
        if (!pathName_) {
            syslog(LOG_ERR, "%s: unable to allocate memory for persistent store file name", __FUNCTION__);
            throw exceptionLevel("memory alloc fail", true);
        }

        // can use strcpy/strcat instead of strncpy/strncat because we've allocated
        // enough space using pathNameLen
        //
        strcpy(pathName_, dirName_);
        strcat(pathName_, progName);
    }
    syslog(LOG_DEBUG, "Reading persistent store \"%s\"", persistentStorePathName_);

    co2Message::Co2PersistentStore co2Store;

    std::fstream input(persistentStorePathName_, std::ios::in | std::ios::binary);
    if (!input) {
        syslog(LOG_INFO, "%s: File not found.  Creating \"%s\"", __FUNCTION__, pathName_);
        return;
    } else if (!co2Store.ParseFromIstream(&input)) {
        syslog(LOG_ERR, "%s: unable to parse persistent store file \"%s\"", __FUNCTION__, pathName_);
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
        }

        nChars = snprintf(syslogBuf, syslogBufLen, "Restart Reason: %s, ", restartReasonStr);
        syslogBufLen -= nChars;
    }

    if (co2Store.has_timestampseconds()) {
        struct tm timeDate;
        struct tm* pTimeDate;
        lastRestartTime_ theTime = co2Store.timestampseconds();
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
        nChars = snprintf(syslogBuf, syslogBufLen, "# reboots after fail = %d, ",
                          numberOfRebootsAfterFail << std::endl;
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
        syslog(LOG_INFO, syslogBuf);
    }

    // now overwrite persistent store so there is
    // something if we crash.
    //
    this->write();
}

void Co2PersistentStore::write()
{
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

    if (pathName_) {
        syslog(LOG_DEBUG, "Writing to: \"%s\"", pathName_);
        std::fstream output(pathName_, std::ios::out | std::ios::trunc | std::ios::binary);
        if (!co2Store.SerializeToOstream(&output)) {
          syslog(LOG_ERR, "Failed to write persistent store \"%s\"", pathName_);
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
    return tv.tv_sec -
}

void Co2PersistentStore::setNumberOfRebootsAfterFail(uint32_t numberOfRebootsAfterFail)
{
}

void Co2PersistentStore::setTemperature(uint32_t temperature)
{
}

void Co2PersistentStore::setCo2(uint32_t co2)
{
}

void Co2PersistentStore::setRelHumidity(uint32_t relHumidity)
{
}


