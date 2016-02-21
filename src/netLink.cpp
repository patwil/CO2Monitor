/*
 * netLink.cpp
 *
 * Created on: 2016-02-13
 *     Author: patw
 */

#include <asm/types.h>
#include <iostream>
#include <typeinfo>
#include <exception>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#define MYPROTO NETLINK_ROUTE
#define MYMGRP RTMGRP_IPV4_ROUTE

#include "netLink.h"
#include "utils.h"

using namespace std;

NetLink::NetLink(const char* device): _socketId(-1), _linkState(UP)
{
    try {
        if (device && *device) {
            _device = string(device);
            _devIndex = if_nametoindex(device);
        } else {
            throw bad_alloc();
        }
    } catch (...) {
        throw;
    }
}

NetLink::~NetLink()
{
    // Delete all dynamic memory.
}

void NetLink::open()
{
    _socketId = socket(AF_NETLINK, SOCK_RAW, MYPROTO);
    if (_socketId < 0) {
        throw exceptionLevel("NetLink - cannot open socket", true);
    }

    struct sockaddr_nl addr;

    memset((void*)&addr, 0, sizeof(addr));

    addr.nl_family = AF_NETLINK;
    addr.nl_pid = getpid();
    //addr.nl_groups = RTMGRP_LINK|RTMGRP_IPV4_IFADDR|RTMGRP_IPV6_IFADDR;
    addr.nl_groups = RTMGRP_LINK | RTM_NEWLINK;

    if (bind(_socketId, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(_socketId);
        _socketId = -1;
        throw exceptionLevel("NetLink - cannot bind socket", true);
    }
}

void NetLink::readEvent(int timeout)
{
    int status;
    fd_set fdset;
    struct timeval tv;
    char buf[4096];
    struct iovec iov = { buf, sizeof buf };
    struct sockaddr_nl snl;
    struct msghdr msg = { (void*)& snl, sizeof snl, &iov, 1, 0, 0, 0};
    struct nlmsghdr* h;

    if (_socketId < 0) {
        throw exceptionLevel("bad socketId", true);
    }
    if (timeout < 0) {
        timeout = 0; // return immediately without waiting
    }

    FD_ZERO(&fdset);
    FD_SET(_socketId, &fdset);
    tv.tv_sec = timeout;
    tv.tv_usec = 0;

    status = select(_socketId+1, &fdset, 0, 0, &tv);
    if (status == -1) {
        throw exceptionLevel("select", true);
    } else if (status == 0) {
        // select timed out
        return;
    }

    status = recvmsg(_socketId, &msg, 0);

    if (status < 0) {
        // Socket non-blocking so bail out once we have read everything
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return;
        }

        // Anything else is an error
        string errstr = string("read_netlink: Error recvmsg: ") + to_string(status);
        throw exceptionLevel(errstr, true);
    }

    if(status == 0) {
        syslog(LOG_INFO, "read_netlink: EOF");
    }

    // We need to handle more than one message per 'recvmsg'
    for (h = (struct nlmsghdr*) buf;
          NLMSG_OK (h, (unsigned int)status);
          h = NLMSG_NEXT (h, status)) {

        // Finish reading
        if (h->nlmsg_type == NLMSG_DONE) {
            return;
        }

        // Message is some kind of error
        if (h->nlmsg_type == NLMSG_ERROR) {
            string errstr = string("read_netlink: Message is an error - decode TBD");
            throw exceptionLevel(errstr, false);
        }

        msgHandler(h);
    }
}

void NetLink::updateLinkState(struct nlmsghdr* pMsg)
{
    struct ifinfomsg* ifi;

    ifi = (struct ifinfomsg*)NLMSG_DATA(pMsg);
    char ifname[1024];
    if_indextoname(ifi->ifi_index, ifname);

    syslog(LOG_WARNING, "netlink_link_state: Link %s %s\n",
           //(ifi->ifi_flags & IFF_RUNNING)?"Up":"Down");
           ifname, (ifi->ifi_flags & IFF_UP) ? "Up" : "Down");

    if (ifi->ifi_index == _devIndex) {
        _linkState = (ifi->ifi_flags & IFF_UP) ? UP : DOWN;
    }
}

void NetLink::msgHandler(struct nlmsghdr* pMsg)
{
    struct ifinfomsg* ifi = (struct ifinfomsg*)NLMSG_DATA(pMsg);
    struct ifaddrmsg* ifa = (struct ifaddrmsg*)NLMSG_DATA(pMsg);
    char ifname[1024];

    switch (pMsg->nlmsg_type) {
        case RTM_NEWADDR:
            if_indextoname(ifi->ifi_index, ifname);
            syslog(LOG_INFO, "msg_handler: RTM_NEWADDR : %s\n", ifname);
            break;

        case RTM_DELADDR:
            if_indextoname(ifi->ifi_index, ifname);
            syslog(LOG_INFO, "msg_handler: RTM_DELADDR : %s\n", ifname);
            break;

        case RTM_NEWLINK:
            if_indextoname(ifa->ifa_index, ifname);
            syslog(LOG_INFO, "msg_handler: RTM_NEWLINK\n");
            updateLinkState(pMsg);
            break;

        case RTM_DELLINK:
            if_indextoname(ifa->ifa_index, ifname);
            syslog(LOG_INFO, "msg_handler: RTM_DELLINK : %s\n", ifname);
            break;

        default:
            syslog(LOG_ERR, "msg_handler: Unknown netlink nlmsg_type %d\n",
                   pMsg->nlmsg_type);
            break;
    }
}




