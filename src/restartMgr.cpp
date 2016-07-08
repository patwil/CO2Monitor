/*
 * restartMgr.cpp
 *
 * Created on: 2016-07-07
 *     Author: patw
 */

#include "restartMgr.h"
#include "co2Message.pb.h"
#include <google/protobuf/text_format.h>

RestartMgr::RestartMgr()
{
}

{
    bool SerializeToOstream(ostream* output) const;
    bool ParseFromIstream(istream* input);
fstream input(argv[1], ios::in | ios::binary);
    if (!input) {
      cout << argv[1] << ": File not found.  Creating a new file." << endl;
    } else if (!address_book.ParseFromIstream(&input)) {
      cerr << "Failed to parse address book." << endl;
      return -1;
    }
    co2Message::Co2PersistentStore co2StoreMsg;

    if (!co2StoreMsg.ParseFromIstream(&input)) {
        std::cerr << "couldn't parse message" << std::endl;
        return;
    }
}

RestartMgr::~RestartMgr()
{
}


void RestartMgr::doShutDown(bool bReboot)
{
    //int cmd = (bReboot) ? LINUX_REBOOT_CMD_RESTART2 : LINUX_REBOOT_CMD_POWER_OFF;
    int cmd = (bReboot) ? RB_AUTOBOOT : RB_POWER_OFF;

    sync();
    sync(); // to be sure
    sync(); // to be sure to be sure

    //reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, cmd, reason);
    reboot(cmd);

    // a successful call to reboot() should not return, so
    // there's something amiss if we're here
    syslog(LOG_ERR, "reboot/shutdown failed");
    exit(errno);
}


