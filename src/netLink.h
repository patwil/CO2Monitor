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
public:
    typedef enum {
        DOWN,
        UP
    } LinkState;

    NetLink(const char* device);

    virtual ~NetLink();

    void open();
    void readEvent(int timeout);
    LinkState linkState() { return _linkState; }
    void updateLinkState(struct nlmsghdr* pMsg);
    void msgHandler(struct nlmsghdr* pMsg);

private:
    NetLink& operator=(const NetLink& rhs);
    NetLink* operator&();
    const NetLink* operator&() const;

    int _socketId;
    std::string _device;
    int _devIndex;
    LinkState _linkState;
};

#endif /* NETLINK_H */
