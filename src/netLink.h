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

public:
    NetLink(int timeout) noexcept : _timeout(timeout), _socketId(-1) {}

    virtual ~NetLink();
};

#endif /* NETLINK_H */