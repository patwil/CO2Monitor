/*
 * ping.h
 *
 * Created on: 2016-02-08
 *     Author: patw
 */

#ifndef PING_H
#define PING_H

#include <iostream>
#include <typeinfo>
#include <exception>
#include <cstdlib>
#include <unistd.h>           // close()
#include <cstring>           // strcpy, memset(), and memcpy()
#include <sys/time.h>
#include <netdb.h>            // struct addrinfo
#include <sys/types.h>        // needed for socket()
#include <sys/socket.h>       // needed for socket()
#include <netinet/in.h>       // IPPROTO_RAW, IPPROTO_IP, IPPROTO_ICMP
#include <netinet/ip.h>       // struct ip and IP_MAXPACKET (which is 65535)
#include <netinet/ip_icmp.h>  // struct icmp, ICMP_ECHO
#include <arpa/inet.h>        // inet_pton() and inet_ntop()
#include <sys/ioctl.h>        // macro ioctl is defined
#include <net/if.h>           // struct ifreq
#include <asm/types.h>
#include <netinet/ether.h>
#include <net/if.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <cerrno>            // errno, perror()

using namespace std;

typedef struct {
    in_addr_t destAddr;
    in_addr_t srcAddr;
    in_addr_t gwAddr;
    uint32_t  ifIndex;
} RouteInfo_t;


class pingException: public exception
{
    string _errorStr;
public:
    pingException(const string errorStr="ping exception") noexcept :
         _errorStr(errorStr) {}

    virtual const char* what() const throw()
    {
        return _errorStr.c_str();
    }
};

class Ping
{
    int _datalen;
    uint8_t* _data;
    RouteInfo_t _rtInfo;
    uint16_t _seqNo;
    int _timeout;
    static const int _defaultDatalen = 56;
    static const int _defaultTimeout = 5; // seconds

    void getRouteInfo(RouteInfo_t* pRtInfo);
    void printRouteInfo(RouteInfo_t* pRtInfo);
    uint16_t checksum (void* addr, int len);
    uint32_t getRandom32();
    int readNlSock(int sockFd, uint8_t* bufPtr, uint32_t seqNum, uint32_t pId);
    void parseRouteInfo(struct nlmsghdr* nlHdr, RouteInfo_t* pRtInfo);
    void ping(in_addr_t destAddr, in_addr_t srcAddr, uint32_t ifIndex, uint8_t* data, uint32_t datalen, uint16_t msgSeq);

    public:
        Ping(int datalen=_defaultDatalen, int timeout=_defaultTimeout);

        ~Ping();

        void pingGateway();
};


#endif /* PING_H */
