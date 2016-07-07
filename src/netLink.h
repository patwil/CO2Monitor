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
    bool readEvent(time_t timeout);
    LinkState linkState() { return linkState_; }
    void updateLinkState(struct nlmsghdr* pMsg);
    void msgHandler(struct nlmsghdr* pMsg);

private:
    NetLink& operator=(const NetLink& rhs);
    NetLink* operator&();
    const NetLink* operator&() const;

    int socketId_;
    std::string device_;
    int devIndex_;
    LinkState linkState_;
};

#endif /* NETLINK_H */
