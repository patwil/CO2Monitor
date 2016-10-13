/*
 * restartMgr.cpp
 *
 * Created on: 2016-08-14
 *     Author: patw
 */


#include "restartMgr.h"
#include "co2PersistentStore.h"
#include "utils.h"

RestartMgr::RestartMgr() :
    restartDelay_(0)
{
    // Persistent store is managed through
    // Restart Manager
    persistentStore_ = new Co2PersistentStore;
    if (!persistentStore_) {
        throw new exceptionLevel("Unable to create Co2PersistentStore",false);
    }
}


RestartMgr::~RestartMgr()
{
}

void RestartMgr::init()
{
    // We need the persistent store to have the reason
    // this program terminated. If it crashes we won't be able
    // to write the reason, so we set it to CRASH, but overwrite this
    // if the program terminates in a controlled manner.
    //
    co2Message::Co2PersistentStore_RestartReason restartReason;

    if (persistentStore_) {
        persistentStore_->read();
    }

    restartReason = persistentStore_->restartReason();

}

void RestartMgr::stop()
{
    this->stop(0, 0, 0);
}

void RestartMgr::stop(uint32_t temperature, uint32_t co2, uint32_t relHumidity)
{
    this->restartReason_ = co2Message::Co2PersistentStore_RestartReason_STOP;
    this->doShutdown(temperature, co2, relHumidity);
}

void RestartMgr::restart()
{
    this->restart(0, 0, 0);
}

void RestartMgr::restart(uint32_t temperature, uint32_t co2, uint32_t relHumidity)
{
    this->restartReason_ = co2Message::Co2PersistentStore_RestartReason_RESTART;
    this->doShutdown(temperature, co2, relHumidity);
}

void RestartMgr::reboot(bool userReq)
{
    this->reboot(true, 0, 0, 0, userReq);
}

void RestartMgr::reboot(uint32_t temperature, uint32_t co2, uint32_t relHumidity, bool userReq)
{
    this->restartReason_ = (userReq) ? co2Message::Co2PersistentStore_RestartReason_REBOOT_USER_REQ,
                                       co2Message::Co2PersistentStore_RestartReason_REBOOT;
    this->doShutdown(temperature, co2, relHumidity);
}

void RestartMgr::shutdown()
{
    this->shutdown(0, 0, 0);
}

void RestartMgr::shutdown(uint32_t temperature, uint32_t co2, uint32_t relHumidity)
{
    this->restartReason_ = co2Message::Co2PersistentStore_RestartReason_SHUTDOWN_USER_REQ;
    this->doShutdown(temperature, co2, relHumidity);
}

void RestartMgr::doShutdown(uint32_t temperature, uint32_t co2, uint32_t relHumidity)
{
    // delay in seconds before reboot or shutdown.
    // We increase this after consecutive failures
    // up to a defined maximum.
    int delayBeforeShutdown;

    uint32_t numberOfRebootsAfterFail = persistentStore_->numberOfRebootsAfterFail();

    const char* restartReasonStr = "";
    switch (restartReason_) {
    case co2Message::Co2PersistentStore_RestartReason_STOP:
        restartReasonStr = "STOP";
        delayBeforeShutdown = 0;
        numberOfRebootsAfterFail = 0;
        break;
    case co2Message::Co2PersistentStore_RestartReason_RESTART:
        restartReasonStr = "RESTART";
        delayBeforeShutdown = numberOfRebootsAfterFail++ * 60;
        break;
    case co2Message::Co2PersistentStore_RestartReason_REBOOT_USER_REQ:
        restartReasonStr = "REBOOT_USER_REQ";
        delayBeforeShutdown = 0;
        numberOfRebootsAfterFail = 0;
        break;
    case co2Message::Co2PersistentStore_RestartReason_REBOOT:
        restartReasonStr = "REBOOT";
        delayBeforeShutdown = numberOfRebootsAfterFail++ * 120;
        break;
    case co2Message::Co2PersistentStore_RestartReason_SHUTDOWN_USER_REQ:
        restartReasonStr = "SHUTDOWN_USER_REQ";
        delayBeforeShutdown = 0;
        numberOfRebootsAfterFail = 0;
        break;
    case co2Message::Co2PersistentStore_RestartReason_UNKNOWN:
        restartReasonStr = "CRASH/UNKNOWN";
        break;
    }

    if (delayBeforeShutdown > kMaxRestartDelay) {
        delayBeforeShutdown = kMaxRestartDelay;
    }

    persistentStore_->setRestartReason(restartReason_);

    if (temperature) {
        persistentStore_->setTemperature(temperature);
    }

    if (co2) {
        persistentStore_->setCo2(co2);
    }

    if (relHumidity) {
        persistentStore_->setRelHumidity(relHumidity);
    }

    persistentStore_->setNumberOfRebootsAfterFail(numberOfRebootsAfterFail);

    syslog(LOG_INFO, "shutdown reason: \"%s\". Consecutive fails: %1u",
           restartReasonStr,
           numberOfRebootsAfterFail);

    google::protobuf::ShutdownProtobufLibrary();

    //int cmd = (bReboot) ? LINUX_REBOOT_CMD_RESTART2 : LINUX_REBOOT_CMD_POWER_OFF;



    sync();
    sync(); // to be sure
    sync(); // to be sure to be sure

    switch (this->restartType_) {
    case RestartMgr::NONE:
    case RestartMgr::STOP:
        // just exit this program
        exit(0);

    case RestartMgr::REBOOT:
        reboot(RB_AUTOBOOT);
        // a successful call to reboot() should not return, so
        // there's something amiss if we're here
        syslog(LOG_ERR, "reboot failed");
        exit(-1);

    case RestartMgr::SHUTDOWN:
        reboot(RB_POWER_OFF);
        // a successful call to reboot() should not return, so
        // there's something amiss if we're here
        syslog(LOG_ERR, "shutdown failed");
        exit(-1);
    }

    // definitely shouldn't get this far
    assert(0);
}

void RestartMgr::delayWithWdogKick(int delay)
{
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

void RestartMgr::setRestartReason(co2Message::Co2PersistentStore_RestartReason restartReason)
{
    restartReason_ = restartReason;
}

co2Message::Co2PersistentStore_RestartReason RestartMgr::restartReason()
{
    return restartReason_;
}


