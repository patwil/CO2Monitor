/*
 * netLink.h
 *
 * Created on: 2016-02-13
 *     Author: patw
 */

#ifndef NETLINK_H
#define NETLINK_H

#include <iostream>

class NetLink
{
    NetLink& operator=(const NetLink& rhs);
    NetLink* operator&();
    const NetLink* operator&() const;

    int _timeout;
    int _socketId;
    std::string _device;

public:
    NetLink(int timeout, const char* device);

    virtual ~NetLink();

    void open();
    void readEvent();
    int linkState(struct sockaddr_nl* nl, struct nlmsghdr* msg);
    int msgHandler(struct sockaddr_nl* nl, struct nlmsghdr* msg);
};

#endif /* NETLINK_H */
