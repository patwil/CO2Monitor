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
    //
}


RestartMgr::~RestartMgr()
{
}

void RestartMgr::init()
{
    co2Message::Co2PersistentStore_RestartReason restartReason;
}

void RestartMgr::shutdown()
{
    write();
}

void RestartMgr::shutdown(uint32_t temperature, uint32_t co2, uint32_t relHumidity)
{
    // delay

    if (temperature || co2 || relHumidity) {
    }
    google::protobuf::ShutdownProtobufLibrary();

}

void RestartMgr::setRestartReason(co2Message::Co2PersistentStore_RestartReason restartReason)
{
    restartReason_ = restartReason;
}

co2Message::Co2PersistentStore_RestartReason RestartMgr::restartReason()
{
    return restartReason_;
}


