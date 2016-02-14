/*
 * netLink.cpp
 *
 * Created on: 2016-02-13
 *     Author: patw
 */

#include <asm/types.h>
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

using namespace std;

NetLink::NetLink()
{
    int rc = 0;
    int nls = open_netlink();
    printf("Started watching:\n");

    if (nls < 0) {
        printf("Open Error!");
    }

    while (rc >= 0) {
        rc = readEvent();
    }

    return 0;
}

NetLink::~NetLink()
{
    // Delete all dynamic memory.
}

int NetLink::open()
{
    int sock = socket(AF_NETLINK, SOCK_RAW, MYPROTO);
    struct sockaddr_nl addr;

    memset((void*)&addr, 0, sizeof(addr));

    if (sock < 0) {
        return sock;
    }

    addr.nl_family = AF_NETLINK;
    addr.nl_pid = getpid();
    //addr.nl_groups = RTMGRP_LINK|RTMGRP_IPV4_IFADDR|RTMGRP_IPV6_IFADDR;
    addr.nl_groups = RTMGRP_LINK | RTM_NEWLINK;

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        return -1;
    }

    return sock;
}

void NetLink::readEvent()
{
    int status;
    fd_set fdset;
    struct timeval tv;
    int ret = 0;
    char buf[4096];
    struct iovec iov = { buf, sizeof buf };
    struct sockaddr_nl snl;
    struct msghdr msg = { (void*)& snl, sizeof snl, &iov, 1, NULL, 0, 0};
    struct nlmsghdr* h;

    FD_ZERO(&fdset);
    FD_SET(_socketId, &fdset);
    tv.tv_sec = _timeout;
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
        /* Socket non-blocking so bail out once we have read everything */
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return;
        }

        /* Anything else is an error */
        string errstr = string("read_netlink: Error recvmsg: ") + string(status);
        throw exceptionLevel(errStr, true);
    }

    if(status == 0) {
        syslog(LOG_INFO, "read_netlink: EOF");
    }

    /* We need to handle more than one message per 'recvmsg' */
    for (h = (struct nlmsghdr*) buf;
          NLMSG_OK (h, (unsigned int)status);
          h = NLMSG_NEXT (h, status)) {
        /* Finish reading */
        if (h->nlmsg_type == NLMSG_DONE) {
            return;
        }

        /* Message is some kind of error */
        if (h->nlmsg_type == NLMSG_ERROR) {
            printf("read_netlink: Message is an error - decode TBD\n");
            return -1; // Error
        }

        /* Call message handler */
        ret = (*msgHandler)(&snl, h);

        if (ret < 0) {
            printf("read_netlink: Message hander error %d\n", ret);
            return ret;
        }
    }

    return ret;
}

int NetLink::linkState(struct sockaddr_nl* nl, struct nlmsghdr* msg)
{
    int len;
    struct ifinfomsg* ifi;

    nl = nl;

    ifi = (struct ifinfomsg*)NLMSG_DATA(msg);
    char ifname[1024];
    if_indextoname(ifi->ifi_index, ifname);

    printf("netlink_link_state: Link %s %s\n",
           /*(ifi->ifi_flags & IFF_RUNNING)?"Up":"Down");*/
           ifname, (ifi->ifi_flags & IFF_UP) ? "Up" : "Down");
    return 0;
}

int NetLink::msgHandler(struct sockaddr_nl* nl, struct nlmsghdr* msg)
{
    struct ifinfomsg* ifi = (struct ifinfomsg*)NLMSG_DATA(msg);
    struct ifaddrmsg* ifa = (struct ifaddrmsg*)NLMSG_DATA(msg);
    char ifname[1024];

    switch (msg->nlmsg_type) {
        case RTM_NEWADDR:
            if_indextoname(ifi->ifi_index, ifname);
            printf("msg_handler: RTM_NEWADDR : %s\n", ifname);
            break;

        case RTM_DELADDR:
            if_indextoname(ifi->ifi_index, ifname);
            printf("msg_handler: RTM_DELADDR : %s\n", ifname);
            break;

        case RTM_NEWLINK:
            if_indextoname(ifa->ifa_index, ifname);
            printf("msg_handler: RTM_NEWLINK\n");
            netlink_link_state(nl, msg);
            break;

        case RTM_DELLINK:
            if_indextoname(ifa->ifa_index, ifname);
            printf("msg_handler: RTM_DELLINK : %s\n", ifname);
            break;

        default:
            printf("msg_handler: Unknown netlink nlmsg_type %d\n",
                   msg->nlmsg_type);
            break;
    }

    return 0;
}

int main(int argc, char* argv[])


