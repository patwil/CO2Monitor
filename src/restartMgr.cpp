/*
 * restartMgr.cpp
 *
 * Created on: 2016-08-14
 *     Author: patw
 */

#include <fstream>
#include <syslog.h>
#include "restartMgr.h"
#include "co2PersistentStore.h"
#include "sysdWatchdog.h"
#include <signal.h>
#include "utils.h"

RestartMgr::RestartMgr() :
    restartReason_(co2Message::Co2PersistentStore_RestartReason_UNKNOWN)
{
    // Persistent store is managed through
    // Restart Manager
    persistentStore_ = new Co2PersistentStore;
    if (!persistentStore_) {
        throw new CO2::exceptionLevel("Unable to create Co2PersistentStore",false);
    }
}


RestartMgr::~RestartMgr()
{
    if (persistentStore_) {
        delete persistentStore_;
    }
}

void RestartMgr::init(const char* filename)
{
    // We need the persistent store to have the reason
    // this program terminated. If it crashes we won't be able
    // to write the reason, so we set it to CRASH, but overwrite this
    // if the program terminates in a controlled manner.
    //

    if (persistentStore_) {
        persistentStore_->setFileName(filename);
        persistentStore_->read();
    }
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
    this->reboot(0, 0, 0, userReq);
}

void RestartMgr::reboot(uint32_t temperature, uint32_t co2, uint32_t relHumidity, bool userReq)
{
    this->restartReason_ = (userReq) ? co2Message::Co2PersistentStore_RestartReason_REBOOT_USER_REQ :
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
    uint32_t delayBeforeShutdown = 0;

    uint32_t numberOfRebootsAfterFail = persistentStore_->numberOfRebootsAfterFail();

    const char* restartReasonStr = "";
    switch (restartReason_) {
    case co2Message::Co2PersistentStore_RestartReason_STOP:
        restartReasonStr = "STOP";
        delayBeforeShutdown = 0;
        numberOfRebootsAfterFail = 0;
        break;

    case co2Message::Co2PersistentStore_RestartReason_RESTART:
        if (numberOfRebootsAfterFail <= kMaxPermittedConsecutiveRestarts) {
            restartReasonStr = "RESTART";
        } else {
            // we've exceeded allowable number of service restarts, so
            // now it's time to see if reboot will fix the problem
            //
            restartReason_ = co2Message::Co2PersistentStore_RestartReason_REBOOT;
            restartReasonStr = "REBOOT";
        }
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

    // proto3 has extra enums, so default case is necessary
    default:
        restartReasonStr = "CRASH/UNKNOWN";
        break;
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

    persistentStore_->write();

    syslog(LOG_INFO, "shutdown reason: \"%s\". Consecutive fails: %1u. Delay before shutdown: %1us",
           restartReasonStr,
           numberOfRebootsAfterFail,
           delayBeforeShutdown);

    google::protobuf::ShutdownProtobufLibrary();

    if (delayBeforeShutdown) {
        this->delayWithWdogKick(delayBeforeShutdown);
    }

    //int cmd = (bReboot) ? LINUX_REBOOT_CMD_RESTART2 : LINUX_REBOOT_CMD_POWER_OFF;

    sync();
    sync(); // to be sure
    sync(); // to be sure to be sure

    switch (restartReason_) {
    case co2Message::Co2PersistentStore_RestartReason_STOP:
    case co2Message::Co2PersistentStore_RestartReason_RESTART:
        // just exit this program
        exit(0);

    case co2Message::Co2PersistentStore_RestartReason_REBOOT_USER_REQ:
    case co2Message::Co2PersistentStore_RestartReason_REBOOT:
        waitForShutdown(true);
        // a successful call to waitForShutdown() should not return, so
        // there's something amiss if we're here
        syslog(LOG_ERR, "reboot failed");
        exit(-1);

    case co2Message::Co2PersistentStore_RestartReason_SHUTDOWN_USER_REQ:
        waitForShutdown(false);
        // a successful call to waitForShutdown() should not return, so
        // there's something amiss if we're here
        syslog(LOG_ERR, "shutdown failed");
        exit(-1);

    default:
        // shouldn't get this far
        assert(0);
    }
}

void RestartMgr::delayWithWdogKick(uint32_t delay)
{
    time_t timeToNextKick = sdWatchdog->timeUntilNextKick();

    while (delay >= static_cast<uint32_t>(timeToNextKick)) {
        sleep(timeToNextKick);

        sdWatchdog->kick();

        delay -= timeToNextKick;
        timeToNextKick = sdWatchdog->timeUntilNextKick();
    }

    // delay is now less than timeToNextKick
    if (delay) {
        sleep(delay);
    }
}

void RestartMgr::waitForShutdown(bool reboot)
{
    // We no longer need extra processing
    // for termination signals.
    //
    struct sigaction action;
    //
    action.sa_handler = exit;
    action.sa_flags = 0;
    sigemptyset(&action.sa_mask);
    sigaction(SIGHUP, &action, 0);
    sigaction(SIGINT, &action, 0);
    sigaction(SIGQUIT, &action, 0);
    sigaction(SIGTERM, &action, 0);

    // We need to spawn a process to call systemctl
    int pid = fork();

    if (pid == 0) {
        // child process
        //
        execl("/usr/bin/systemctl", "systemctl", reboot ? "reboot" : "poweroff", 0);
        //
        // shouldn't get here
        exit(0);
    }

    if (pid < 0) {
        syslog(LOG_ERR, "%s - fork returned error %d", __FUNCTION__, pid);
        exit(0);
    }

    // Now we just wait for system to
    // send termination signal, but we
    // need to keep watchdog timer happy
    // in the meantime.

    time_t timeToNextKick = sdWatchdog->timeUntilNextKick();

    while (true) {
        sleep(timeToNextKick);

        sdWatchdog->kick();

        timeToNextKick = sdWatchdog->timeUntilNextKick();
    }

}

co2Message::Co2PersistentStore_RestartReason RestartMgr::restartReason()
{
    return persistentStore_->restartReason();
}


